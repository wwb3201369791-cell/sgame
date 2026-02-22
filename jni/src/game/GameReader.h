#pragma once
#include "Offsets.h"
#include "GameStructs.h"
#include "../driver/memory.h"
#include <cstdio>
#include <cstring>

// ============================================================
// GameReader — 封装所有内存读取链
// ============================================================

class GameReader {
public:
    uintptr_t bssBase    = 0;   // libGameCore.so:bss 基址
    uintptr_t il2cppBase = 0;   // libil2cpp.so 基址 (矩阵用)
    int       debugMatchRaw = 0;
    uintptr_t debugEntityListHead = 0;
    uintptr_t debugMonsterListHead = 0;
    float     debugMatrix0 = 0.0f;

    GameState state;

    // 初始化: 定位 bss 基址
    bool Init() {
        // 方法1: 通过 /proc/pid/maps 找 libGameCore.so 的 BSS 段 (最后一个rw-p映射)
        bssBase = get_bss_base("libGameCore.so");
        if (bssBase != 0) {
            printf("[GameReader] ✓ BSS基址 (rw-p映射): 0x%lX\n", bssBase);
        } else {
            // 方法2: 回退到text段基址 (不准确,但至少不为0)
            bssBase = get_module_base("libGameCore.so");
            if (bssBase != 0) {
                printf("[GameReader] ⚠ 仅找到text基址: 0x%lX (BSS偏移可能不准)\n", bssBase);
            }
        }
        if (bssBase == 0) {
            printf("[GameReader] ✗ 无法定位 libGameCore.so\n");
            return false;
        }

        // il2cpp BSS (矩阵) — 也优先找 BSS 段
        uintptr_t il2cppBss = get_bss_base("libil2cpp.so");
        if (il2cppBss != 0) {
            il2cppBase = il2cppBss;
            printf("[GameReader] ✓ il2cpp BSS: 0x%lX\n", il2cppBase);
        } else {
            il2cppBase = get_module_base("libil2cpp.so");
            if (il2cppBase != 0) {
                printf("[GameReader] ⚠ il2cpp text基址: 0x%lX\n", il2cppBase);
            }
        }

        return true;
    }

    // 每帧调用: 读取所有数据
    void Update() {
        memset(&state, 0, sizeof(state));
        debugEntityListHead = 0;
        debugMonsterListHead = 0;
        debugMatrix0 = 0.0f;

        // 1. 判断是否在对局中
        debugMatchRaw = read_int(bssBase + Offsets::BSS::MatchState);
        state.inMatch = (debugMatchRaw != 0);
        if (!state.inMatch) return;

        // 2. 读取矩阵
        ReadMatrix();

        // 3. 判断阵营
        float firstMatrixVal = state.matrix[0];
        state.foeCamp = (firstMatrixVal > 0) ? 2 : 1;
        state.myCamp  = (state.foeCamp == 2) ? 1 : 2;
        state.orient  = (state.foeCamp == 1) ? -1 : 1;

        // 4. 读取英雄列表
        ReadHeroes();

        // 5. 读取野怪列表
        ReadMonsters();

        // 6. 读取控制状态
        ReadControlState();

        // 7. 计算屏幕坐标 (WorldToScreen)
        for (int i = 0; i < state.heroCount; i++) {
            if (state.heroes[i].valid) {
                state.heroes[i].screen = WorldToScreen(
                    state.heroes[i].coordX, state.heroes[i].coordY, state.matrix);
            }
        }
        for (int i = 0; i < state.monsterCount; i++) {
            if (state.monsters[i].valid) {
                state.monsters[i].screen = WorldToScreen(
                    state.monsters[i].coordX, state.monsters[i].coordY, state.matrix);
            }
        }
    }

private:
    // 安全链式读取 (任何一环为0则返回0)
    uintptr_t ReadChain(uintptr_t base, const uint32_t offsets[], int count) {
        uintptr_t ptr = base;
        for (int i = 0; i < count; i++) {
            ptr = (uintptr_t)read_long(ptr + offsets[i]);
            if (ptr == 0) return 0;
        }
        return ptr;
    }

