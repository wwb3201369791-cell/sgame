# Control Protocol (Planned)

This is the planned runtime control contract between the shell launcher and the future core process.

## Transport (planned)

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

## Expected responses (planned)

```text
OK PONG
OK DRAW_ON
OK DRAW_OFF
OK EXITING
ERR UNSUPPORTED_COMMAND
ERR INVALID_ARG
```

## Phase-1 behavior

The launcher already exposes menu items for draw control, but the actual IPC transport is not implemented yet.
Until the core control endpoint is implemented, those menu actions log a placeholder message and keep state for operator workflow consistency.

