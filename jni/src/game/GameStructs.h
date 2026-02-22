#pragma once
#include <cstdint>
#include <cmath>

// ============================================================
// 游戏数据结构定义
// ============================================================

// 屏幕分辨率 (运行时由overlay设置)
extern float g_screenW;
extern float g_screenH;

// 屏幕坐标
struct ScreenCoord {
    float X = -1, Y = -1, W = -1, H = -1;
};

// 英雄数据 (每帧从内存读取后填充)
struct HeroData {
    int   heroId   = 0;       // 英雄ID
    int   camp     = 0;       // 阵营 1=蓝 2=红
    int   hp       = 0;       // 当前血量
    int   maxHp    = 0;       // 最大血量
    int   coordX   = 0;       // 世界坐标X (原始int)
    int   coordY   = 0;       // 世界坐标Y (原始int)
    int   summonerCd = 0;     // 召唤师技能CD (秒)
    int   summonerId = 0;     // 召唤师技能ID
    int   recall     = 0;     // 回城状态 (非0=回城中)
    int   vision     = 0;     // 视野暴露值 (257=暴露)
    bool  valid      = false; // 实体是否有效
    ScreenCoord screen;       // 屏幕投影坐标
};

// 野怪数据
struct MonsterData {
    int   id       = 0;       // 野怪ID
    int   hp       = 0;       // 当前血量
    int   maxHp    = 0;       // 最大血量
    int   coordX   = 0;       // 世界坐标X
    int   coordY   = 0;       // 世界坐标Y
    int   spawnX   = 0;       // 固定出生X
    int   spawnY   = 0;       // 固定出生Y
    int   cd       = 0;       // 当前CD (秒)
    int   maxCd    = 0;       // 最大CD (秒)
    bool  valid    = false;
    ScreenCoord screen;
};

// 兵线数据 (TODO: 待偏移确认后启用)
struct MinionData {
    int   camp     = 0;
    int   coordX   = 0;
    int   coordY   = 0;
    bool  valid    = false;
    ScreenCoord screen;
};

// 完整游戏状态 (用于网络传输和渲染)
struct GameState {
    bool      inMatch   = false;       // 是否在对局中
    int       myCamp    = 0;           // 自身阵营
    int       foeCamp   = 0;           // 敌方阵营
    int       orient    = 1;           // 地图方向 (-1 or 1)

    HeroData    heroes[20]  = {};      // 最多20个英雄 (10v10)
    int         heroCount   = 0;

    MonsterData monsters[24] = {};     // 最多24个野怪
    int         monsterCount = 0;

    MinionData  minions[50]  = {};     // 兵线 (TODO: 待偏移确认)
    int         minionCount  = 0;

    float       matrix[16]   = {};     // 4x4 投影矩阵
    int         controlState = 0;      // 自身控制状态
};

// 世界坐标 → 屏幕坐标 投射 (基于 offset_mapping.md 的公式)
inline ScreenCoord WorldToScreen(int worldX, int worldY, const float M[16]) {
    ScreenCoord out;
    float XM = worldX / 1000.0f;
    float ZM = worldY / 1000.0f;

    float radio = fabsf(ZM * M[11] + M[15]);
    if (radio < 0.001f) return out;

    out.X = g_screenW / 2.0f + (XM * M[0] + M[12]) / radio * g_screenW / 2.0f;
    out.Y = g_screenH / 2.0f - (ZM * M[9] + M[13]) / radio * g_screenH / 2.0f;
    float W = g_screenH / 2.0f - (XM * M[1] + 4.0f * M[5] + ZM * M[9] + M[13]) / radio * g_screenH / 2.0f;
    out.H = (out.Y - W) / 2.0f;
    out.W = W;

    return out;
}
