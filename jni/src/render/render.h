#pragma once
#include "imgui.h"
#include "../game/GameStructs.h"
#include "../game/Offsets.h"
#include "../overlay/menu.h"
#include <cmath>
#include <cstdio>

// ============================================================
// 渲染系统 — 小地图、ESP、技能CD、视野警示、野怪计时
// ============================================================

namespace Render {

// 颜色定义
static const ImU32 COLOR_BLUE     = IM_COL32(60, 140, 255, 255);   // 蓝方
static const ImU32 COLOR_RED      = IM_COL32(255, 80, 80, 255);    // 红方
static const ImU32 COLOR_GREEN    = IM_COL32(60, 200, 60, 255);    // 血量/安全
static const ImU32 COLOR_YELLOW   = IM_COL32(255, 220, 60, 255);   // 警告
static const ImU32 COLOR_WHITE    = IM_COL32(255, 255, 255, 255);
static const ImU32 COLOR_BG       = IM_COL32(0, 0, 0, 120);       // 半透明黑
static const ImU32 COLOR_RECALL   = IM_COL32(180, 100, 255, 255);  // 回城紫色

// ===== 小地图 =====
inline void DrawMinimap(const GameState& gs) {
    if (!g_config.showMinimap) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    float scale = g_config.mapScale;
    float ox = g_config.mapOffsetX;
    float oy = g_config.mapOffsetY;

    // 小地图背景
    draw->AddRectFilled(
        ImVec2(ox, oy),
        ImVec2(ox + scale * 2, oy + scale * 2),
        COLOR_BG, 8.0f);

    // 绘制英雄点位
    for (int i = 0; i < gs.heroCount; i++) {
        const HeroData& h = gs.heroes[i];
        if (!h.valid || h.coordX == 0) continue;

        float mx = h.coordX * gs.orient * scale / 50000.0f + scale + ox;
        float my = h.coordY * gs.orient * scale / 50000.0f * -1.0f + scale + oy;

        ImU32 color = (h.camp == 1) ? COLOR_BLUE : COLOR_RED;

        // 圆点
        draw->AddCircleFilled(ImVec2(mx, my), 6.0f, color);

        // 英雄名
        const char* name = Offsets::GetHeroName(h.heroId);
        ImVec2 textSize = ImGui::CalcTextSize(name);
        draw->AddText(ImVec2(mx - textSize.x / 2, my - 18.0f), COLOR_WHITE, name);

        // 回城标记
        if (h.recall != 0) {
            draw->AddCircle(ImVec2(mx, my), 10.0f, COLOR_RECALL, 0, 2.0f);
        }
    }

    // 绘制野怪点位
    for (int i = 0; i < gs.monsterCount; i++) {
        const MonsterData& m = gs.monsters[i];
        if (!m.valid) continue;

        int cx = (m.hp > 0) ? m.coordX : m.spawnX;
        int cy = (m.hp > 0) ? m.coordY : m.spawnY;
        if (cx == 0 && cy == 0) continue;

        float mx = cx * gs.orient * scale / 50000.0f + scale + ox;
        float my = cy * gs.orient * scale / 50000.0f * -1.0f + scale + oy;

        ImU32 color = (m.hp > 0) ? COLOR_GREEN : COLOR_YELLOW;
        draw->AddRectFilled(
            ImVec2(mx - 3, my - 3),
            ImVec2(mx + 3, my + 3), color);
    }
}

// ===== ESP 方框 =====
inline void DrawESP(const GameState& gs) {
    if (!g_config.showESP) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    float lineW = g_config.espLineWidth;

    for (int i = 0; i < gs.heroCount; i++) {
        const HeroData& h = gs.heroes[i];
        if (!h.valid || h.screen.X < 0) continue;
        if (h.camp == gs.myCamp) continue;  // 只画敌方

        float x = h.screen.X;
        float y = h.screen.Y;
        float boxH = fabsf(h.screen.H) * 2.0f;
        if (boxH < 20.0f) boxH = 80.0f;
        float boxW = boxH * 0.6f;

        ImU32 color = (h.camp == 1) ? COLOR_BLUE : COLOR_RED;

        // 方框
        draw->AddRect(
            ImVec2(x - boxW / 2, y - boxH),
            ImVec2(x + boxW / 2, y),
            color, 0, 0, lineW);

        // 血条
        if (h.maxHp > 0) {
            float hpRatio = (float)h.hp / (float)h.maxHp;
            float barW = boxW;
            float barH = 4.0f;
            float barY = y + 4.0f;

            draw->AddRectFilled(
                ImVec2(x - barW / 2, barY),
                ImVec2(x + barW / 2, barY + barH),
                IM_COL32(60, 60, 60, 180));
            draw->AddRectFilled(
                ImVec2(x - barW / 2, barY),
                ImVec2(x - barW / 2 + barW * hpRatio, barY + barH),
                COLOR_GREEN);
        }

        // 英雄名
        const char* name = Offsets::GetHeroName(h.heroId);
        ImVec2 ts = ImGui::CalcTextSize(name);
        draw->AddText(ImVec2(x - ts.x / 2, y - boxH - 18.0f), COLOR_WHITE, name);
    }
}

// ===== 技能CD显示 =====
inline void DrawSkillCD(const GameState& gs) {
    if (!g_config.showSkillCD) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    for (int i = 0; i < gs.heroCount; i++) {
        const HeroData& h = gs.heroes[i];
        if (!h.valid || h.camp == gs.myCamp) continue;
        if (h.screen.X < 0) continue;

        if (h.summonerCd > 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s: %ds",
                Offsets::GetSummonerName(h.summonerId), h.summonerCd);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            draw->AddText(
                ImVec2(h.screen.X - ts.x / 2, h.screen.Y + 12.0f),
                COLOR_YELLOW, buf);
        }
    }
}

