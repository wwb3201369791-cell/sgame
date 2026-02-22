#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <inttypes.h>
#include <ctype.h>

// 模块头文件
#include "stealth/stealth.h"
#include "driver/memory.h"
#include "game/GameReader.h"
#include "overlay/overlay.h"
#include "overlay/menu.h"
#include "render/render.h"
#include "net/net.h"
#include "Android_touch/TouchHelperA.h"
#include "ANativeWindowCreator.h"
#include "imgui.h"

// ============================================================
// 哈皮哈啤哈屁 — 王者荣耀视野共享工具
// ============================================================

static char g_gamePackage[128] = "com.tencent.tmgp.sgame";
static volatile int g_loopCount = 0;
static const char* g_phase = "初始化";
static bool g_drawEnabled = true;
static char g_controlFile[256] = {0};
static char g_controlAckFile[256] = {0};
static unsigned long long g_controlLastSeq = 0;
static bool g_controlEnabled = false;
static pid_t g_gamePidHint = 0;

static void copy_str(char* dst, size_t dstSize, const char* src) {
    if (!dst || dstSize == 0) return;
    if (!src) src = "";
    snprintf(dst, dstSize, "%s", src);
}

static bool str_is_true(const char* s) {
    if (!s || !*s) return false;
    return strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "TRUE") == 0 ||
           strcmp(s, "yes") == 0 || strcmp(s, "YES") == 0 || strcmp(s, "on") == 0 ||
           strcmp(s, "ON") == 0;
}

static bool str_is_false(const char* s) {
    if (!s || !*s) return false;
    return strcmp(s, "0") == 0 || strcmp(s, "false") == 0 || strcmp(s, "FALSE") == 0 ||
           strcmp(s, "no") == 0 || strcmp(s, "NO") == 0 || strcmp(s, "off") == 0 ||
           strcmp(s, "OFF") == 0;
}

static void trim_line(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start > 0) memmove(s, s + start, strlen(s + start) + 1);
}

