#pragma once
#include "imgui.h"
#include "overlay.h"
#include "../game/GameStructs.h"

// ============================================================
// 菜单配置与绘制
// ============================================================

// 全局配置
struct Config {
    // 显示开关
    bool showMenu       = false;   // 菜单面板可见
    bool showMinimap    = true;    // 小地图点位
    bool showESP        = true;    // ESP方框
    bool showSkillCD    = true;    // 技能CD
    bool showVisionAlert = true;   // 视野警示
    bool showMonsterCD  = true;    // 野怪计时
    bool showMinions    = false;   // 兵线 (TODO: 待偏移确认)

    // 网络配置
    bool isServer       = false;   // 是否为服务端
    bool netEnabled     = false;   // 网络共享开启
    char serverIP[32]   = "192.168.1.100";
    int  serverPort     = 8888;
    bool touchAltMapping = false;  // 触摸坐标备用映射

    // 绘制配置
    float mapScale      = 150.0f;  // 小地图缩放
    float mapOffsetX    = 10.0f;   // 小地图X偏移
    float mapOffsetY    = 10.0f;   // 小地图Y偏移
    float espLineWidth  = 2.0f;    // ESP线宽
};

static Config g_config;

// 浮球状态
static bool   g_ballDragging = false;
static ImVec2 g_ballPos = ImVec2(50.0f, 300.0f);
static float  g_ballRadius = 30.0f;
static bool   g_ballPosInited = false;
static bool   g_ballPrevDown = false;
static bool   g_ballMoved = false;
static ImVec2 g_ballPressPos = ImVec2(0.0f, 0.0f);
static ImVec2 g_ballStartPos = ImVec2(0.0f, 0.0f);
static bool   g_exitRequested = false;

inline bool ConsumeExitRequest() {
    bool requested = g_exitRequested;
    g_exitRequested = false;
    return requested;
}

