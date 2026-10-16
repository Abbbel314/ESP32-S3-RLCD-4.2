// 通过 USB 串口持续接收电脑状态行并更新快照。
//
// 板载 CH343 桥接芯片接在 ESP32-S3 的 UART0（GPIO43/44）上，与烧录和日志共用
// 同一物理串口。日志只用 TX，本模块只用 RX，两者方向独立互不干扰。
// 固件没有使用 esp_console，因此 UART0 的接收通道是空闲的。
#include "pc_status_uart.h"

#include "app_metadata.h"
#include "pc_status_state.h"

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
constexpr uart_port_t kPcStatusUartPort = UART_NUM_0;
constexpr int kPcStatusUartBaud = 115200;
constexpr int kPcStatusUartRxBuffer = 2048;
constexpr size_t kPcStatusReadChunk = 128;
constexpr int kPcStatusReadTimeoutMs = 200;
constexpr uint32_t kPcStatusTaskStackSize = 4096;
constexpr UBaseType_t kPcStatusTaskPriority = 3;
constexpr const char *kPcStatusTaskName = "pc_status_uart";
constexpr const char *kPcStatusLinePrefix = "PC ";
constexpr size_t kPcStatusLinePrefixLen = 3;
constexpr size_t kPcStatusLineMaxLen = 192;
constexpr size_t kPcStatusKeyMaxLen = 16;
constexpr size_t kPcStatusValueMaxLen = 32;
constexpr const char *kPcStatusUartInitFailedLog = "pc status uart init failed";
constexpr const char *kPcStatusTaskCreateFailedLog = "pc status uart task create failed";

std::atomic<bool> s_started{false};
char s_line_buffer[kPcStatusLineMaxLen];
size_t s_line_length = 0;

static_assert(kPcStatusLineMaxLen > 32, "pc status line buffer must hold a valid frame");
static_assert(kPcStatusLinePrefixLen == 3, "pc status frame prefix must stay three bytes");

void apply_field(PcStatusSnapshot *snapshot, const char *key, const char *value)
{
    if (strcmp(key, "host") == 0) {
        strncpy(snapshot->host, value, sizeof(snapshot->host) - 1);
        snapshot->host[sizeof(snapshot->host) - 1] = '\0';
    } else if (strcmp(key, "cpu") == 0) {
        snapshot->cpu_percent = strtof(value, nullptr);
    } else if (strcmp(key, "freq") == 0) {
        snapshot->cpu_freq_mhz = atoi(value);
    } else if (strcmp(key, "temp") == 0) {
        snapshot->cpu_temp_c = atoi(value);
    } else if (strcmp(key, "mem") == 0) {
        snapshot->mem_percent = strtof(value, nullptr);
    } else if (strcmp(key, "memu") == 0) {
        snapshot->mem_used_mb = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    } else if (strcmp(key, "memt") == 0) {
        snapshot->mem_total_mb = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    } else if (strcmp(key, "disk") == 0) {
        snapshot->disk_percent = strtof(value, nullptr);
    } else if (strcmp(key, "up") == 0) {
        snapshot->net_up_kbps = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    } else if (strcmp(key, "down") == 0) {
        snapshot->net_down_kbps = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    } else if (strcmp(key, "uptime") == 0) {
        snapshot->uptime_sec = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    }
}

void handle_frame(const char *line)
{
    if (strncmp(line, kPcStatusLinePrefix, kPcStatusLinePrefixLen) != 0) {
        return;
    }
    PcStatusSnapshot snapshot;
    snapshot.valid = true;
    snapshot.received_us = esp_timer_get_time();

    const char *cursor = line + kPcStatusLinePrefixLen;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *equals = strchr(cursor, '=');
        if (!equals) {
            break;
        }
        const char *value_begin = equals + 1;
        const char *value_end = strchr(value_begin, ' ');
        const size_t key_len = static_cast<size_t>(equals - cursor);
        const size_t value_len =
            value_end ? static_cast<size_t>(value_end - value_begin) : strlen(value_begin);

        char key[kPcStatusKeyMaxLen] = {};
        char value[kPcStatusValueMaxLen] = {};
        if (key_len < sizeof(key) && value_len < sizeof(value)) {
            memcpy(key, cursor, key_len);
            memcpy(value, value_begin, value_len);
            apply_field(&snapshot, key, value);
        }
        cursor = value_end ? value_end : value_begin + value_len;
    }
    pc_status_snapshot_store(snapshot);
}

void consume_bytes(const uint8_t *data, int length)
{
    for (int i = 0; i < length; ++i) {
        const char byte = static_cast<char>(data[i]);
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            if (s_line_length > 0) {
                s_line_buffer[s_line_length] = '\0';
                handle_frame(s_line_buffer);
            }
            s_line_length = 0;
            continue;
        }
        if (s_line_length + 1 < sizeof(s_line_buffer)) {
            s_line_buffer[s_line_length++] = byte;
        } else {
            // 超长帧直接丢弃，等待下一个换行重新同步。
            s_line_length = 0;
        }
    }
}

void pc_status_uart_task(void *)
{
    uint8_t chunk[kPcStatusReadChunk];
    for (;;) {
        const int read =
            uart_read_bytes(kPcStatusUartPort, chunk, sizeof(chunk),
                            pdMS_TO_TICKS(kPcStatusReadTimeoutMs));
        if (read > 0) {
            consume_bytes(chunk, read);
        }
    }
}
} // namespace

void pc_status_uart_start()
{
    bool expected = false;
    if (!s_started.compare_exchange_strong(expected, true)) {
        return;
    }
    if (!pc_status_state_init()) {
        s_started.store(false);
        ESP_LOGW(TAG, "%s", kPcStatusUartInitFailedLog);
        return;
    }
    if (!uart_is_driver_installed(kPcStatusUartPort)) {
        uart_config_t config = {};
        config.baud_rate = kPcStatusUartBaud;
        config.data_bits = UART_DATA_8_BITS;
        config.parity = UART_PARITY_DISABLE;
        config.stop_bits = UART_STOP_BITS_1;
        config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        config.source_clk = UART_SCLK_DEFAULT;
        if (uart_driver_install(kPcStatusUartPort,
                                kPcStatusUartRxBuffer,
                                0,
                                0,
                                nullptr,
                                0) != ESP_OK) {
            ESP_LOGW(TAG, "%s", kPcStatusUartInitFailedLog);
            return;
        }
        uart_param_config(kPcStatusUartPort, &config);
        uart_set_pin(kPcStatusUartPort,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
    }
    if (xTaskCreate(pc_status_uart_task,
                    kPcStatusTaskName,
                    kPcStatusTaskStackSize,
                    nullptr,
                    kPcStatusTaskPriority,
                    nullptr) != pdPASS) {
        ESP_LOGW(TAG, "%s", kPcStatusTaskCreateFailedLog);
    }
}
