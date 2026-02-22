#pragma once
#include "imgui.h"
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

    if (io.MouseClicked[0] && dist < g_ballRadius * 1.5f) {
        g_ballDragging = true;
    }
    if (g_ballDragging && io.MouseDown[0]) {
        g_ballPos = mousePos;
    }
    if (g_ballDragging && io.MouseReleased[0]) {
        g_ballDragging = false;
        if (dist < g_ballRadius) {
            g_config.showMenu = !g_config.showMenu;
        }
    }
}

// 绘制设置面板
inline void DrawMenu() {
    if (!g_config.showMenu) return;

    ImGui::SetNextWindowSize(ImVec2(380, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("哈皮哈啤哈屁 设置", &g_config.showMenu,
        ImGuiWindowFlags_NoCollapse);

    // ===== 显示设置 =====
    if (ImGui::CollapsingHeader("显示设置", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("小地图点位",   &g_config.showMinimap);
        ImGui::Checkbox("ESP 方框",     &g_config.showESP);
        ImGui::Checkbox("技能CD显示",   &g_config.showSkillCD);
        ImGui::Checkbox("视野警示",     &g_config.showVisionAlert);
        ImGui::Checkbox("野怪计时",     &g_config.showMonsterCD);
        ImGui::Checkbox("兵线显示 (TODO)", &g_config.showMinions);
    }

    ImGui::Separator();

    // ===== 绘制配置 =====
    if (ImGui::CollapsingHeader("绘制配置")) {
        ImGui::SliderFloat("地图缩放", &g_config.mapScale, 80.0f, 300.0f);
        ImGui::SliderFloat("地图X偏移", &g_config.mapOffsetX, 0.0f, 500.0f);
        ImGui::SliderFloat("地图Y偏移", &g_config.mapOffsetY, 0.0f, 500.0f);
        ImGui::SliderFloat("ESP线宽", &g_config.espLineWidth, 1.0f, 5.0f);
    }

    ImGui::Separator();

    // ===== 网络设置 =====
    if (ImGui::CollapsingHeader("网络共享")) {
        ImGui::Checkbox("启用网络共享", &g_config.netEnabled);
        ImGui::Checkbox("作为服务端",   &g_config.isServer);
        ImGui::InputText("服务器IP", g_config.serverIP, sizeof(g_config.serverIP));
        ImGui::InputInt("端口", &g_config.serverPort);
    }

    ImGui::End();
}
