#pragma once
#include "driver/driver_base.h"
#include "driver/driver_manager.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <dirent.h>

// ============================================================
// 全局驱动实例 & 便捷内存读写封装
// ============================================================

static driver_base* g_driver = nullptr;
static pid_t g_game_pid = 0;
inline unsigned long long g_mem_read_total = 0;
inline unsigned long long g_mem_read_fail = 0;
inline uintptr_t g_mem_last_fail_addr = 0;

// 初始化驱动 (自动检测)
inline bool init_driver() {
    g_driver = DriverManager::auto_detect();
    return g_driver != nullptr;
}

// 通过包名获取目标进程PID
inline pid_t get_game_pid(const char* pkg) {
    DIR* dir = opendir("/proc");
    if (!dir) return 0;

    struct dirent* entry;
    char path[256], line[256];

    while ((entry = readdir(dir)) != nullptr) {
        // 跳过非数字目录
        bool isNum = true;
        for (int i = 0; entry->d_name[i]; i++) {
            if (entry->d_name[i] < '0' || entry->d_name[i] > '9') {
                isNum = false;
                break;
            }
        }
        if (!isNum) continue;

        snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
        FILE* f = fopen(path, "r");
        if (f) {
            if (fgets(line, sizeof(line), f)) {
                if (strstr(line, pkg)) {
                    pid_t pid = atoi(entry->d_name);
                    fclose(f);
                    closedir(dir);

                    g_game_pid = pid;
                    if (g_driver) {
                        g_driver->set_pid(pid);
                    }
                    return pid;
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    return 0;
}

// 获取模块基址 (优先驱动方式, 回退 /proc/pid/maps)
inline uintptr_t get_module_base(const char* name) {
    // 先尝试驱动方式
    if (g_driver) {
        uintptr_t base = g_driver->get_module_base(name);
        if (base != 0) return base;
    }

    // 回退: 解析 /proc/<pid>/maps (找第一个匹配行 = text段基址)
    if (g_game_pid <= 0) return 0;

    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", g_game_pid);
    FILE* f = fopen(maps_path, "r");
    if (!f) return 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            uintptr_t base = 0;
            sscanf(line, "%lx-", &base);
            fclose(f);
            return base;
        }
    }
    fclose(f);
    return 0;
}

// 获取模块的BSS段基址 (找最后一个 rw-p 映射)
// BSS在ELF内存映射中通常是最后一个可读写区域
inline uintptr_t get_bss_base(const char* module_name) {
    if (g_game_pid <= 0) return 0;

    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", g_game_pid);
    FILE* f = fopen(maps_path, "r");
    if (!f) return 0;

    char line[512];
    uintptr_t lastRW = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, module_name) && strstr(line, "rw-")) {
            uintptr_t addr = 0;
            sscanf(line, "%lx-", &addr);
            lastRW = addr;
        }
    }
    fclose(f);
    return lastRW;
}

// ============================================================
// 便捷读写函数 (兼容旧接口)
// ============================================================

inline long read_long(uintptr_t addr) {
    if (!g_driver) return 0;
    long val = 0;
    bool ok = g_driver->read(addr, &val, sizeof(val));
    g_mem_read_total++;
    if (!ok) {
        g_mem_read_fail++;
        g_mem_last_fail_addr = addr;
    }
    return val & 0xFFFFFFFFFFFF; // 48位地址掩码
}

inline int read_int(uintptr_t addr) {
    if (!g_driver) return 0;
    int val = 0;
    bool ok = g_driver->read(addr, &val, sizeof(val));
    g_mem_read_total++;
    if (!ok) {
        g_mem_read_fail++;
        g_mem_last_fail_addr = addr;
    }
    return val;
}

inline float read_float(uintptr_t addr) {
    if (!g_driver) return 0.0f;
    float val = 0.0f;
    bool ok = g_driver->read(addr, &val, sizeof(val));
    g_mem_read_total++;
    if (!ok) {
        g_mem_read_fail++;
        g_mem_last_fail_addr = addr;
    }
    return val;
}

inline void write_int(uintptr_t addr, int val) {
    if (g_driver) g_driver->write(addr, &val, sizeof(val));
}

inline void write_float(uintptr_t addr, float val) {
    if (g_driver) g_driver->write(addr, &val, sizeof(val));
}

// 模板读取 (链式读取专用)
template<typename T>
inline T mem_read(uintptr_t addr) {
    if (!g_driver) return T{};
    return g_driver->read<T>(addr);
}
