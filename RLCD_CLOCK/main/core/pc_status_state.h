// 声明电脑状态快照的线程安全读写接口。
#pragma once

#include <stddef.h>
#include <stdint.h>

inline constexpr size_t kPcStatusHostMaxLen = 24;

struct PcStatusSnapshot {
    bool valid = false;
    uint32_t version = 0;
    char host[kPcStatusHostMaxLen] = {};
    float cpu_percent = 0.0f;
    int cpu_freq_mhz = 0;
    int cpu_temp_c = -1;
    float mem_percent = 0.0f;
    uint32_t mem_used_mb = 0;
    uint32_t mem_total_mb = 0;
    float disk_percent = -1.0f;
    uint32_t net_up_kbps = 0;
    uint32_t net_down_kbps = 0;
    uint32_t uptime_sec = 0;
    int64_t received_us = 0;
};

// 首次使用前必须成功调用一次；重复调用返回既有初始化结果。
bool pc_status_state_init();
void pc_status_snapshot_store(const PcStatusSnapshot &snapshot);
// 读取成功才改写输出；互斥读取失败时保留调用方已有快照。
bool pc_status_snapshot_load(PcStatusSnapshot *snapshot);
uint32_t pc_status_version_load();
