#ifndef DRIVER_DIT_H
#define DRIVER_DIT_H

#include "driver_base.h"
#include <sys/socket.h>
#include <linux/netlink.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

/**
 * Dit 驱动 (Dit 3.3)
 * 使用 Netlink 方式通信
 */
class dit_driver : public driver_base {
private:
    static constexpr int MAX_PAYLOAD = 1048;
    
    struct Memory_uct {
        int read_write;  // 读或者写
        int pid;
        uintptr_t addr;
        void* buffer;
        size_t size;
    };
    
    static constexpr int READ_OP = 0x400;
    static constexpr int WRITE_OP = 0x200;
    
    int sock_fd;
    struct sockaddr_nl dest_addr;
    struct nlmsghdr* nlh;
    struct iovec iov;
    struct msghdr msg;

public:
    dit_driver() : sock_fd(-1), nlh(nullptr) {
        sock_fd = socket(PF_NETLINK, SOCK_RAW, 17);
        if (sock_fd < 0) {
            std::cout << "[-] Dit驱动打开失败" << std::endl;
            return;
        }
        
        // 初始化 netlink
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.nl_family = AF_NETLINK;
        dest_addr.nl_pid = 0;  // 内核
        dest_addr.nl_groups = 0;
        
        nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_PAYLOAD));
        if (!nlh) {
            perror("malloc");
            close(sock_fd);
            sock_fd = -1;
            return;
        }
        
        memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
        nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
        nlh->nlmsg_flags = 0;
        
        iov.iov_base = (void*)nlh;
        iov.iov_len = nlh->nlmsg_len;
        
        msg.msg_name = (void*)&dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        
        std::cout << "[+] Dit驱动加载成功" << std::endl;
    }
    
    ~dit_driver() override {
        if (sock_fd >= 0) {
            close(sock_fd);
        }
        if (nlh) {
            free(nlh);
        }
        std::cout << "[-] Dit驱动关闭成功" << std::endl;
    }
    
    bool set_pid(pid_t pid) override {
        target_pid = pid;
        return target_pid > 0;
    }
    
    bool read(uintptr_t address, void* buffer, size_t size) override {
        if (sock_fd < 0 || !nlh) return false;
        
        Memory_uct ptr;
        void* uctaddr = &ptr;
        ptr.addr = address;
        ptr.buffer = buffer;
        ptr.pid = target_pid;
        ptr.read_write = READ_OP;
        ptr.size = size;
        
        int bytelen = 8;
        char* source = (char*)&uctaddr;
        char* target = (char*)NLMSG_DATA(nlh);
        
        if (!source || !target) return false;
        
        while (bytelen--) {
            *target++ = *source++;
        }
        
        return sendmsg(sock_fd, &msg, 0) >= 0;
    }
    
    bool write(uintptr_t address, void* buffer, size_t size) override {
        if (sock_fd < 0 || !nlh) return false;
        
        Memory_uct ptr;
        void* uctaddr = &ptr;
        ptr.addr = address;
        ptr.buffer = buffer;
        ptr.pid = target_pid;
        ptr.read_write = WRITE_OP;
        ptr.size = size;
        
        int bytelen = 8;
        char* source = (char*)&uctaddr;
        char* target = (char*)NLMSG_DATA(nlh);
        
        if (!source || !target) return false;
        
        while (bytelen--) {
            *target++ = *source++;
        }
        
        return sendmsg(sock_fd, &msg, 0) >= 0;
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

#endif // DRIVER_DIT_H