    void ReadMatrix() {
        if (il2cppBase == 0) return;

        // il2cpp_bss + 0x4712B8 → +0xB8 → +0x0 → +0x10 → +0x128
        uintptr_t temp = (uintptr_t)read_long(il2cppBase + Offsets::Matrix::IL2CPP_BSS);
        if (temp == 0) return;
        temp = (uintptr_t)read_long(temp + Offsets::Matrix::Chain1);
        if (temp == 0) return;
        temp = (uintptr_t)read_long(temp + Offsets::Matrix::Chain2);
        if (temp == 0) return;
        temp = (uintptr_t)read_long(temp + Offsets::Matrix::Chain3);
        if (temp == 0) return;

        uintptr_t matAddr = temp + Offsets::Matrix::MatrixStart;
        if (g_driver) {
            g_driver->read(matAddr, state.matrix, sizeof(float) * 16);
            debugMatrix0 = state.matrix[0];
        }
    }

    void ReadHeroes() {
        uintptr_t listHead = (uintptr_t)read_long(bssBase + Offsets::BSS::EntityList);
        debugEntityListHead = listHead;
        if (listHead == 0) return;

        // 自身实体
        uintptr_t selfPtr = (uintptr_t)read_long(
            (uintptr_t)read_long(listHead + Offsets::List::SelfPtr) + Offsets::List::SelfDeref);

        // 数组基址
        uintptr_t arrayBase = listHead + Offsets::List::ArrayBase;

        state.heroCount = 0;
        for (int i = 0; i < 20 && state.heroCount < 20; i++) {
            uintptr_t slot = (uintptr_t)read_long(arrayBase + i * Offsets::List::Stride);
            if (slot == 0) continue;

            uintptr_t entity = (uintptr_t)read_long(slot + Offsets::List::EntityDeref);
            if (entity == 0) continue;

            HeroData& hero = state.heroes[state.heroCount];
            hero.valid  = true;
            hero.heroId = read_int(entity + Offsets::Entity::HeroId);
            hero.camp   = read_int(entity + Offsets::Entity::Camp);

            // 血量
            uintptr_t hpPtr = (uintptr_t)read_long(entity + Offsets::Entity::HpPtr);
            if (hpPtr != 0) {
                hero.hp    = read_int(hpPtr + Offsets::Entity::HpCur);
                hero.maxHp = read_int(hpPtr + Offsets::Entity::HpMax);
            }

            // 坐标
            uintptr_t c1 = (uintptr_t)read_long(entity + Offsets::Entity::CoordChain1);
            if (c1 != 0) {
                uintptr_t c2 = (uintptr_t)read_long(c1 + Offsets::Entity::CoordChain2);
                if (c2 != 0) {
                    uintptr_t coord = (uintptr_t)read_long(c2 + Offsets::Entity::CoordChain3);
                    if (coord != 0) {
                        hero.coordX = read_int(coord + Offsets::Entity::CoordX);
                        hero.coordY = read_int(coord + Offsets::Entity::CoordY);
                    }
                }
            }

            // 技能CD
            uintptr_t sk1 = (uintptr_t)read_long(entity + Offsets::Entity::SkillChain1);
            if (sk1 != 0) {
                uintptr_t sk2 = (uintptr_t)read_long(sk1 + Offsets::Entity::SkillChain2);
                if (sk2 != 0) {
                    uintptr_t cdPtr = (uintptr_t)read_long(sk2 + Offsets::Entity::SummonerCdPtr);
                    if (cdPtr != 0) {
                        hero.summonerCd = read_int(cdPtr + Offsets::Entity::SummonerCdVal) / Offsets::Entity::CdDivisor;
                    }
                    uintptr_t idPtr = (uintptr_t)read_long(sk2 + Offsets::Entity::SummonerIdPtr);
                    if (idPtr != 0) {
                        hero.summonerId = read_int(idPtr + Offsets::Entity::SummonerIdVal);
                    }
                }
            }

            // 回城状态
            uintptr_t r1 = (uintptr_t)read_long(entity + Offsets::Entity::RecallChain1);
            if (r1 != 0) {
                uintptr_t r2 = (uintptr_t)read_long(r1 + Offsets::Entity::RecallChain2);
                if (r2 != 0) {
                    uintptr_t r3 = (uintptr_t)read_long(r2 + Offsets::Entity::RecallChain3);
                    if (r3 != 0) {
                        hero.recall = read_int(r3 + Offsets::Entity::RecallValue);
                    }
                }
            }

            // 视野
            uintptr_t v1 = (uintptr_t)read_long(entity + Offsets::Entity::VisionChain1);
            if (v1 != 0) {
                uintptr_t v2 = (uintptr_t)read_long(v1 + Offsets::Entity::VisionChain2);
                if (v2 != 0) {
                    if (hero.camp == 2) {
                        hero.vision = read_int(v2 + Offsets::Entity::VisionExpose);
                    } else {
                        hero.vision = read_int(v2 + Offsets::Entity::VisionBlue);
                    }
                }
            }

            state.heroCount++;
        }
    }