static const char* now_str() {
    static char buf[32];
    time_t t = time(nullptr);
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

static void control_ack(unsigned long long seq, const char* status, const char* detail) {
    if (g_controlAckFile[0] == '\0') return;
    FILE* f = fopen(g_controlAckFile, "a");
    if (!f) return;
    fprintf(f, "%llu|%s|%s|%s\n",
            seq,
            now_str(),
            status ? status : "OK",
            detail ? detail : "");
    fclose(f);
}

static void runtime_load_env() {
    const char* gamePkg = getenv("SGAME_GAME_PKG");
    if (gamePkg && *gamePkg) {
        copy_str(g_gamePackage, sizeof(g_gamePackage), gamePkg);
        printf("[Runtime] 使用环境变量游戏包名: %s\n", g_gamePackage);
    }

    const char* ctrlFile = getenv("SGAME_CONTROL_FILE");
    if (ctrlFile && *ctrlFile) {
        copy_str(g_controlFile, sizeof(g_controlFile), ctrlFile);
        g_controlEnabled = true;

        const char* ackFile = getenv("SGAME_CONTROL_ACK_FILE");
        if (ackFile && *ackFile) {
            copy_str(g_controlAckFile, sizeof(g_controlAckFile), ackFile);
        } else {
            snprintf(g_controlAckFile, sizeof(g_controlAckFile), "%s.ack", g_controlFile);
        }
        printf("[Control] 控制队列启用: %s\n", g_controlFile);
        printf("[Control] ACK文件: %s\n", g_controlAckFile);
    }

    const char* drawDefault = getenv("SGAME_DRAW_DEFAULT");
    if (drawDefault && *drawDefault) {
        if (str_is_false(drawDefault)) g_drawEnabled = false;
        if (str_is_true(drawDefault)) g_drawEnabled = true;
        printf("[Control] 初始绘制状态: %s (SGAME_DRAW_DEFAULT=%s)\n",
               g_drawEnabled ? "ON" : "OFF", drawDefault);
    }

    const char* pidHint = getenv("SGAME_GAME_PID");
    if (pidHint && *pidHint) {
        char* endp = nullptr;
        long v = strtol(pidHint, &endp, 10);
        if (endp && *endp == '\0' && v > 0) {
            g_gamePidHint = (pid_t)v;
            printf("[Runtime] 游戏PID提示: %d\n", (int)g_gamePidHint);
        }
    }
}

static bool control_parse_line(char* line,
                               unsigned long long fallbackSeq,
                               unsigned long long* outSeq,
                               char* outCmd,
                               size_t outCmdSize) {
    if (!line || !outSeq || !outCmd || outCmdSize == 0) return false;
    trim_line(line);
    if (line[0] == '\0') return false;

    // 新格式: SEQ|TIMESTAMP|CMD
    char* p1 = strchr(line, '|');
    if (p1) {
        char* p2 = strchr(p1 + 1, '|');
        if (p2) {
            *p1 = '\0';
            *p2 = '\0';
            char* endp = nullptr;
            unsigned long long seq = strtoull(line, &endp, 10);
            if (endp && *endp == '\0') {
                *outSeq = seq;
                copy_str(outCmd, outCmdSize, p2 + 1);
                trim_line(outCmd);
                return outCmd[0] != '\0';
            }
        }
    }

    // 兼容旧格式: CMD
    *outSeq = fallbackSeq;
    copy_str(outCmd, outCmdSize, line);
    trim_line(outCmd);
    return outCmd[0] != '\0';
}

static void control_exec_command(unsigned long long seq, const char* cmdline) {
    if (!cmdline || !*cmdline) return;

    char buf[256];
    copy_str(buf, sizeof(buf), cmdline);
    trim_line(buf);

    char* cmd = buf;
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    char* arg = cmd;
    while (*arg && *arg != ' ' && *arg != '\t') arg++;
    if (*arg) {
        *arg = '\0';
        arg++;
        while (*arg == ' ' || *arg == '\t') arg++;
    } else {
        arg = (char*)"";
    }

    if (strcmp(cmd, "PING") == 0) {
        printf("[Control] seq=%llu PING -> PONG\n", seq);
        control_ack(seq, "OK", "PONG");
        return;
    }
    if (strcmp(cmd, "INIT_DRAW") == 0) {
        g_drawEnabled = true;
        printf("[Control] seq=%llu INIT_DRAW -> DRAW_ON\n", seq);
        control_ack(seq, "OK", "DRAW_ON");
        return;
    }
    if (strcmp(cmd, "STOP_DRAW") == 0) {
        g_drawEnabled = false;
        g_config.showMenu = false;
        printf("[Control] seq=%llu STOP_DRAW -> DRAW_OFF\n", seq);
        control_ack(seq, "OK", "DRAW_OFF");
        return;
    }
    if (strcmp(cmd, "EXIT") == 0) {
        g_config.showMenu = false;
        g_exitRequested = true;
        printf("[Control] seq=%llu EXIT -> EXITING\n", seq);
        control_ack(seq, "OK", "EXITING");
        return;
    }
    if (strcmp(cmd, "SET_LOG_LEVEL") == 0) {
        printf("[Control] seq=%llu SET_LOG_LEVEL %s (当前版本仅记录)\n", seq, arg);
        control_ack(seq, "OK", "SET_LOG_LEVEL_QUEUED");
        return;
    }

    printf("[Control] seq=%llu 未支持命令: %s\n", seq, cmdline);
    control_ack(seq, "ERR", "UNSUPPORTED_COMMAND");
}

static void control_poll_file_once(bool allowResetRetry) {
    if (!g_controlEnabled || g_controlFile[0] == '\0') return;

    FILE* f = fopen(g_controlFile, "r");
    if (!f) return;

    char line[512];
    unsigned long long lineNo = 0;
    unsigned long long maxSeq = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        unsigned long long seq = 0;
        char cmd[256];
        if (!control_parse_line(line, lineNo, &seq, cmd, sizeof(cmd))) {
            continue;
        }
        if (seq > maxSeq) maxSeq = seq;
        if (seq <= g_controlLastSeq) {
            continue;
        }
        control_exec_command(seq, cmd);
        g_controlLastSeq = seq;
    }
    fclose(f);

    if (allowResetRetry && maxSeq < g_controlLastSeq) {
        // 队列可能被清空并重置序号；回退一次重新读取，避免新命令被永久忽略。
        printf("[Control] 检测到队列重置 (maxSeq=%llu < lastSeq=%llu), 重新同步\n",
               maxSeq, g_controlLastSeq);
        g_controlLastSeq = 0;
        control_poll_file_once(false);
    }
}