// 绘制浮动悬浮球 (点击切换菜单)
inline void DrawFloatingBall() {
    ImGuiIO& io = ImGui::GetIO();

    // 首帧按当前显示尺寸放到左侧中上区域，避免固定坐标在横屏设备上跑偏
    if (!g_ballPosInited && io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f) {
        g_ballPos = ImVec2(io.DisplaySize.x * 0.12f, io.DisplaySize.y * 0.32f);
        g_ballPosInited = true;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 浮球颜色
    ImU32 ballColor = g_config.showMenu
        ? IM_COL32(255, 130, 150, 220)   // 深粉 (菜单打开)
        : IM_COL32(255, 200, 180, 180);  // 淡粉 (菜单关闭)

    draw->AddCircleFilled(g_ballPos, g_ballRadius, ballColor);
    draw->AddCircle(g_ballPos, g_ballRadius, IM_COL32(255, 255, 255, 120), 0, 2.0f);

    // 文字
    const char* txt = g_config.showMenu ? "×" : "HP";
    ImVec2 textSize = ImGui::CalcTextSize(txt);
    draw->AddText(
        ImVec2(g_ballPos.x - textSize.x / 2, g_ballPos.y - textSize.y / 2),
        IM_COL32(255, 255, 255, 255), txt);

    // 交互检测
    ImVec2 mousePos = io.MousePos;
    float dist = sqrtf((mousePos.x - g_ballPos.x) * (mousePos.x - g_ballPos.x) +
                       (mousePos.y - g_ballPos.y) * (mousePos.y - g_ballPos.y));
    bool down = io.MouseDown[0];
    bool justPressed = down && !g_ballPrevDown;
    bool justReleased = !down && g_ballPrevDown;
    const float hitRadius = g_ballRadius * 1.6f;

    if (justPressed && dist < hitRadius) {
        g_ballDragging = true;
        g_ballMoved = false;
        g_ballPressPos = mousePos;
        g_ballStartPos = g_ballPos;
    }

    if (g_ballDragging && down) {
        float dx = mousePos.x - g_ballPressPos.x;
        float dy = mousePos.y - g_ballPressPos.y;
        if (fabsf(dx) > 4.0f || fabsf(dy) > 4.0f) {
            g_ballMoved = true;
        }

        g_ballPos.x = g_ballStartPos.x + dx;
        g_ballPos.y = g_ballStartPos.y + dy;

        // 约束在屏幕内，避免拖出可见区域
        if (io.DisplaySize.x > 0.0f) {
            if (g_ballPos.x < g_ballRadius) g_ballPos.x = g_ballRadius;
            if (g_ballPos.x > io.DisplaySize.x - g_ballRadius) g_ballPos.x = io.DisplaySize.x - g_ballRadius;
        }
        if (io.DisplaySize.y > 0.0f) {
            if (g_ballPos.y < g_ballRadius) g_ballPos.y = g_ballRadius;
            if (g_ballPos.y > io.DisplaySize.y - g_ballRadius) g_ballPos.y = io.DisplaySize.y - g_ballRadius;
        }
    }

    if (g_ballDragging && justReleased) {
        if (!g_ballMoved && dist < hitRadius) {
            g_config.showMenu = !g_config.showMenu;
        }
        g_ballDragging = false;
    }

    g_ballPrevDown = down;
}

// 绘制设置面板
inline void DrawMenu() {
    if (!g_config.showMenu) return;
    bool zh = overlay_has_cjk_font();

    ImGui::SetNextWindowSize(ImVec2(380, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin(zh ? "哈皮哈啤哈屁 设置" : "HPHP Settings", &g_config.showMenu,
        ImGuiWindowFlags_NoCollapse);

    // ===== Display =====
    if (ImGui::CollapsingHeader(zh ? "显示设置" : "Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(zh ? "小地图点位" : "Minimap", &g_config.showMinimap);
        ImGui::Checkbox(zh ? "ESP 方框" : "ESP Box", &g_config.showESP);
        ImGui::Checkbox(zh ? "技能CD显示" : "Skill CD", &g_config.showSkillCD);
        ImGui::Checkbox(zh ? "视野警示" : "Vision Alert", &g_config.showVisionAlert);
        ImGui::Checkbox(zh ? "野怪计时" : "Monster Timer", &g_config.showMonsterCD);
        ImGui::Checkbox(zh ? "兵线显示 (TODO)" : "Minions (TODO)", &g_config.showMinions);
    }

    ImGui::Separator();

    // ===== Render =====
    if (ImGui::CollapsingHeader(zh ? "绘制配置" : "Render")) {
        ImGui::SliderFloat(zh ? "地图缩放" : "Map Scale", &g_config.mapScale, 80.0f, 300.0f);
        ImGui::SliderFloat(zh ? "地图X偏移" : "Map Offset X", &g_config.mapOffsetX, 0.0f, 500.0f);
        ImGui::SliderFloat(zh ? "地图Y偏移" : "Map Offset Y", &g_config.mapOffsetY, 0.0f, 500.0f);
        ImGui::SliderFloat(zh ? "ESP线宽" : "ESP Line Width", &g_config.espLineWidth, 1.0f, 5.0f);
    }

    ImGui::Separator();

    // ===== Network =====
    if (ImGui::CollapsingHeader(zh ? "网络共享" : "Network")) {
        ImGui::Checkbox(zh ? "启用网络共享" : "Enable Network Share", &g_config.netEnabled);
        ImGui::Checkbox(zh ? "作为服务端" : "Server Mode", &g_config.isServer);
        ImGui::InputText(zh ? "服务器IP" : "Server IP", g_config.serverIP, sizeof(g_config.serverIP));
        ImGui::InputInt(zh ? "端口" : "Port", &g_config.serverPort);
    }

    ImGui::Separator();

    // ===== Touch =====
    if (ImGui::CollapsingHeader(zh ? "触摸" : "Touch")) {
        ImGui::Checkbox(zh ? "备用映射" : "Alt Mapping", &g_config.touchAltMapping);
        ImGui::TextWrapped("%s",
            zh ? "如果拖动方向不对或只有单方向能动，请切换备用映射。"
               : "If drag works only on one axis or is rotated, toggle Alt Mapping.");
    }

    ImGui::Separator();
    if (ImGui::Button(zh ? "隐藏菜单" : "Hide Menu")) {
        g_config.showMenu = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(zh ? "退出工具" : "Exit Tool")) {
        g_exitRequested = true;
    }

    ImGui::End();
}
