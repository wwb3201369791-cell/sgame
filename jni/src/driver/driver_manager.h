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
#include <unistd.h>

/**
 * 驱动自动检测管理器
 * 按优先级依次尝试: QX → Dev → Proc → RT_HookPro → Dit → DitPro → Pread → Syscall
 */
class DriverManager {
public:
    static driver_base* auto_detect() {
        printf("[DriverManager] 开始自动检测驱动...\n");

        auto accept_or_delete = [](driver_base* drv) -> driver_base* {
            if (!drv) return nullptr;
            if (drv->is_available()) return drv;
            delete drv;
            return nullptr;
        };

        auto probe_driver = [](driver_base* drv, const char* name) -> bool {
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
        };

        // 优先级 1: QX 内核驱动 (ioctl 0x801/0x802)
        try {
            if (auto* drv = accept_or_delete(new qx_driver())) {
                if (probe_driver(drv, "QX")) {
                    printf("[DriverManager] ✓ QX驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ QX驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ QX驱动不可用\n");
        }

        // 优先级 2: RT Dev驱动 (/dev 6字母节点)
        try {
            if (auto* drv = accept_or_delete(new dev_driver())) {
                if (probe_driver(drv, "Dev")) {
                    printf("[DriverManager] ✓ Dev驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ Dev驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ Dev驱动不可用\n");
        }

        // 优先级 3: GT2.0 Proc驱动 (/proc 6字母节点)
        try {
            if (auto* drv = accept_or_delete(new proc_driver())) {
                if (probe_driver(drv, "Proc")) {
                    printf("[DriverManager] ✓ Proc驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ Proc驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ Proc驱动不可用\n");
        }

        // 优先级 4: RT HookPro驱动 (AF_INET SOCK_DGRAM ioctl)
        try {
            if (auto* drv = accept_or_delete(new rt_hookpro_driver())) {
                if (probe_driver(drv, "RT_HookPro")) {
                    printf("[DriverManager] ✓ RT_HookPro驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ RT_HookPro驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ RT_HookPro驱动不可用\n");
        }

        // 优先级 5: Dit 3.3 (Netlink socket)
        try {
            if (auto* drv = accept_or_delete(new dit_driver())) {
                if (probe_driver(drv, "Dit")) {
                    printf("[DriverManager] ✓ Dit驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ Dit驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ Dit驱动不可用\n");
        }

        // 优先级 6: DitPro (syscall hook)
        try {
            if (auto* drv = accept_or_delete(new ditpro_driver())) {
                if (probe_driver(drv, "DitPro")) {
                    printf("[DriverManager] ✓ DitPro驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ DitPro驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ DitPro驱动不可用\n");
        }

        // 优先级 7: pread64 (/proc/pid/mem) — 需root但无需内核驱动
        try {
            if (auto* drv = accept_or_delete(new pread_driver())) {
                if (probe_driver(drv, "Pread")) {
                    printf("[DriverManager] ✓ Pread驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ Pread驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ Pread驱动不可用\n");
        }

        // 优先级 8: process_vm_readv/writev (最基础的方式)
        try {
            if (auto* drv = accept_or_delete(new syscall_driver())) {
                if (probe_driver(drv, "Syscall")) {
                    printf("[DriverManager] ✓ Syscall驱动加载成功\n");
                    return drv;
                }
                delete drv;
            }
            printf("[DriverManager] ✗ Syscall驱动不可用\n");
        } catch (...) {
            printf("[DriverManager] ✗ Syscall驱动不可用\n");
        }

        printf("[DriverManager] ✗ 所有驱动均不可用!\n");
        return nullptr;
    }
};
