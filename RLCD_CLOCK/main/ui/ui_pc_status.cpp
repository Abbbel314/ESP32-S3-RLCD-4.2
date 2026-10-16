// 构建并刷新电脑状态工作页，展示局域网采集服务上报的电脑运行状态。
#include "ui_work_pages.h"

#include "ui_page_state.h"
#include "ui_widgets.h"
#include "work_page_ids.h"

#include <esp_attr.h>

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
constexpr const char *kPcTitle = "电脑状态";
constexpr const char *kPcRowPlaceholder = "--";
constexpr const char *kPcHint = "Hold KEY to return";
constexpr const char *kPcTitleCreateFailedLog = "pc status title create failed";
constexpr const char *kPcHintCreateFailedLog = "pc status hint create failed";
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
    bool changed = false;
    changed |= set_label_text_if_changed(s_pc_row_labels[0], kPcRowPlaceholder);
    return changed;
}

void clear_pc_status_object_refs()
{
    for (lv_obj_t *&label : s_pc_row_labels) {
        label = nullptr;
    }
}
