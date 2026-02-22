#ifndef DRIVER_DEV_H
#define DRIVER_DEV_H

#include "driver_base.h"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <time.h>
#include <fstream>
#include <iostream>

/**
 * Dev 驱动 (RT_DevPro+)
 * 使用 /dev 目录下的驱动节点通信
 */
class dev_driver : public driver_base {
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
    
    // 搜索 /dev 目录下的驱动节点
    char* dev_search() {
        const char* dev_path = "/dev";
        DIR* dir = opendir(dev_path);
        if (!dir) {
            std::cout << "无法打开/dev目录" << std::endl;
            return nullptr;
        }
        
        struct dirent* entry;
        char file_path[256];
        
        while ((entry = readdir(dir)) != nullptr) {
            // 过滤掉一些已知的非驱动文件
            if (strstr(entry->d_name, "std") != nullptr ||
                strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0 ||
                strstr(entry->d_name, "gpiochip") != nullptr ||
                strstr(entry->d_name, "common") != nullptr ||
                strstr(entry->d_name, "accdet") != nullptr) {
                continue;
            }
            
            // 过滤包含特殊字符的文件名
            if (strchr(entry->d_name, '_') != nullptr ||
                strchr(entry->d_name, '-') != nullptr ||
                strchr(entry->d_name, ':') != nullptr) {
                continue;
            }
            
            // 长度必须是6
            if (strlen(entry->d_name) != 6) {
                continue;
            }
            
            sprintf(file_path, "%s/%s", dev_path, entry->d_name);
            
            struct stat file_info;
            if (stat(file_path, &file_info) < 0) {
                continue;
            }
            
            // 检查创建时间（1980年之前的跳过）
            if ((localtime(&file_info.st_ctime)->tm_year + 1900) <= 1980) {
                continue;
            }
            
            // 检查所有者
            if (file_info.st_gid != 0 || file_info.st_uid != 0) {
                continue;
            }
            
            // 检查权限
            if ((file_info.st_mode & 07777) != 0600) {
                continue;
            }
            
            // 检查是否是字符设备或块设备
            if (S_ISCHR(file_info.st_mode) || S_ISBLK(file_info.st_mode)) {
                std::cout << "[+] 找到驱动节点: " << file_path << std::endl;
                char* devpath = (char*)malloc(32);
                strcpy(devpath, file_path);
                closedir(dir);
                return devpath;
            }
        }
        
        closedir(dir);
        return nullptr;
    }

public:
    dev_driver() : fd(-1) {
        char* dev_path = dev_search();
        if (!dev_path) {
            std::cout << "[-] 未找到Dev驱动节点" << std::endl;
            return;
        }
        
        fd = open(dev_path, O_RDWR);
        if (fd > 0) {
            std::cout << "[+] Dev驱动加载成功: " << dev_path << std::endl;
        } else {
            std::cout << "[-] Dev驱动打开失败: " << dev_path << std::endl;
        }
        
        free(dev_path);
    }
    
    ~dev_driver() override {
        if (fd > 0) {
            close(fd);
            std::cout << "[-] Dev驱动关闭成功" << std::endl;
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

#endif // DRIVER_DEV_H

