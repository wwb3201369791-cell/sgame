#ifndef DRIVER_PROC_H
#define DRIVER_PROC_H

#include "driver_base.h"
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>

/**
 * Proc 驱动 (GT2.0 或 RT_Proc)
 * 使用 /proc 目录下的驱动节点通信
 */
class proc_driver : public driver_base {
private:
    int fd;
    
    typedef struct _COPY_MEMORY {
        pid_t pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
    } COPY_MEMORY;
    
    enum OPERATIONS {
        OP_READ_MEM = 0x801,
        OP_WRITE_MEM = 0x802
    };
    
    // 搜索 proc 驱动节点
    char* proc_search() {
        DIR* dr = opendir("/proc");
        if (!dr) {
            std::cout << "Could not open /proc directory" << std::endl;
            return nullptr;
        }
        
        struct dirent* de;
        char* device_path = nullptr;
        
        while ((de = readdir(dr)) != nullptr) {
            // 过滤已知的非驱动文件
            if (strlen(de->d_name) != 6 || 
                strcmp(de->d_name, "NVISPI") == 0 ||
                strcmp(de->d_name, "aputag") == 0 ||
                strcmp(de->d_name, "asound") == 0 ||
                strcmp(de->d_name, "clkdbg") == 0 ||
                strcmp(de->d_name, "crypto") == 0 ||
                strcmp(de->d_name, "driver") == 0 ||
                strcmp(de->d_name, "mounts") == 0 ||
                strcmp(de->d_name, "pidmap") == 0) {
                continue;
            }
            
            // 检查是否全是字母数字
            bool is_valid = true;
            for (int i = 0; i < 6; i++) {
                if (!isalnum(de->d_name[i])) {
                    is_valid = false;
                    break;
                }
            }
            
            if (is_valid) {
                device_path = (char*)malloc(11 + strlen(de->d_name));
                sprintf(device_path, "/proc/%s", de->d_name);
                
                struct stat sb;
                if (stat(device_path, &sb) == 0 && S_ISREG(sb.st_mode)) {
                    break;
                } else {
                    free(device_path);
                    device_path = nullptr;
                }
            }
        }
        
        closedir(dr);
        return device_path;
    }

public:
    proc_driver() : fd(-1) {
        char* proc_path = proc_search();
        if (!proc_path) {
            std::cout << "[-] 未找到Proc驱动节点" << std::endl;
            return;
        }
        
        fd = open(proc_path, O_RDWR);
        if (fd > 0) {
            std::cout << "[+] Proc驱动加载成功: " << proc_path << std::endl;
        } else {
            std::cout << "[-] Proc驱动打开失败: " << proc_path << std::endl;
        }
        
        free(proc_path);
    }
    
    ~proc_driver() override {
        if (fd > 0) {
            close(fd);
            std::cout << "[-] Proc驱动关闭成功" << std::endl;
        }
    }
    
    bool set_pid(pid_t pid) override {
        target_pid = pid;
        return target_pid > 0;
    }
    
    bool read(uintptr_t address, void* buffer, size_t size) override {
        if (fd <= 0) return false;
        
        COPY_MEMORY cm;
        cm.pid = target_pid;
        cm.addr = address;
        cm.buffer = buffer;
        cm.size = size;
        
        return ioctl(fd, OP_READ_MEM, &cm) == 0;
    }
    
    bool write(uintptr_t address, void* buffer, size_t size) override {
        if (fd <= 0) return false;
        
        COPY_MEMORY cm;
        cm.pid = target_pid;
        cm.addr = address;
        cm.buffer = buffer;
        cm.size = size;
        
        return ioctl(fd, OP_WRITE_MEM, &cm) == 0;
    }
    
    uintptr_t get_module_base(const char* name) override {
        return get_module_base_from_maps(target_pid, name);
    }

private:
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

#endif // DRIVER_PROC_H

