#pragma once
#include <cstdint>

// ============================================================
// 游戏内存偏移配置 — 王者荣耀 libGameCore.so
// 来源: offset_mapping.md + Ghidra 分析 + 参考源码
// ============================================================

namespace Offsets {

    // =====================================================
    // BSS 全局偏移 (相对于 libGameCore.so:bss)
    // =====================================================
    namespace BSS {
        constexpr uint32_t MatchState    = 0x256C;   // 对局判断 (非0=对局中)
        constexpr uint32_t EntityList    = 0x25C8;   // 实体列表头 (核心基址)
        constexpr uint32_t ControlState  = 0x2540;   // 控制状态链入口
        constexpr uint32_t MonsterList   = 0x1E18;   // 野怪列表头

        // TODO: 需动态验证 (旧版偏移在新版中无效)
        constexpr uint32_t MinionList    = 0x0;      // 兵线列表头 (旧版0x165840)
        constexpr uint32_t WardList      = 0x0;      // 眼位/扩展实体 (旧版0x18CE40)
    }

    // =====================================================
    // 实体列表遍历
    // =====================================================
    namespace List {
        constexpr uint32_t SelfPtr       = 0x48;     // +0x48 → 自身实体
        constexpr uint32_t SelfDeref     = 0xD8;     // → +0xD8 得到自身entity
        constexpr uint32_t ArrayBase     = 0x120;    // +0x120 = 实体数组基址
        constexpr uint32_t Stride        = 0x18;     // 数组步长
        constexpr uint32_t EntityDeref   = 0x68;     // Read(Read(base + i*0x18) + 0x68)
    }

    // =====================================================
    // 英雄/实体属性偏移 (相对于 entity 指针)
    // =====================================================
    namespace Entity {
        constexpr uint32_t HeroId        = 0x30;     // int: 英雄ID
        constexpr uint32_t Camp          = 0x3C;     // int: 阵营 (1蓝/2红)
        constexpr uint32_t ValidMark     = 0xCD;     // byte: 实体有效标记
        constexpr uint32_t StatusBit     = 0x17;     // byte: 实体状态位

        // 血量指针链: Read(entity + 0x168) → hp_ptr
        constexpr uint32_t HpPtr         = 0x168;    // → HP结构体指针
        constexpr uint32_t HpCur         = 0xA8;     // hp_ptr + 0xA8 → 当前血量
        constexpr uint32_t HpMax         = 0xB0;     // hp_ptr + 0xB0 → 最大血量

        // 坐标链: Read(Read(Read(entity+0x248)+0x10))+0x10
        constexpr uint32_t CoordChain1   = 0x248;
        constexpr uint32_t CoordChain2   = 0x10;
        constexpr uint32_t CoordChain3   = 0x10;     // → coord_ptr
        constexpr uint32_t CoordX        = 0x00;     // coord_ptr + 0x00
        constexpr uint32_t CoordY        = 0x08;     // coord_ptr + 0x08

        // 技能CD链: Read(Read(entity+0x150)+0x150) → skill_base
        constexpr uint32_t SkillChain1   = 0x150;
        constexpr uint32_t SkillChain2   = 0x150;
        constexpr uint32_t SummonerCdPtr = 0xF8;     // skill_base + 0xF8 → +0x3C / 8192000
        constexpr uint32_t SummonerCdVal = 0x3C;
        constexpr uint32_t SummonerIdPtr = 0xC8;     // skill_base + 0xC8 → +0x10
        constexpr uint32_t SummonerIdVal = 0x10;
        constexpr int      CdDivisor     = 8192000;

        // 回城状态: Read(Read(Read(Read(entity+0x150)+0x168)+0x168)+0x20)
        constexpr uint32_t RecallChain1  = 0x150;
        constexpr uint32_t RecallChain2  = 0x168;
        constexpr uint32_t RecallChain3  = 0x168;
        constexpr uint32_t RecallValue   = 0x20;

        // 视野判断: Read(Read(entity+0x260)+0x68) → vision_ptr
        constexpr uint32_t VisionChain1  = 0x260;
        constexpr uint32_t VisionChain2  = 0x68;
        constexpr uint32_t VisionBlue    = 0x38;     // 蓝方视野
        constexpr uint32_t VisionRed     = 0x10;     // 红方视野
        constexpr uint32_t VisionExpose  = 0x18;     // 暴露标记 (red)
        constexpr int      VisionVisible = 257;      // == 257 表示已暴露
    }

    // =====================================================
    // 野怪结构偏移
    // =====================================================
    namespace Monster {
        constexpr uint32_t BuffChain1    = 0x3B8;    // temp2 + 0x3B8
        constexpr uint32_t BuffChain2    = 0x88;     // → +0x88
        constexpr uint32_t BuffChain3    = 0x120;    // → +0x120 = BuffAddress
        constexpr uint32_t Stride        = 0x18;     // 遍历步长
        constexpr uint32_t MaxCount      = 23;       // 最大野怪数量

        constexpr uint32_t EntityPtr     = 0x3A0;    // monster_base + 0x3A0 → entity_ptr
        constexpr uint32_t MonsterId     = 0xC0;     // monster_base + 0xC0 → 野怪ID

        // 坐标: Read(Read(Read(entity_ptr+0x230)+0x60)+0x10) → coord_ptr
        constexpr uint32_t CoordChain1   = 0x230;
        constexpr uint32_t CoordChain2   = 0x60;
        constexpr uint32_t CoordChain3   = 0x10;