// ===== 视野警示 =====
inline void DrawVisionAlert(const GameState& gs) {
    if (!g_config.showVisionAlert) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 检查自身是否暴露在敌方视野中
    for (int i = 0; i < gs.heroCount; i++) {
        const HeroData& h = gs.heroes[i];
        if (!h.valid || h.camp != gs.myCamp) continue;

        if (h.vision == Offsets::Entity::VisionVisible) {
            // 屏幕边缘红色警告
            draw->AddRect(
                ImVec2(5, 5),
                ImVec2(g_screenW - 5, g_screenH - 5),
                IM_COL32(255, 0, 0, 100), 0, 0, 6.0f);

            // 顶部文字
            const char* warn = "⚠ 你已暴露在敌方视野中!";
            ImVec2 ts = ImGui::CalcTextSize(warn);
            draw->AddText(
                ImVec2(g_screenW / 2 - ts.x / 2, 20.0f),
                IM_COL32(255, 60, 60, 255), warn);
            break;
        }
    }
}

// ===== 野怪计时 =====
inline void DrawMonsterTimer(const GameState& gs) {
    if (!g_config.showMonsterCD) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 在小地图旁显示野怪刷新倒计时
    float startY = g_config.mapOffsetY + g_config.mapScale * 2 + 10.0f;
    float startX = g_config.mapOffsetX;

    for (int i = 0; i < gs.monsterCount; i++) {
        const MonsterData& m = gs.monsters[i];
        if (!m.valid || m.hp > 0 || m.cd <= 0) continue;

        char buf[64];
        snprintf(buf, sizeof(buf), "野怪#%d: %ds / %ds", m.id, m.cd, m.maxCd);
        draw->AddText(ImVec2(startX, startY), COLOR_YELLOW, buf);
        startY += 20.0f;
    }
}

// ===== 主绘制入口 =====
inline void DrawAll(const GameState& gs) {
    DrawMinimap(gs);
    DrawESP(gs);
    DrawSkillCD(gs);
    DrawVisionAlert(gs);
    DrawMonsterTimer(gs);
}

} // namespace Render
