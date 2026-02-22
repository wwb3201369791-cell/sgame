#pragma once
#include "../game/GameStructs.h"
#include <cstdint>
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
        if (running && serverFd >= 0) return true;

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
            serverFd = -1;
            return false;
        }

        if (listen(serverFd, 1) < 0) {
            printf("[NetServer] ✗ listen失败: %s\n", strerror(errno));
            close(serverFd);
            serverFd = -1;
            return false;
        }

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

        if (!WriteAll(clientFd, &pkt, sizeof(pkt))) {
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
    static bool WriteAll(int fd, const void* data, size_t size) {
        const char* p = static_cast<const char*>(data);
        size_t sent = 0;
        while (sent < size) {
            int sendFlags = 0;
#ifdef MSG_NOSIGNAL
            sendFlags = MSG_NOSIGNAL;
#endif
            ssize_t ret = send(fd, p + sent, size - sent, sendFlags);
            if (ret < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (ret == 0) return false;
            sent += static_cast<size_t>(ret);
        }
        return true;
    }

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
            } else if (self->running) {
                usleep(100000);
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
        if (connected) return true;

        sockFd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockFd < 0) return false;

        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
            printf("[NetClient] ✗ IP无效: %s\n", ip);
            close(sockFd);
            sockFd = -1;
            return false;
        }

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
    static bool ReadAll(int fd, void* data, size_t size) {
        char* p = static_cast<char*>(data);
        size_t got = 0;
        while (got < size) {
            ssize_t ret = read(fd, p + got, size - got);
            if (ret < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (ret == 0) return false;
            got += static_cast<size_t>(ret);
        }
        return true;
    }

    static void* RecvThread(void* arg) {
        NetClient* self = (NetClient*)arg;

        while (self->connected) {
            NetPacket pkt;
            if (!ReadAll(self->sockFd, &pkt, sizeof(pkt))) {
                if (self->connected) {
                    printf("[NetClient] 连接断开\n");
                }
                self->connected = false;
                if (self->sockFd >= 0) {
                    close(self->sockFd);
                    self->sockFd = -1;
                }
                return nullptr;
            }

            if (pkt.magic == NET_MAGIC && pkt.size == sizeof(GameState)) {
                self->receivedState = pkt.data;
            }
        }
        return nullptr;
    }
};
