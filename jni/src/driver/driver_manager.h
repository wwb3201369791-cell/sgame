#pragma once
#include "driver_base.h"
#include "driver_qx.h"
#include "driver_dev.h"
#include "driver_proc.h"
#include "driver_rt_hookpro.h"
#include "driver_dit.h"
#include "driver_ditpro.h"
#include "driver_pread.h"
#include "driver_syscall.h"
#include <memory>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/**
 * 驱动自动检测管理器
 * 按优先级依次尝试: QX → Dev → Proc → RT_HookPro → Dit → DitPro → Pread → Syscall
 */
class DriverManager {
public:
    static driver_base* auto_detect() {
        printf("[DriverManager] 开始自动检测驱动...\n");

        // 优先级 1: QX 内核驱动 (ioctl 0x801/0x802)
        if (auto* drv = try_driver(new qx_driver(), "QX")) return drv;

        // 优先级 2: RT Dev驱动 (/dev 6字母节点)
        if (auto* drv = try_driver(new dev_driver(), "Dev")) return drv;

        // 优先级 3: GT2.0 Proc驱动 (/proc 6字母节点)
        if (auto* drv = try_driver(new proc_driver(), "Proc")) return drv;

        // 优先级 4: RT HookPro驱动 (AF_INET SOCK_DGRAM ioctl)
        if (auto* drv = try_driver(new rt_hookpro_driver(), "RT_HookPro")) return drv;

        // 优先级 5: Dit 3.3 (Netlink socket)
        if (auto* drv = try_driver(new dit_driver(), "Dit")) return drv;

        // 优先级 6: DitPro (syscall hook)
        if (auto* drv = try_driver(new ditpro_driver(), "DitPro")) return drv;

        // 优先级 7: pread64 (/proc/pid/mem) — 需root但无需内核驱动
        if (auto* drv = try_driver(new pread_driver(), "Pread")) return drv;

        // 优先级 8: process_vm_readv/writev (最基础的方式)
        if (auto* drv = try_driver(new syscall_driver(), "Syscall")) return drv;

        printf("[DriverManager] ✗ 所有驱动均不可用!\n");
        return nullptr;
    }

    static driver_base* detect_by_mode(const char* mode) {
        if (!mode || !*mode || strcmp(mode, "auto") == 0) {
            return auto_detect();
        }

        printf("[DriverManager] 按指定模式加载驱动: %s\n", mode);

        if (strcmp(mode, "qx") == 0) {
            return try_driver(new qx_driver(), "QX");
        }
        if (strcmp(mode, "dev") == 0) {
            return try_driver(new dev_driver(), "Dev");
        }
        if (strcmp(mode, "proc") == 0) {
            return try_driver(new proc_driver(), "Proc");
        }
        if (strcmp(mode, "rt") == 0 || strcmp(mode, "rt_hookpro") == 0) {
            return try_driver(new rt_hookpro_driver(), "RT_HookPro");
        }
        if (strcmp(mode, "dit") == 0) {
            return try_driver(new dit_driver(), "Dit");
        }
        if (strcmp(mode, "ditpro") == 0) {
            return try_driver(new ditpro_driver(), "DitPro");
        }
        if (strcmp(mode, "pread") == 0) {
            return try_driver(new pread_driver(), "Pread");
        }
        if (strcmp(mode, "syscall") == 0) {
            return try_driver(new syscall_driver(), "Syscall");
        }

        printf("[DriverManager] ✗ 未知驱动模式: %s\n", mode);
        return nullptr;
    }

private:
    static driver_base* accept_or_delete(driver_base* drv) {
        if (!drv) return nullptr;
        if (drv->is_available()) return drv;
        delete drv;
        return nullptr;
    }

    static bool probe_driver(driver_base* drv, const char* name) {
        if (!drv) return false;

        volatile unsigned long long marker = 0x1122334455667788ULL;
        unsigned long long out = 0;
        pid_t self = getpid();

        if (!drv->set_pid(self)) {
            printf("[DriverManager] ✗ %s驱动探测失败: set_pid(self) 失败\n", name);
            return false;
        }
        if (!drv->read((uintptr_t)&marker, &out, sizeof(out))) {
            printf("[DriverManager] ✗ %s驱动探测失败: read(self) 失败\n", name);
            return false;
        }
        if (out != marker) {
            printf("[DriverManager] ✗ %s驱动探测失败: 回读不一致 (0x%llX != 0x%llX)\n",
                   name, out, marker);
            return false;
        }
        return true;
    }

    static driver_base* try_driver(driver_base* raw, const char* name) {
        try {
            if (auto* drv = accept_or_delete(raw)) {
                if (probe_driver(drv, name)) {
                    printf("[DriverManager] ✓ %s驱动加载成功\n", name);
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ %s驱动不可用\n", name);
        } catch (...) {
            printf("[DriverManager] ✗ %s驱动不可用\n", name);
        }
        return nullptr;
    }
};