    void ReadMonsters() {
        uintptr_t listHead = (uintptr_t)read_long(bssBase + Offsets::BSS::MonsterList);
        debugMonsterListHead = listHead;
        if (listHead == 0) return;

        // BuffAddress = Read(Read(Read(listHead+0x3B8)+0x88)+0x120)
        uintptr_t b1 = (uintptr_t)read_long(listHead + Offsets::Monster::BuffChain1);
        if (b1 == 0) return;
        uintptr_t b2 = (uintptr_t)read_long(b1 + Offsets::Monster::BuffChain2);
        if (b2 == 0) return;
        uintptr_t buffAddr = (uintptr_t)read_long(b2 + Offsets::Monster::BuffChain3);
        if (buffAddr == 0) return;

        state.monsterCount = 0;
        for (int i = 0; i < (int)Offsets::Monster::MaxCount && state.monsterCount < 24; i++) {
            uintptr_t slot = (uintptr_t)read_long(buffAddr + i * Offsets::Monster::Stride);
            if (slot == 0) continue;

            MonsterData& mon = state.monsters[state.monsterCount];
            mon.valid = true;
            mon.id = read_int(slot + Offsets::Monster::MonsterId);

            uintptr_t entityPtr = (uintptr_t)read_long(slot + Offsets::Monster::EntityPtr);

            // 坐标
            if (entityPtr != 0) {
                uintptr_t c1 = (uintptr_t)read_long(entityPtr + Offsets::Monster::CoordChain1);
                if (c1 != 0) {
                    uintptr_t c2 = (uintptr_t)read_long(c1 + Offsets::Monster::CoordChain2);
                    if (c2 != 0) {
                        uintptr_t coord = (uintptr_t)read_long(c2 + Offsets::Monster::CoordChain3);
                        if (coord != 0) {
                            mon.coordX = read_int(coord + 0x0);
                            mon.coordY = read_int(coord + 0x8);
                        }
                    }
                }

                // 血量
                uintptr_t hpPtr = (uintptr_t)read_long(entityPtr + Offsets::Monster::HpPtr);
                if (hpPtr != 0) {
                    mon.hp    = read_int(hpPtr + Offsets::Monster::HpCur);
                    mon.maxHp = read_int(hpPtr + Offsets::Monster::HpMax);
                }
            }

            // 刷新时间
            mon.cd    = read_int(slot + Offsets::Monster::CdCurrent) / 1000;
            mon.maxCd = read_int(slot + Offsets::Monster::CdMax) / 1000;

            // 固定出生坐标
            mon.spawnX = read_int(slot + Offsets::Monster::SpawnX);
            mon.spawnY = read_int(slot + Offsets::Monster::SpawnY);

            state.monsterCount++;
        }
    }

    void ReadControlState() {
        // Read(Read(Read(Read(Read(bss+0x2540)+0x48)+0xD8)+0x108)+0x110)+0x258
        uintptr_t ptr = (uintptr_t)read_long(bssBase + Offsets::BSS::ControlState);
        if (ptr == 0) return;
        ptr = (uintptr_t)read_long(ptr + Offsets::Control::Chain1);
        if (ptr == 0) return;
        ptr = (uintptr_t)read_long(ptr + Offsets::Control::Chain2);
        if (ptr == 0) return;
        ptr = (uintptr_t)read_long(ptr + Offsets::Control::Chain3);
        if (ptr == 0) return;
        ptr = (uintptr_t)read_long(ptr + Offsets::Control::Chain4);
        if (ptr == 0) return;

        state.controlState = read_int(ptr + Offsets::Control::Value);
    }
};