static void control_init_baseline() {
    if (!g_controlEnabled || g_controlFile[0] == '\0') return;

    FILE* f = fopen(g_controlFile, "r");
    if (!f) return;

    char line[512];
    unsigned long long lineNo = 0;
    unsigned long long maxSeq = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        unsigned long long seq = 0;
        char cmd[256];
        if (!control_parse_line(line, lineNo, &seq, cmd, sizeof(cmd))) {
            continue;
        }
        if (seq > maxSeq) maxSeq = seq;
    }
    fclose(f);

    g_controlLastSeq = maxSeq;
    printf("[Control] 初始队列基线 seq=%llu\n", g_controlLastSeq);
    control_ack(g_controlLastSeq, "OK", g_drawEnabled ? "BOOT_DRAW_ON" : "BOOT_DRAW_OFF");
}

static void crash_handler(int sig) {
    const char* name = (sig == SIGSEGV) ? "SIGSEGV" :
                       (sig == SIGBUS)  ? "SIGBUS"  :
                       (sig == SIGABRT) ? "SIGABRT" : "SIGNAL";
    printf("\n[CRASH] 信号 %d (%s), 帧=%d, 阶段=%s\n", sig, name, (int)g_loopCount, g_phase);
    fflush(stdout);
    _exit(128 + sig);
}

static void DrawDebugHUD(const GameReader& reader) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (!draw) return;

    char line1[256];
    char line2[256];
    char line3[256];

    snprintf(line1, sizeof(line1),
             "DBG inMatch=%d raw=%d hero=%d mon=%d m0=%.3f",
             reader.state.inMatch ? 1 : 0, reader.debugMatchRaw,
             reader.state.heroCount, reader.state.monsterCount, reader.debugMatrix0);
    snprintf(line2, sizeof(line2),
             "DBG entity=0x%llX monster=0x%llX bss=0x%llX",
             (unsigned long long)reader.debugEntityListHead,
             (unsigned long long)reader.debugMonsterListHead,
             (unsigned long long)reader.bssBase);
    snprintf(line3, sizeof(line3),
             "DBG readFail=%llu/%llu lastFail=0x%llX",
             g_mem_read_fail, g_mem_read_total, (unsigned long long)g_mem_last_fail_addr);

    ImVec2 p(18.0f, g_screenH - 90.0f);
    draw->AddRectFilled(ImVec2(p.x - 8, p.y - 6), ImVec2(p.x + 620, p.y + 54),
                        IM_COL32(0, 0, 0, 120), 6.0f);
    draw->AddText(p, IM_COL32(255, 255, 255, 255), line1);
    draw->AddText(ImVec2(p.x, p.y + 18), IM_COL32(255, 255, 255, 255), line2);
    draw->AddText(ImVec2(p.x, p.y + 36),
                  (g_mem_read_fail > 0 ? IM_COL32(255, 120, 120, 255) : IM_COL32(120, 255, 120, 255)),
                  line3);
}

// 屏幕分辨率全局变量 (GameStructs.h 中 extern 声明)
float g_screenW = 2340.0f;
float g_screenH = 1080.0f;

