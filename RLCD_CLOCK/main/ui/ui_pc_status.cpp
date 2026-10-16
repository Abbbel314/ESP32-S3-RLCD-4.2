// 构建并刷新电脑状态工作页，展示串口接收到的电脑运行状态。
#include "ui_work_pages.h"

#include "pc_status_state.h"
#include "pc_status_uart.h"
#include "ui_page_state.h"
#include "ui_widgets.h"
#include "work_page_ids.h"

#include <esp_attr.h>
#include <esp_timer.h>

#include <stdio.h>

namespace {
constexpr int kPcTitleX = 24;
constexpr int kPcTitleY = 18;
constexpr int kPcTitleW = 352;
constexpr int kPcTitleH = 28;
constexpr int kPcFrameX = 24;
constexpr int kPcTopLineY = 52;
constexpr int kPcFrameW = 352;
constexpr int kPcTopLineH = 3;
constexpr int kPcRowX = 30;
constexpr int kPcRowW = 340;
constexpr int kPcRowH = 26;
constexpr int kPcRowGap = 38;
constexpr int kPcFirstRowY = 72;
constexpr int kPcBottomLineY = 266;
constexpr int kPcBottomLineH = 2;
constexpr size_t kPcRowCount = 5;
constexpr size_t kPcLineTextSize = 64;
constexpr int64_t kPcStatusLinkTimeoutUs = 5000000; // 5 秒未收到数据视为断线
constexpr const char *kPcTitle = "电脑状态";
constexpr const char *kPcRowPlaceholder = "--";
constexpr const char *kPcWaitingText = "WAITING FOR PC DATA";
constexpr const char *kPcHint = "Hold KEY to return";
constexpr const char *kPcTitleCreateFailedLog = "pc status title create failed";
constexpr const char *kPcHintCreateFailedLog = "pc status hint create failed";
constexpr const char *kPcOfflineSuffix = "OFFLINE";
constexpr const char *kPcOnlineSuffix = "LINK OK";
EXT_RAM_BSS_ATTR lv_obj_t *s_pc_row_labels[kPcRowCount];

static_assert(kPcRowCount > 0, "PC status page must have at least one data row");
static_assert(kPcRowGap > kPcRowH, "PC status rows must not overlap");
static_assert(kPcFirstRowY + static_cast<int>(kPcRowCount - 1) * kPcRowGap + kPcRowH <=
                  kPcBottomLineY,
              "PC status rows must fit above the bottom frame");
} // namespace

void build_pc_status_page()
{
    if (work_page_root(kWorkPagePcStatus)) {
        return;
    }
    lv_obj_t *screen = create_page_root();
    if (!screen) {
        return;
    }
    set_work_page_root(kWorkPagePcStatus, screen);

    make_centered_label(screen,
                        kPcTitleX,
                        kPcTitleY,
                        kPcTitleW,
                        kPcTitleH,
                        kPcTitle,
                        kPcTitleCreateFailedLog);
    make_black_bar(screen, kPcFrameX, kPcTopLineY, kPcFrameW, kPcTopLineH);
    for (size_t i = 0; i < kPcRowCount; ++i) {
        s_pc_row_labels[i] = make_label(screen,
                                        kPcRowX,
                                        kPcFirstRowY + static_cast<int>(i) * kPcRowGap,
                                        kPcRowW,
                                        kPcRowH,
                                        kPcRowPlaceholder);
    }
    make_black_bar(screen, kPcFrameX, kPcBottomLineY, kPcFrameW, kPcBottomLineH);
    make_centered_label(screen,
                        kPcTitleX,
                        kPcBottomLineY + 6,
                        kPcTitleW,
                        20,
                        kPcHint,
                        kPcHintCreateFailedLog);

    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
}

bool update_pc_status_page(const struct tm &local)
{
    (void)local;
    pc_status_uart_start();

    bool changed = false;
    PcStatusSnapshot pc;
    if (!pc_status_snapshot_load(&pc) || !pc.valid) {
        changed |= set_label_text_if_changed(s_pc_row_labels[0], kPcWaitingText);
        for (size_t i = 1; i < kPcRowCount; ++i) {
            changed |= set_label_text_if_changed(s_pc_row_labels[i], kPcRowPlaceholder);
        }
        return changed;
    }

    const bool link_ok =
        (esp_timer_get_time() - pc.received_us) < kPcStatusLinkTimeoutUs;
    char line[kPcLineTextSize] = {};

    snprintf(line, sizeof(line), "%s  %s", pc.host[0] ? pc.host : "PC",
             link_ok ? kPcOnlineSuffix : kPcOfflineSuffix);
    changed |= set_label_text_if_changed(s_pc_row_labels[0], line);

    if (pc.cpu_temp_c >= 0) {
        snprintf(line, sizeof(line), "CPU %.0f%%  %dMHz  %dC",
                 pc.cpu_percent, pc.cpu_freq_mhz, pc.cpu_temp_c);
    } else {
        snprintf(line, sizeof(line), "CPU %.0f%%  %dMHz", pc.cpu_percent, pc.cpu_freq_mhz);
    }
    changed |= set_label_text_if_changed(s_pc_row_labels[1], line);

    if (pc.mem_total_mb > 0) {
        snprintf(line, sizeof(line), "MEM %.0f%%  %.1f/%.1fGB",
                 pc.mem_percent,
                 static_cast<double>(pc.mem_used_mb) / 1024.0,
                 static_cast<double>(pc.mem_total_mb) / 1024.0);
    } else {
        snprintf(line, sizeof(line), "MEM %.0f%%", pc.mem_percent);
    }
    changed |= set_label_text_if_changed(s_pc_row_labels[2], line);

    if (pc.disk_percent >= 0.0f) {
        snprintf(line, sizeof(line), "DISK C: %.0f%%", pc.disk_percent);
    } else {
        snprintf(line, sizeof(line), "DISK %s", kPcRowPlaceholder);
    }
    changed |= set_label_text_if_changed(s_pc_row_labels[3], line);

    snprintf(line, sizeof(line), "NET U%u D%u KB/s", pc.net_up_kbps, pc.net_down_kbps);
    changed |= set_label_text_if_changed(s_pc_row_labels[4], line);

    return changed;
}

void clear_pc_status_object_refs()
{
    for (lv_obj_t *&label : s_pc_row_labels) {
        label = nullptr;
    }
}
