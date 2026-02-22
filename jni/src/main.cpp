#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// 模块头文件
#include "stealth/stealth.h"
#include "driver/memory.h"
#include "game/GameReader.h"
#include "overlay/overlay.h"
#include "overlay/menu.h"
#include "render/render.h"
#include "net/net.h"

// ============================================================
// 哈皮哈啤哈屁 — 王者荣耀视野共享工具
// ============================================================

static const char* GAME_PACKAGE = "com.tencent.tmgp.sgame";

// 屏幕分辨率全局变量 (GameStructs.h 中 extern 声明)
float g_screenW = 2340.0f;
float g_screenH = 1080.0f;

// 获取屏幕分辨率
static void GetScreenSize(int& w, int& h) {
    // 默认值
    w = 2340; h = 1080;

    FILE* f = popen("wm size", "r");
    if (f) {
        char buf[128];
        while (fgets(buf, sizeof(buf), f)) {
            // "Physical size: 2340x1080"
            int tw = 0, th = 0;
            if (sscanf(buf, "Physical size: %dx%d", &tw, &th) == 2 ||
                sscanf(buf, "Override size: %dx%d", &tw, &th) == 2) {
                w = tw; h = th;
            }
        }
        pclose(f);
    }
}

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("  哈皮哈啤哈屁 v1.0\n");
    printf("  王者荣耀视野共享工具\n");
    printf("========================================\n");

    // ===== Step 1: 隐身模块 =====
    stealth_init(argc, argv);
    printf("[✓] 隐身模块初始化完成\n");

    // ===== Step 2: 驱动初始化 =====
    if (!init_driver()) {
        printf("[✗] 无可用驱动，退出\n");
        return 1;
    }
    printf("[✓] 驱动初始化完成\n");

    // ===== Step 3: 等待游戏进程 =====
    printf("[...] 等待游戏 (%s)...\n", GAME_PACKAGE);
    pid_t pid = 0;
    while ((pid = get_game_pid(GAME_PACKAGE)) <= 0) {
        sleep(1);
    }
    printf("[✓] 游戏PID: %d\n", pid);

    // ===== Step 4: 初始化 GameReader =====
    GameReader reader;
    while (!reader.Init()) {
        printf("[...] 等待 libGameCore.so 加载...\n");
        sleep(2);
    }
    printf("[✓] GameReader 初始化完成\n");

    // ===== Step 5: 获取屏幕尺寸 & 初始化 Overlay =====
    int screenW, screenH;
    GetScreenSize(screenW, screenH);
    g_screenW = (float)screenW;
    g_screenH = (float)screenH;
    printf("[✓] 屏幕分辨率: %dx%d\n", screenW, screenH);

    if (!overlay_init(screenW, screenH)) {
        printf("[✗] 覆盖层初始化失败\n");
        return 1;
    }
    printf("[✓] EGL覆盖层初始化完成\n");

    // ===== Step 6: 网络模块 (可选) =====
    NetServer server;
    NetClient client;

    // ===== Step 7: 主循环 =====
    printf("[✓] 进入主循环 (60FPS)\n");
    printf("========================================\n");

    while (true) {
        // 1. 读取游戏数据
        reader.Update();

        // 2. 如果启用网络
        if (g_config.netEnabled) {
            if (g_config.isServer) {
                // 服务端: 发送数据
                if (server.serverFd < 0) {
                    server.Start(g_config.serverPort);
                }
                server.Send(reader.state);
            } else {
                // 客户端: 接收数据 (覆盖本地数据)
                if (!client.connected) {
                    client.Connect(g_config.serverIP, g_config.serverPort);
                }
            }
        }

        // 使用的游戏状态 (网络客户端模式用远程数据)
        const GameState& gs = (g_config.netEnabled && !g_config.isServer && client.connected)
            ? client.receivedState
            : reader.state;

        // 3. 开始渲染帧
        overlay_begin();

        // 4. 绘制游戏数据
        if (gs.inMatch) {
            Render::DrawAll(gs);
        }

        // 5. 绘制UI
        DrawFloatingBall();
        DrawMenu();

        // 6. 结束渲染帧
        overlay_end();

        // 7. 帧率控制 (~60FPS)
        usleep(16000);
    }

    // 清理
    overlay_destroy();
    server.Stop();
    client.Disconnect();

    return 0;
}
