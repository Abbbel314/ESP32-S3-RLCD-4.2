// 负责工作页开关、顺序和上一版页面数据的 NVS 读写。
#include "network_page_storage.h"

#include "network_page_storage_policy.h"
#include "ui_work_page_catalog.h"

#include <string.h>

namespace network_page_storage {
namespace {
bool saved_page_order_matches(nvs_handle_t nvs,
                              const uint8_t *page_order,
                              size_t page_order_size)
{
    uint8_t saved_order[kWorkPageCount] = {};
    size_t stored_len = sizeof(saved_order);
    return page_order &&
           page_order_size == sizeof(saved_order) &&
           nvs_get_blob(nvs,
                        kPageOrderV7Key,
                        saved_order,
                        &stored_len) == ESP_OK &&
           stored_len == sizeof(saved_order) &&
           memcmp(saved_order, page_order, page_order_size) == 0;
}
} // namespace

uint16_t read_saved_page_mask(nvs_handle_t nvs)
{
    uint16_t page_mask = kCurrentKnownPageMask;
    if (nvs_get_u16(nvs, kPageMaskV7Key, &page_mask) == ESP_OK) {
        return normalize_work_page_enabled_mask(page_mask);
    }
    uint8_t legacy_mask = 0;
    if (nvs_get_u8(nvs, kPageMaskV6Key, &legacy_mask) == ESP_OK) {
        return normalize_work_page_enabled_mask(
            static_cast<uint16_t>(legacy_mask) | kNewerWorkPageMigrationBits);
    }
    if (nvs_get_u8(nvs, kPageMaskV5Key, &legacy_mask) == ESP_OK) {
        return normalize_work_page_enabled_mask(
            static_cast<uint16_t>(legacy_mask) |
            static_cast<uint16_t>(1U << kWorkPageAggregateClock) |
            kNewerWorkPageMigrationBits);
    }
    if (nvs_get_u8(nvs, kPageMaskV4Key, &legacy_mask) == ESP_OK) {
        return normalize_work_page_enabled_mask(
            migrate_v4_page_mask(legacy_mask) |
            static_cast<uint16_t>(1U << kWorkPageAggregateClock) |
            kNewerWorkPageMigrationBits);
    }
    return page_mask;
}

bool read_saved_page_order(nvs_handle_t nvs, uint8_t *page_order, size_t page_order_size)
{
    if (!page_order || page_order_size != kWorkPageCount) {
        return false;
    }
    size_t stored_len = page_order_size;
    if (nvs_get_blob(nvs, kPageOrderV7Key, page_order, &stored_len) == ESP_OK &&
        stored_len == page_order_size) {
        return true;
    }
    stored_len = kLegacyV6WorkPageCount;
    if (nvs_get_blob(nvs, kPageOrderV6Key, page_order, &stored_len) == ESP_OK &&
        stored_len == kLegacyV6WorkPageCount) {
        page_order[kWorkPageAggregateClock] = kWorkPageAggregateClock;
        page_order[kWorkPagePcStatus] = kWorkPagePcStatus;
        return true;
    }
    stored_len = kWorkPageAggregateClock;
    if (nvs_get_blob(nvs, kPageOrderV5Key, page_order, &stored_len) == ESP_OK &&
        stored_len == kWorkPageAggregateClock) {
        page_order[kWorkPageAggregateClock] = kWorkPageAggregateClock;
        page_order[kWorkPagePcStatus] = kWorkPagePcStatus;
        return true;
    }
    uint8_t legacy_order[kLegacyV4WorkPageCount] = {};
    stored_len = sizeof(legacy_order);
    return nvs_get_blob(nvs, kPageOrderV4Key, legacy_order, &stored_len) == ESP_OK &&
           stored_len == sizeof(legacy_order) &&
           migrate_v4_page_order(legacy_order,
                                 sizeof(legacy_order),
                                 page_order,
                                 page_order_size);
}

esp_err_t write_work_page_mask_nvs(nvs_handle_t nvs,
                                   esp_err_t err,
                                   uint16_t page_mask,
                                   bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    uint16_t saved_mask = 0;
    if (nvs_get_u16(nvs, kPageMaskV7Key, &saved_mask) == ESP_OK &&
        saved_mask == page_mask) {
        return ESP_OK;
    }
    esp_err_t write_err = nvs_set_u16(nvs, kPageMaskV7Key, page_mask);
    if (write_err == ESP_OK && changed) {
        *changed = true;
    }
    return write_err;
}

esp_err_t write_work_page_order_nvs(nvs_handle_t nvs,
                                    esp_err_t err,
                                    const uint8_t *page_order,
                                    size_t page_order_size,
                                    bool *changed)
{
    if (changed) {
        *changed = false;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (saved_page_order_matches(nvs, page_order, page_order_size)) {
        return ESP_OK;
    }
    esp_err_t write_err = nvs_set_blob(nvs, kPageOrderV7Key, page_order, page_order_size);
    if (write_err == ESP_OK && changed) {
        *changed = true;
    }
    return write_err;
}
} // namespace network_page_storage
