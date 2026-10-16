// 定义工作页 NVS 版本迁移使用的纯计算规则。
#pragma once

#include "work_page_ids.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace network_page_storage {
// V4 的最后一页是温湿历史；从 V5 起依次加入小智 AI、聚合时钟和电脑状态页。
inline constexpr size_t kLegacyV4WorkPageCount = kWorkPageHistory + 1;
// V6 的最后一页是聚合时钟，也是引入电脑状态页之前的最后一版布局。
inline constexpr size_t kLegacyV6WorkPageCount = kWorkPageAggregateClock + 1;
inline constexpr uint16_t kLegacyV4KnownPageMask =
    static_cast<uint16_t>((1U << kLegacyV4WorkPageCount) - 1U);
inline constexpr uint16_t kCurrentKnownPageMask =
    static_cast<uint16_t>((1U << kWorkPageCount) - 1U);
// V6 及更早版本都不含电脑状态页，迁移时补上该页的默认启用位。
inline constexpr uint16_t kNewerWorkPageMigrationBits =
    static_cast<uint16_t>(1U << kWorkPagePcStatus);

constexpr uint16_t migrate_v4_page_mask(uint16_t legacy_mask)
{
    return static_cast<uint16_t>(legacy_mask | (1U << kWorkPageXiaozhiAI));
}

inline bool migrate_v4_page_order(const uint8_t *legacy_order,
                                  size_t legacy_order_size,
                                  uint8_t *current_order,
                                  size_t current_order_size)
{
    if (!legacy_order || legacy_order_size != kLegacyV4WorkPageCount ||
        !current_order || current_order_size != kWorkPageCount) {
        return false;
    }
    memcpy(current_order, legacy_order, legacy_order_size);
    current_order[kWorkPageXiaozhiAI] = kWorkPageXiaozhiAI;
    current_order[kWorkPageAggregateClock] = kWorkPageAggregateClock;
    current_order[kWorkPagePcStatus] = kWorkPagePcStatus;
    return true;
}

static_assert(kWorkPageCount <= 16, "work page enabled mask is stored as uint16_t");
static_assert(kWorkPageXiaozhiAI == static_cast<int>(kLegacyV4WorkPageCount),
              "v5 migration expects Xiaozhi AI after every v4 page");
static_assert(kWorkPageAggregateClock == static_cast<int>(kLegacyV6WorkPageCount),
              "v6 migration expects aggregate clock to be the final v6 page");
static_assert(kWorkPageCount == kWorkPagePcStatus + 1,
              "v7 migration expects the PC status page to be the final page");
static_assert((kLegacyV4KnownPageMask & (1U << kWorkPageXiaozhiAI)) == 0,
              "v4 mask must not contain the Xiaozhi AI page");
} // namespace network_page_storage
