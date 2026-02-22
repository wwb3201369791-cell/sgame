# SGAME Refactor Engineering Book

## 1. Background and Objective

The current project has usable pieces (driver adapters, game reader, overlay, touch), but product flow is fragmented:

- No unified launcher flow for driver selection -> game PID -> draw control.
- Runtime controls are mixed into C++ UI and not script-friendly.
- Driver "open success" can still mean "read failure", causing fake availability.
- Debug and user control are hard to separate.

This refactor track builds a new productized workflow with clear boundaries:

1. `sh` launcher owns process orchestration and operator UX.
2. Core binary owns memory read, game parsing, and overlay draw.
3. Config and runtime state are explicit files.
4. Driver selection supports both manual and auto probe.

## 2. Scope and Non-Goals

### In scope

- Build a new refactor track under `refactor/`.
- Define a stable shell menu flow.
- Define core CLI/control protocol contract.
- Implement phase-1 launcher that already supports:
  - Driver selection
  - Wait/find game PID
  - Start/stop core process
  - Draw init/stop command placeholders
  - Status/log view

### Out of scope (for phase 1)

- Full rewrite of existing `jni/src` code.
- Final anti-detection or protection strategy.
- Full offset auto-update pipeline.

## 3. Design Principles

1. Product flow first:
   - Operator should finish all common actions from one menu.
2. Loose coupling:
   - Shell and core communicate via explicit contract.
3. Probe before trust:
   - Driver node visibility is not enough. Functionality probe is required.
4. Observable runtime:
   - Every decision (driver/pid/draw) must be inspectable through status and logs.
5. Incremental migration:
   - Reuse old code selectively, no blind copy from references.

## 4. Target Architecture

## 4.1 Layers

1. Launcher layer (`refactor/launcher`)
   - Human-facing menu.
   - Config and process lifecycle.
2. Contract layer (`refactor/protocol`)
   - CLI args and control command definitions.
3. Core layer (`refactor/core`)
   - New executable with modularized driver/game/overlay services.
4. Runtime layer (`refactor/runtime`, `refactor/logs`)
   - State file, pid file, logs, future control fifo/socket.

## 4.2 Data Flow

1. Operator selects settings in launcher.
2. Launcher writes config and starts core process.
3. Core loads config/args, probes driver, attaches game PID.
4. Core reads game state and renders overlay.
5. Launcher can send commands (future FIFO/socket) to change runtime behavior.

## 5. Module Plan

## 5.1 Launcher module

Planned menu capabilities:

1. Select driver mode:
   - `auto`, `qx`, `dev`, `proc`, `rt`, `dit`, `ditpro`, `pread`, `syscall`.
2. Set core binary path.
3. Wait/find game PID.
4. Start core.
5. Stop core.
6. Init draw (phase-1 placeholder, phase-2 real IPC command).
7. Stop draw (phase-1 placeholder, phase-2 real IPC command).
8. Show status.
9. Tail logs.
10. Exit.

## 5.2 Core module (planned)

Submodules:

- `driver_service`: adapter + probe + selection.
- `process_service`: game PID resolve and module base/BSS resolve.
- `game_service`: entity/monster/matrix parsing.
- `overlay_service`: EGL + touch + render pipeline.
- `control_service`: command intake (`INIT_DRAW`, `STOP_DRAW`, `EXIT`, ...).

## 5.3 Protocol module

Define:

- CLI args (example):
  - `--driver auto|dev|...`
  - `--game-pkg com.tencent.tmgp.sgame`
  - `--draw auto|off`
  - `--control-endpoint <path>`
- Control commands (line-based):
  - `INIT_DRAW`
  - `STOP_DRAW`
  - `SET_DRIVER <mode>`
  - `EXIT`

## 6. Configuration and Runtime State

## 6.1 Config file

Path:

- `refactor/conf/user.env`

Keys:

- `DRIVER_MODE`
- `GAME_PKG`
- `CORE_BIN`
- `AUTO_INIT_DRAW`
- `LOG_LEVEL`
- `EXTRA_ARGS`

## 6.2 Runtime files

Paths:

- `refactor/runtime/core.pid`
- `refactor/runtime/state.env`
- `refactor/logs/core.log`

`state.env` baseline keys:

- `CORE_RUNNING`
- `LAST_START_TS`
- `LAST_GAME_PID`
- `LAST_DRIVER`
- `LAST_ACTION`

## 7. State Machine (target)

Launcher state:

1. `IDLE`
2. `CONFIGURED`
3. `CORE_RUNNING`
4. `DRAW_ON`
5. `CORE_STOPPED`

Core state:

1. `BOOT`
2. `DRIVER_PROBE`
3. `WAIT_GAME`
4. `READY`
5. `DRAW_ON`
6. `DRAW_OFF`
7. `EXITING`

## 8. Milestones and Deliverables

## M0 (this commit)

- Engineering book.
- Refactor directory skeleton.
- Phase-1 launcher script.

## M1

- Core supports CLI config load.
- Driver mode forced from launcher config.
- Unified startup logs.

## M2

- Add control endpoint (FIFO/socket).
- Launcher `Init draw` and `Stop draw` become real runtime commands.

## M3

- Offset validation command in launcher.
- Render diagnostics switch (on/off).
- Better Chinese font fallback policy.

## M4

- Automated smoke test script.
- Stable release packaging.

## 9. Acceptance Criteria

Phase-1 acceptance:

1. Launcher can persist driver mode and core path.
2. Launcher can find game PID and display it.
3. Launcher can start core in background and track PID.
4. Launcher can stop core safely.
5. Logs and status are visible from menu.

Phase-2 acceptance:

1. `Init draw` and `Stop draw` operate without restarting core.
2. Runtime command results are visible in status/log.

## 10. Risk Register and Mitigation

1. Fake driver nodes:
   - Mitigation: mandatory read-back probe before selection.
2. Touch mapping differences by device orientation:
   - Mitigation: explicit orientation updates + alternative mapping switch.
3. Chinese font build instability:
   - Mitigation: minimal glyph subset + TTC font index fallback + English fallback UI.
4. Offset drift after game update:
   - Mitigation: add offset check tool and hard fail with clear diagnostics.

## 11. Coding Standards for Refactor Track

1. Shell scripts:
   - POSIX `sh` compatible.
   - No hidden side effects.
   - Every external action logs concise status.
2. Core C++:
   - Clear service boundaries.
   - No global state unless intentionally in runtime context.
   - All driver operations return explicit status.
3. Config/state:
   - Plain text `KEY=VALUE`.
   - No silent defaults without log output.

## 12. Immediate Build Order

1. Finalize launcher script behavior and menu UX.
2. Wire core startup flags to consume launcher config.
3. Implement control endpoint for draw toggles.
4. Replace temporary debug HUD with switchable diagnostics panel.

