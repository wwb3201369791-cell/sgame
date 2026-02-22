#pragma once
#include "../game/GameStructs.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

// ============================================================
// 网络共享模块 — TCP 发送/接收 GameState
// ============================================================

// 魔数标识 (用于校验数据包)
static const uint32_t NET_MAGIC = 0x48504850; // "HPHP"

struct NetPacket {
    uint32_t  magic;
    uint32_t  size;
    GameState data;
};

// ===== 服务端 =====
class NetServer {
public:
    int serverFd = -1;
    int clientFd = -1;
    bool running = false;

    bool Start(int port) {
        serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0) {
            printf("[NetServer] ✗ socket创建失败\n");
            return false;
        }

        int opt = 1;
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("[NetServer] ✗ bind失败: %s\n", strerror(errno));
            close(serverFd);
            return false;
        }

        listen(serverFd, 1);
        running = true;
        printf("[NetServer] ✓ 监听端口 %d\n", port);

        // 异步等待连接
        pthread_t tid;
        pthread_create(&tid, nullptr, AcceptThread, this);
        pthread_detach(tid);

        return true;
    }

    void Send(const GameState& state) {
        if (clientFd < 0) return;

        NetPacket pkt;
        pkt.magic = NET_MAGIC;
        pkt.size = sizeof(GameState);
        pkt.data = state;

        ssize_t ret = write(clientFd, &pkt, sizeof(pkt));
        if (ret <= 0) {
            printf("[NetServer] 客户端断开\n");
            close(clientFd);
            clientFd = -1;
        }
    }

    void Stop() {
        running = false;
        if (clientFd >= 0) close(clientFd);
        if (serverFd >= 0) close(serverFd);
        clientFd = serverFd = -1;
    }

private:
    static void* AcceptThread(void* arg) {
        NetServer* self = (NetServer*)arg;
        while (self->running) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int fd = accept(self->serverFd, (struct sockaddr*)&clientAddr, &addrLen);
            if (fd >= 0) {
                if (self->clientFd >= 0) close(self->clientFd);
                self->clientFd = fd;
                printf("[NetServer] ✓ 客户端已连接\n");
            }
        }
        return nullptr;
    }
};

// ===== 客户端 =====
class NetClient {
public:
    int sockFd = -1;
    bool connected = false;
    GameState receivedState;

    bool Connect(const char* ip, int port) {
        sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockFd < 0) return false;

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip, &addr.sin_addr);

        if (connect(sockFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("[NetClient] ✗ 连接失败: %s:%d\n", ip, port);
            close(sockFd);
            sockFd = -1;
            return false;
        }

        connected = true;
        printf("[NetClient] ✓ 已连接 %s:%d\n", ip, port);

        // 异步接收
        pthread_t tid;
        pthread_create(&tid, nullptr, RecvThread, this);
        pthread_detach(tid);

        return true;
    }

    void Disconnect() {
        connected = false;
        if (sockFd >= 0) close(sockFd);
        sockFd = -1;
    }

private:
    static void* RecvThread(void* arg) {
        NetClient* self = (NetClient*)arg;

        while (self->connected) {
            NetPacket pkt;
            ssize_t total = 0;
            ssize_t target = sizeof(pkt);

            while (total < target) {
                ssize_t ret = read(self->sockFd, (char*)&pkt + total, target - total);
                if (ret <= 0) {
                    printf("[NetClient] 连接断开\n");
                    self->connected = false;
                    close(self->sockFd);
                    self->sockFd = -1;
                    return nullptr;
                }
                total += ret;
            }

            if (pkt.magic == NET_MAGIC && pkt.size == sizeof(GameState)) {
                self->receivedState = pkt.data;
            }
        }
        return nullptr;
    }
};
