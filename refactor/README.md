# Refactor Track

This directory contains the new productized workflow track:

- `ENGINEERING_BOOK.md`: architecture, milestones, and acceptance criteria
- `launcher/`: shell-based operator menu
- `conf/`: persisted launcher config
- `runtime/`: pid/state files
- `logs/`: runtime logs
- `protocol/`: shell <-> core contract docs
- `core/`: future core refactor implementation

Phase 1 delivers the launcher scaffold and contracts. The existing `jni/` project remains the execution backend until the refactor core is ready.

