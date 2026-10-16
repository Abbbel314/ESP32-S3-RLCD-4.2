// 集中保存电脑状态快照，避免串口接收任务与 UI 任务之间的数据竞争。
#include "pc_status_state.h"

#include "scoped_semaphore_lock.h"

namespace {
StaticTaskMutex s_pc_status_mutex;
PcStatusSnapshot s_pc_status_snapshot;
uint32_t s_pc_status_version = 0;
bool s_pc_status_initialized = false;
} // namespace

bool pc_status_state_init()
{
    if (s_pc_status_initialized) {
        return true;
    }
    if (!s_pc_status_mutex.init()) {
        return false;
    }
    s_pc_status_initialized = true;
    return true;
}

void pc_status_snapshot_store(const PcStatusSnapshot &snapshot)
{
    ScopedSemaphoreLock lock(s_pc_status_mutex.handle());
    if (!lock) {
        return;
    }
    ++s_pc_status_version;
    s_pc_status_snapshot = snapshot;
    s_pc_status_snapshot.version = s_pc_status_version;
}

bool pc_status_snapshot_load(PcStatusSnapshot *snapshot)
{
    if (!snapshot) {
        return false;
    }
    ScopedSemaphoreLock lock(s_pc_status_mutex.handle());
    if (!lock) {
        return false;
    }
    *snapshot = s_pc_status_snapshot;
    return true;
}

uint32_t pc_status_version_load()
{
    ScopedSemaphoreLock lock(s_pc_status_mutex.handle());
    if (!lock) {
        return 0;
    }
    return s_pc_status_version;
}
