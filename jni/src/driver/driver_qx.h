#ifndef DRIVER_QX_H
#define DRIVER_QX_H

#include "driver_base.h"
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctype.h>
#include <fstream>
#include <iostream>

/**
 * QX 驱动 (QX 10.6 ~ 11.4)
 * 使用被隐藏的 dev 节点通信
 */
class qx_driver : public driver_base {
private:
    int fd;
    
    typedef struct _COPY_MEMORY {
        pid_t pid;
        uintptr_t address;
        void* buffer;
        size_t size;
    } COPY_MEMORY, *PCOPY_MEMORY;
    
    typedef struct _MODULE_BASE {
        pid_t pid;
        char* name;
        uintptr_t base;
    } MODULE_BASE, *PMODULE_BASE;
    
    enum OPERATIONS {
        OP_INIT_KEY = 0x800,
        OP_READ_MEM = 0x801,
        OP_WRITE_MEM = 0x802,
        OP_MODULE_BASE = 0x803
    };
    
    // 检查字符串是否全是字母
    bool str_alpha(const char* str) {
        for (int i = 0; i < 6; i++) {
            if (!isalpha(str[i])) {
                return false;
            }
        }
        return true;
    }
    
    // 启动驱动，查找被删除的驱动节点
    int start_driver() {
        DIR* dir = opendir("/proc");
        if (!dir) return -1;
        
        struct dirent* ptr;
        struct stat info;
        char path[1024];
        char buffer[1024];
        char fd_path[1024];
        char fd_buffer[1024];
        char dev_path[1024];
        char data_path[1024];
        int ID;
        int PPID;
        
        while ((ptr = readdir(dir)) != nullptr) {
            if (ptr->d_type == DT_DIR) {
                sprintf(buffer, "/proc/%d/exe", atoi(ptr->d_name));
                ssize_t len = readlink(buffer, path, sizeof(path) - 1);
                
                if (len != -1) {
                    path[len] = '\0';
                    char* stres = strstr(path, "(deleted)");
                    
                    if (stres != nullptr) {
                        sscanf(path, "/data/%s", data_path);
                        
                        if (str_alpha(data_path)) {
                            sscanf(buffer, "/proc/%d/exe", &PPID);
                            
                            // 检查 fd/3 和 fd/4
                            for (int i = 3; i < 5; i++) {
                                sprintf(fd_path, "/proc/%d/fd/%d", PPID, i);
                                ssize_t fd_len = readlink(fd_path, fd_buffer, sizeof(fd_buffer) - 1);
                                if (fd_len < 0) {
                                    continue;
                                }
                                fd_buffer[fd_len] = '\0';
                                char* stress = strstr(fd_buffer, "(deleted)");
                                
                                if (stress != nullptr) {
                                    int fd_file = open(fd_path, O_RDONLY);
                                    if (fd_file == -1) {
                                        continue;
                                    }
                                    
                                    if (fstat(fd_file, &info) == -1) {
                                        close(fd_file);
                                        continue;
                                    }
                                    
                                    sscanf(fd_buffer, "%s (deleted)", dev_path);
                                    
                                    // 检查文件是否存在
                                    if (access(dev_path, F_OK) == 0) {
                                        ID = open(dev_path, O_RDWR);
                                        if (ID != -1) {
                                            if (unlink(dev_path) == 0) {
                                                close(fd_file);
                                                closedir(dir);
                                                return ID;
                                            }
                                        }
                                    } else {
                                        // 驱动已经隐藏，需要重新创建节点
                                        mode_t mode = S_IFCHR | 0666;
                                        dev_t dev = makedev(major(info.st_rdev), minor(info.st_rdev));
                                        
                                        mknod(dev_path, mode, dev);
                                        ID = open(dev_path, O_RDWR);
                                        
                                        if (ID != -1) {
                                            unlink(dev_path);
                                            close(fd_file);
                                            closedir(dir);
                                            return ID;
                                        }
                                    }
                                    
                                    close(fd_file);
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        
        closedir(dir);
        return -1;
    }

public:
    qx_driver() : fd(-1) {
        fd = start_driver();
        if (fd <= 0) {
            std::cout << "[-] QX驱动异常或不存在,请安装QX10.6~11.4" << std::endl;
        } else {
            std::cout << "[+] QX驱动加载成功" << std::endl;
        }
    }

    bool is_available() const override {
        return fd > 0;
    }
    
    ~qx_driver() override {
        if (fd > 0) {
            close(fd);
            std::cout << "[-] QX驱动关闭成功" << std::endl;
        }
    }
    
    bool set_pid(pid_t pid) override {
        target_pid = pid;
        return target_pid > 0;
    }
    
    bool read(uintptr_t address, void* buffer, size_t size) override {
        if (fd <= 0) return false;
        
        // QX驱动需要对地址进行处理
        address = address & 0xFFFFFFFFFFFF;
        
        COPY_MEMORY cm;
        cm.pid = target_pid;
        cm.address = address;
        cm.buffer = buffer;
        cm.size = size;
        
        return ioctl(fd, OP_READ_MEM, &cm) == 0;
    }
    
    bool write(uintptr_t address, void* buffer, size_t size) override {
        if (fd <= 0) return false;
        
        // QX驱动需要对地址进行处理
        address = address & 0xFFFFFFFFFFFF;
        
        COPY_MEMORY cm;
        cm.pid = target_pid;
        cm.address = address;
        cm.buffer = buffer;
        cm.size = size;
        
        return ioctl(fd, OP_WRITE_MEM, &cm) == 0;
    }
    
    uintptr_t get_module_base(const char* name) override {
        if (fd <= 0) return 0;
        
        MODULE_BASE mb;
        char buf[0x100];
        strncpy(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        mb.pid = target_pid;
        mb.name = buf;
        
        if (ioctl(fd, OP_MODULE_BASE, &mb) != 0) {
            return 0;
        }
        
        return mb.base;
    }
};

#endif // DRIVER_QX_H