        // 血量: Read(entity_ptr+0x168) → +0xA8/+0xB0
        constexpr uint32_t HpPtr         = 0x168;
        constexpr uint32_t HpCur         = 0xA8;
        constexpr uint32_t HpMax         = 0xB0;

        // 刷新时间
        constexpr uint32_t CdCurrent     = 0x240;    // / 1000
        constexpr uint32_t CdMax         = 0x1E4;    // / 1000

        // 固定出生坐标
        constexpr uint32_t SpawnX        = 0x2B8;
        constexpr uint32_t SpawnY        = 0x2C0;
    }

    // =====================================================
    // 矩阵偏移 (libil2cpp.so:bss)
    // =====================================================
    namespace Matrix {
        constexpr uint32_t IL2CPP_BSS    = 0x4712B8; // il2cpp_bss起始偏移
        constexpr uint32_t Chain1        = 0xB8;
        constexpr uint32_t Chain2        = 0x00;
        constexpr uint32_t Chain3        = 0x10;
        constexpr uint32_t MatrixStart   = 0x128;    // 16个float的起始
    }

    // =====================================================
    // 控制状态
    // =====================================================
    namespace Control {
        // Read(Read(Read(Read(Read(bss+0x2540)+0x48)+0xD8)+0x108)+0x110)+0x258
        constexpr uint32_t Chain1 = 0x48;
        constexpr uint32_t Chain2 = 0xD8;
        constexpr uint32_t Chain3 = 0x108;
        constexpr uint32_t Chain4 = 0x110;
        constexpr uint32_t Value  = 0x258;
        // 0=正常, 1=沉默, 2=眩晕, 3=击飞, 4=冰冻
    }

    // =====================================================
    // 召唤师技能ID映射
    // =====================================================
    inline const char* GetSummonerName(int id) {
        switch(id) {
            case 80102: return "治疗";
            case 80103: return "晕眩";
            case 80104: case 80116: return "惩戒";
            case 80105: return "干扰";
            case 80107: return "净化";
            case 80108: return "斩杀";
            case 80109: return "疾跑";
            case 80110: return "狂暴";
            case 80115: return "闪现";
            case 80121: return "弱化";
            default: return "未知";
        }
    }

    // =====================================================
    // 英雄ID→中文名映射
    // =====================================================
    inline const char* GetHeroName(int id) {
        switch(id) {
            case 105: return "廉颇";    case 106: return "小乔";
            case 107: return "赵云";    case 108: return "墨子";
            case 109: return "妲己";    case 110: return "嬴政";
            case 111: return "孙尚香";  case 112: return "鲁班七号";
            case 113: return "庄周";    case 114: return "刘禅";
            case 115: return "高渐离";  case 116: return "阿轲";
            case 117: return "钟无艳";  case 118: return "孙膑";
            case 119: return "扁鹊";    case 120: return "白起";
            case 121: return "芈月";    case 123: return "吕布";
            case 124: return "周瑜";    case 125: return "元歌";
            case 126: return "老夫子";  case 127: return "亚瑟";
            case 128: return "孙悟空";  case 129: return "牛魔";
            case 130: return "后裔";    case 131: return "鲁班大师";
            case 132: return "达摩";    case 133: return "甄姬";
            case 134: return "曹操";    case 135: return "典韦";
            case 136: return "宫本武藏"; case 137: return "李白";
            case 139: return "马可波罗"; case 140: return "狄仁杰";
            case 141: return "张飞";    case 142: return "李元芳";
            case 144: return "刘邦";    case 146: return "姜子牙";
            case 148: return "花木兰";  case 149: return "兰陵王";
            case 150: return "韩信";    case 152: return "王昭君";
            case 153: return "诸葛亮";  case 154: return "黄忠";
            case 155: return "大乔";    case 156: return "东皇太一";
            case 157: return "太乙真人"; case 159: return "哪吒";
            case 162: return "娜可露露"; case 163: return "雅典娜";
            case 166: return "杨戬";    case 167: return "成吉思汗";
            case 168: return "关羽";    case 169: return "铠";
            case 501: return "明世隐";  case 502: return "裴擒虎";
            case 503: return "公孙离";  case 504: return "弈星";
            case 505: return "沈梦溪";  case 506: return "杨玉环";
            case 507: return "百里守约"; case 508: return "百里玄策";
            case 509: return "苏烈";    case 510: return "狂铁";
            case 511: return "米莱迪";  case 513: return "盘古";
            case 515: return "瑶";      case 517: return "马超";
            case 519: return "鲁班大师"; case 521: return "蒙恬";
            case 522: return "镜";      case 523: return "阿古朵";
            case 524: return "澜";      case 525: return "司空震";
            case 527: return "夏洛特";  case 528: return "云缨";
            case 529: return "艾琳";    case 531: return "暃";
            case 533: return "金蝉";    case 534: return "桑启";
            case 536: return "海月";    case 537: return "海诺";
            case 538: return "戈娅";    case 540: return "莱西奥";
            case 542: return "姬小满";  case 544: return "赵怀真";
            case 545: return "云中君";  case 548: return "朵莉亚";
            case 558: return "空空儿";  case 563: return "少司缘";
            case 564: return "敖隐";    case 577: return "太华";
            case 582: return "元流之子"; case 620: return "盾山";
            default: return "未知";
        }
    }
}
