#!/usr/bin/env bash
set -euo pipefail

# Host-side helper:
# Push the refactor launcher track to an Android device via adb.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REFACTOR_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ADB_BIN="${ADB_BIN:-adb}"
ANDROID_DIR="/data/local/tmp/sgame_refactor"
SERIAL=""
AUTO_RUN=0

usage() {
  cat <<'EOF'
Usage:
  deploy_refactor.sh [--serial <id>] [--target <android_dir>] [--run]

Options:
  --serial <id>      adb device serial (optional)
  --target <path>    target directory on device (default: /data/local/tmp/sgame_refactor)
  --run              run launcher immediately after deploy
  -h, --help         show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --serial)
      [[ $# -ge 2 ]] || { echo "missing value for --serial" >&2; exit 1; }
      SERIAL="$2"
      shift 2
      ;;
    --target)
      [[ $# -ge 2 ]] || { echo "missing value for --target" >&2; exit 1; }
      ANDROID_DIR="$2"
      shift 2
      ;;
    --run)
      AUTO_RUN=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if ! command -v "$ADB_BIN" >/dev/null 2>&1; then
  echo "adb not found, set ADB_BIN or install Android platform-tools." >&2
  exit 1
fi

adb_cmd=("$ADB_BIN")
if [[ -n "$SERIAL" ]]; then
  adb_cmd+=("-s" "$SERIAL")
fi

echo "[1/5] checking adb device connection"
"${adb_cmd[@]}" get-state >/dev/null

echo "[2/5] creating target directories: $ANDROID_DIR"
"${adb_cmd[@]}" shell "mkdir -p '$ANDROID_DIR'"

echo "[3/5] syncing refactor track"
"${adb_cmd[@]}" push "$REFACTOR_DIR/." "$ANDROID_DIR/" >/dev/null

echo "[4/5] fixing runtime permissions"
"${adb_cmd[@]}" shell "chmod 755 '$ANDROID_DIR/launcher/sgame_menu.sh'"
"${adb_cmd[@]}" shell "mkdir -p '$ANDROID_DIR/runtime' '$ANDROID_DIR/logs'"

echo "[5/5] ensuring user config exists"
"${adb_cmd[@]}" shell "[ -f '$ANDROID_DIR/conf/user.env' ] || cp '$ANDROID_DIR/conf/default.env' '$ANDROID_DIR/conf/user.env'"

echo
echo "Deploy complete."
echo "Run launcher command:"
echo "  adb ${SERIAL:+-s $SERIAL }shell 'cd $ANDROID_DIR/launcher && sh ./sgame_menu.sh'"

if [[ "$AUTO_RUN" -eq 1 ]]; then
  echo
  echo "Launching now..."
  "${adb_cmd[@]}" shell "cd '$ANDROID_DIR/launcher' && sh ./sgame_menu.sh"
fi
