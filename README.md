# sgame

`sgame` 是一个基于 Android NDK 的原生 C++ 项目，按 `step0 ~ step9` 计划逐步实现以下能力：

- 多驱动内存读写抽象与自动检测
- 游戏状态读取（英雄、野怪、矩阵、控制状态）
- EGL 透明覆盖层 + ImGui 可视化菜单
- 小地图/ESP/技能CD/视野警示/野怪计时绘制
- TCP 局内状态共享（服务端/客户端）

当前代码位于 `jni/`，可直接通过 `ndk-build` 构建 `arm64-v8a` 可执行文件 `hphphp`。

## 目录结构

```text
jni/
├── include/
│   ├── ANativeWindowCreator.h
│   ├── Android_touch/
│   └── ImGui/
├── src/
│   ├── main.cpp
│   ├── driver/      # 驱动抽象与自动检测
│   ├── game/        # 偏移、结构体、内存读取
│   ├── overlay/     # EGL + ImGui 初始化与菜单
│   ├── render/      # 小地图/ESP等绘制
│   ├── net/         # TCP 状态共享
│   ├── stealth/     # 进程伪装与反调试
│   ├── Android_touch/
│   └── ImGui/
├── Android.mk
└── Application.mk
```

## 构建环境

- Android NDK（已验证路径示例：`$HOME/android-ndk/ndk/29.0.0-beta1`）
- ABI：`arm64-v8a`
- 平台：`android-28`

## 构建命令（Termux）

```bash
cd ~/storage/sgame
export NDK=$HOME/android-ndk/ndk/29.0.0-beta1
$NDK/ndk-build
```

## 运行流程（代码层）

`jni/src/main.cpp` 的主流程：

1. 初始化 stealth 模块
2. 初始化并自动检测驱动
3. 等待目标进程出现
4. 初始化 `GameReader`
5. 初始化 `overlay`（EGL + ImGui）
6. 进入主循环（读取、网络共享、渲染、UI）

## 当前状态与待办

- 已完成：
  - 核心模块串联与基础构建
  - Overlay 初始化兼容修复（命名空间/接口参数）
  - 网络模块稳定性修复（完整收发、失败清理）
  - EGL 初始化失败路径回滚
- 待完善：
  - 兵线相关偏移仍为 TODO
  - 偏移表需持续随版本校验
  - 更完整的真机构建与回归脚本

