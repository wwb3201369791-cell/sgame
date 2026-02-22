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

/**
 * 驱动自动检测管理器
 * 按优先级依次尝试: QX → Dev → Proc → RT_HookPro → Dit → DitPro → Pread → Syscall
 */
class DriverManager {
public:
    static driver_base* auto_detect() {
        printf("[DriverManager] 开始自动检测驱动...\n");

        // 优先级 1: QX 内核驱动 (ioctl 0x801/0x802)
        try {
            auto* drv = new driver_qx();
            printf("[DriverManager] ✓ QX驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ QX驱动不可用\n");
        }

        // 优先级 2: RT Dev驱动 (/dev 6字母节点)
        try {
            auto* drv = new driver_dev();
            printf("[DriverManager] ✓ Dev驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ Dev驱动不可用\n");
        }

        // 优先级 3: GT2.0 Proc驱动 (/proc 6字母节点)
        try {
            auto* drv = new driver_proc();
            printf("[DriverManager] ✓ Proc驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ Proc驱动不可用\n");
        }

        // 优先级 4: RT HookPro驱动 (AF_INET SOCK_DGRAM ioctl)
        try {
            auto* drv = new driver_rt_hookpro();
            printf("[DriverManager] ✓ RT_HookPro驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ RT_HookPro驱动不可用\n");
        }

        // 优先级 5: Dit 3.3 (Netlink socket)
        try {
            auto* drv = new driver_dit();
            printf("[DriverManager] ✓ Dit驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ Dit驱动不可用\n");
        }

        // 优先级 6: DitPro (syscall hook)
        try {
            auto* drv = new driver_ditpro();
            printf("[DriverManager] ✓ DitPro驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ DitPro驱动不可用\n");
        }

        // 优先级 7: pread64 (/proc/pid/mem) — 需root但无需内核驱动
        try {
            auto* drv = new driver_pread();
            printf("[DriverManager] ✓ Pread驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ Pread驱动不可用\n");
        }

        // 优先级 8: process_vm_readv/writev (最基础的方式)
        try {
            auto* drv = new driver_syscall();
            printf("[DriverManager] ✓ Syscall驱动加载成功\n");
            return drv;
        } catch (...) {
            printf("[DriverManager] ✗ Syscall驱动不可用\n");
        }

        printf("[DriverManager] ✗ 所有驱动均不可用!\n");
        return nullptr;
    }
};
