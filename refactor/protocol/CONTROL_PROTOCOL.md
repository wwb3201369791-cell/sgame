# Control Protocol (M2 File Queue / Future IPC)

This is the planned runtime control contract between the shell launcher and the future core process.

## Transport

### Current (M2 implemented)

- File queue (launcher appends commands)
- Default file:
  - `refactor/runtime/control.cmd`
- ACK/result file (core appends results):
  - `refactor/runtime/control.ack`

### Future (planned)

- Unix domain socket or FIFO at a configurable endpoint
- Default endpoint:
  - `/data/local/tmp/sgame_refactor.ctrl`

## Command format

- One command per line
- ASCII text
- Optional arguments separated by spaces

Examples:

```text
PING
INIT_DRAW
STOP_DRAW
EXIT
SET_DRIVER dev
SET_LOG_LEVEL debug
```

## Expected responses

```text
OK PONG
OK DRAW_ON
OK DRAW_OFF
OK EXITING
ERR UNSUPPORTED_COMMAND
ERR INVALID_ARG
```

## M2 file queue behavior (current)

- Launcher writes one command line to `control.cmd`
- Core polls the file, consumes only new sequence numbers
- Core writes ACK lines to `control.ack`
- Queue reset is supported (core detects sequence rollback and resyncs)

ACK examples:

```text
2|2026-02-22 21:10:09|OK|DRAW_ON
3|2026-02-22 21:10:12|OK|DRAW_OFF
4|2026-02-22 21:10:15|ERR|UNSUPPORTED_COMMAND
```

Supported commands in M2:

- `PING`
- `INIT_DRAW`
- `STOP_DRAW`
- `EXIT`
- `SET_LOG_LEVEL <level>` (accepted but currently only logged)

## Phase-1 note (legacy)

The launcher already exposes menu items for draw control, but the actual IPC transport is not implemented yet.
Until the core control endpoint is implemented, those menu actions log a placeholder message and keep state for operator workflow consistency.

Phase-1 queue file (current temporary implementation):

- File: `refactor/runtime/control.cmd`
- Format: `SEQ|TIMESTAMP|CMD`

Examples:

```text
1|2026-02-22 21:10:03|INIT_DRAW
2|2026-02-22 21:10:08|STOP_DRAW
```