// 获取屏幕分辨率
static void GetScreenSize(int& w, int& h) {
    // 默认值
    w = 2340; h = 1080;

    // 优先用 SurfaceComposer 获取当前方向下的真实尺寸（比 wm size 更准确）
    auto di = android::ANativeWindowCreator::GetDisplayInfo();
    if (di.width > 0 && di.height > 0) {
        w = di.width;
        h = di.height;
        return;
    }

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
    signal(SIGSEGV, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGABRT, crash_handler);

    runtime_load_env();

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
    printf("[...] 等待游戏 (%s)...\n", g_gamePackage);
    pid_t pid = 0;
    if (g_gamePidHint > 0 && kill(g_gamePidHint, 0) == 0) {
        pid = g_gamePidHint;
        g_game_pid = pid;
        if (g_driver) g_driver->set_pid(pid);
        printf("[✓] 使用环境变量提供的游戏PID: %d\n", pid);
    }
    while (pid <= 0) {
        pid = get_game_pid(g_gamePackage);
        if (pid > 0) break;
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

    // ===== Step 5.1: 触摸输入 (用于悬浮球/菜单交互, 只读不抢占游戏触摸) =====
    auto di = android::ANativeWindowCreator::GetDisplayInfo();
    if (di.width > 0 && di.height > 0) {
        Touch::setOrientation(di.orientation);
        printf("[Touch] 显示方向: %d, 显示尺寸: %dx%d\n", di.orientation, di.width, di.height);
    }
    if (Touch::Init({(float)screenW, (float)screenH}, true)) {
        Touch::setOtherTouch(g_config.touchAltMapping);
        printf("[✓] 触摸输入初始化完成 (只读模式)\n");
    } else {
        printf("[!] 触摸输入初始化失败 (悬浮球不可交互)\n");
    }

    // ===== Step 6: 网络模块 (可选) =====
    NetServer server;
    NetClient client;
    control_init_baseline();

    // ===== Step 7: 主循环 =====
    printf("[✓] 进入主循环 (60FPS)\n");
    printf("========================================\n");

    while (true) {
        g_loopCount++;

        if ((g_loopCount % 6) == 1) {
            g_phase = "control_poll";
            control_poll_file_once(true);
        }

        // 动态刷新触摸方向和备用映射（处理旋转/机型差异）
        static int lastTouchOrientation = -1;
        static bool lastAltMapping = g_config.touchAltMapping;
        if ((g_loopCount % 60) == 1) {
            auto curDi = android::ANativeWindowCreator::GetDisplayInfo();
            if (curDi.width > 0 && curDi.height > 0 && curDi.orientation != lastTouchOrientation) {
                lastTouchOrientation = curDi.orientation;
                Touch::setOrientation(curDi.orientation);
                printf("[Touch] 方向更新: %d (%dx%d)\n", curDi.orientation, curDi.width, curDi.height);
            }
        }
        if (lastAltMapping != g_config.touchAltMapping) {
            lastAltMapping = g_config.touchAltMapping;
            Touch::setOtherTouch(g_config.touchAltMapping);
            printf("[Touch] Alt Mapping: %s\n", g_config.touchAltMapping ? "ON" : "OFF");
        }

        // 1. 读取游戏数据
        g_phase = "reader.Update";
        reader.Update();

        // 2. 如果启用网络
        static time_t lastClientConnectTry = 0;
        if (g_config.netEnabled) {
            g_phase = "network";
            if (g_config.isServer) {
                // 切换到服务端模式时，关闭客户端连接
                if (client.connected || client.sockFd >= 0) {
                    client.Disconnect();
                }

                // 服务端: 发送数据
                if (server.serverFd < 0) {
                    server.Start(g_config.serverPort);
                }
                server.Send(reader.state);
            } else {
                // 切换到客户端模式时，关闭服务端监听
                if (server.serverFd >= 0 || server.running) {
                    server.Stop();
                }

                // 客户端: 接收数据 (覆盖本地数据)
                if (!client.connected) {
                    time_t now = time(nullptr);
                    if (now - lastClientConnectTry >= 2) {
                        lastClientConnectTry = now;
                        client.Connect(g_config.serverIP, g_config.serverPort);
                    }
                }
            }
        } else {
            // 网络关闭时释放连接/监听，避免后台占用端口
            if (server.serverFd >= 0 || server.running) {
                server.Stop();
            }
            if (client.connected || client.sockFd >= 0) {
                client.Disconnect();
            }
        }

        // 使用的游戏状态 (网络客户端模式用远程数据)
        const GameState& gs = (g_config.netEnabled && !g_config.isServer && client.connected)
            ? client.receivedState
            : reader.state;

        // 3. 开始渲染帧
        g_phase = "overlay_begin";
        overlay_begin();

        // 4. 绘制游戏数据
        if (g_drawEnabled && gs.inMatch) {
            g_phase = "Render::DrawAll";
            Render::DrawAll(gs);
        }

        // 5. 绘制UI
        if (g_drawEnabled) {
            g_phase = "DrawFloatingBall";
            DrawFloatingBall();
            g_phase = "DrawMenu";
            DrawMenu();
            g_phase = "DebugHUD";
            DrawDebugHUD(reader);
        }

        // 6. 结束渲染帧
        g_phase = "overlay_end";
        overlay_end();

        if (ConsumeExitRequest()) {
            printf("[Main] 收到退出请求，正在退出...\n");
            break;
        }

        // 7. 帧率控制 (~60FPS)
        usleep(16000);
    }

    // 清理
    overlay_destroy();
    Touch::Close();
    server.Stop();
    client.Disconnect();

    return 0;
}
