#ifndef DRIVER_DITPRO_H
#define DRIVER_DITPRO_H

#include "driver_base.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

/**
 * DitPro 驱动
 * 使用 syscall 方式通信
 */
class ditpro_driver : public driver_base {
private:
    struct Ditpro_uct {
        int read_write;  // 读或者写
        int pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
        int wendi;  // 这个是判断只进行属于我们的操作,616
    };
    
    static constexpr int READ_OP = 0x400;
    static constexpr int WRITE_OP = 0x200;
    static constexpr int WENDI_MAGIC = 616;

public:
    ditpro_driver() {
        std::cout << "[+] DitPro驱动初始化成功" << std::endl;
    }
    
    ~ditpro_driver() override {
        std::cout << "[-] DitPro驱动关闭" << std::endl;
    }
    
    bool set_pid(pid_t pid) override {
        target_pid = pid;
        return target_pid > 0;
    }
    
    bool read(uintptr_t address, void* buffer, size_t size) override {
        if (target_pid <= 0) return false;
        
        Ditpro_uct ptr;
        ptr.read_write = READ_OP;
        ptr.addr = address;
        ptr.buffer = buffer;
        ptr.wendi = WENDI_MAGIC;
        ptr.pid = target_pid;
        ptr.size = size;
        
        return syscall(SYS_lookup_dcookie, &ptr) == 0;
    }
    
    bool write(uintptr_t address, void* buffer, size_t size) override {
        if (target_pid <= 0) return false;
        
        Ditpro_uct ptr;
        ptr.read_write = WRITE_OP;
        ptr.addr = address;
        ptr.buffer = buffer;
        ptr.wendi = WENDI_MAGIC;
        ptr.pid = target_pid;
        ptr.size = size;
        
        return syscall(SYS_lookup_dcookie, &ptr) == 0;
    }
    
    uintptr_t get_module_base(const char* name) override {
        return get_module_base_from_maps(target_pid, name);
    }

private:
    // 从 /proc/pid/maps 读取模块基址
    static uintptr_t get_module_base_from_maps(int pid, const char* name) {
        char filename[64];
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
        
        std::ifstream file(filename);
        if (!file.is_open()) {
            return 0;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(name) != std::string::npos) {
                uintptr_t addr = 0;
                sscanf(line.c_str(), "%lx", &addr);
                if (addr == 0x8000) addr = 0;
                return addr;
            }
        }
        
        return 0;
    }
};

#endif // DRIVER_DITPRO_H

