#!/system/bin/sh
set -eu

# Refactor launcher (phase-1)
# Product goal: provide a stable operator menu flow independent from in-game overlay UI.

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
CONF_DIR="$ROOT_DIR/conf"
RUNTIME_DIR="$ROOT_DIR/runtime"
LOG_DIR="$ROOT_DIR/logs"
CONF_FILE="$CONF_DIR/user.env"
DEFAULT_CONF="$CONF_DIR/default.env"
STATE_FILE="$RUNTIME_DIR/state.env"
PID_FILE="$RUNTIME_DIR/core.pid"
LOG_FILE="$LOG_DIR/core.log"
CONTROL_FILE="$RUNTIME_DIR/control.cmd"

mkdir -p "$CONF_DIR" "$RUNTIME_DIR" "$LOG_DIR"
[ -f "$STATE_FILE" ] || : > "$STATE_FILE"
[ -f "$LOG_FILE" ] || : > "$LOG_FILE"
[ -f "$CONTROL_FILE" ] || : > "$CONTROL_FILE"

log() {
    printf '%s\n' "$*"
}

pause() {
    printf '\n按回车继续...'
    read _dummy || true
}

ensure_conf() {
    if [ ! -f "$CONF_FILE" ]; then
        if [ -f "$DEFAULT_CONF" ]; then
            cp "$DEFAULT_CONF" "$CONF_FILE"
        else
            cat > "$CONF_FILE" <<'EOF'
DRIVER_MODE=auto
GAME_PKG=com.tencent.tmgp.sgame
CORE_BIN=/data/local/tmp/hphphp
AUTO_INIT_DRAW=0
LOG_LEVEL=info
EXTRA_ARGS=
EOF
        fi
    fi
}

load_conf() {
    ensure_conf
    DRIVER_MODE=auto
    GAME_PKG=com.tencent.tmgp.sgame
    CORE_BIN=/data/local/tmp/hphphp
    AUTO_INIT_DRAW=0
    LOG_LEVEL=info
    EXTRA_ARGS=
    # shellcheck disable=SC1090
    . "$CONF_FILE"
}

save_conf() {
    tmp="$CONF_FILE.tmp"
    cat > "$tmp" <<EOF
DRIVER_MODE=$DRIVER_MODE
GAME_PKG=$GAME_PKG
CORE_BIN=$CORE_BIN
AUTO_INIT_DRAW=$AUTO_INIT_DRAW
LOG_LEVEL=$LOG_LEVEL
EXTRA_ARGS=$EXTRA_ARGS
EOF
    mv "$tmp" "$CONF_FILE"
}

write_state() {
    key=$1
    value=$2
    tmp="$STATE_FILE.tmp"
    if [ -f "$STATE_FILE" ]; then
        grep -v "^${key}=" "$STATE_FILE" > "$tmp" 2>/dev/null || true
    else
        : > "$tmp"
    fi
    printf '%s=%s\n' "$key" "$value" >> "$tmp"
    mv "$tmp" "$STATE_FILE"
}

get_state() {
    key=$1
    if [ ! -f "$STATE_FILE" ]; then
        return 1
    fi
    grep "^${key}=" "$STATE_FILE" 2>/dev/null | tail -n 1 | cut -d= -f2-
}

now_ts() {
    date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo "unknown-time"
}

find_game_pid() {
    pkg="$1"
    if command -v pidof >/dev/null 2>&1; then
        pidof "$pkg" 2>/dev/null | awk '{print $1}' && return 0
    fi
    ps 2>/dev/null | awk -v p="$pkg" '$0 ~ p {print $2; exit}' && return 0
    ps -A 2>/dev/null | awk -v p="$pkg" '$0 ~ p {print $2; exit}' && return 0
    return 1
}

is_pid_running() {
    pid="$1"
    [ -n "$pid" ] || return 1
    kill -0 "$pid" 2>/dev/null
}

core_pid() {
    if [ -f "$PID_FILE" ]; then
        cat "$PID_FILE"
    fi
}

stop_core_if_stale() {
    pid=$(core_pid || true)
    if [ -n "${pid:-}" ] && ! is_pid_running "$pid"; then
        rm -f "$PID_FILE"
        write_state CORE_RUNNING 0
    fi
}

show_header() {
    clear 2>/dev/null || true
    load_conf
    stop_core_if_stale
    pid=$(core_pid || true)
    running=0
    if [ -n "${pid:-}" ] && is_pid_running "$pid"; then
        running=1
    fi

    echo "========================================"
    echo "  SGAME Refactor Launcher (Phase-1)"
    echo "========================================"
    echo "Driver Mode : $DRIVER_MODE"
    echo "Game Pkg    : $GAME_PKG"
    echo "Core Bin    : $CORE_BIN"
    echo "Core PID    : ${pid:-N/A} (running=$running)"
    echo "Auto Draw   : $AUTO_INIT_DRAW"
    echo "Log File    : $LOG_FILE"
    echo "----------------------------------------"
}

select_driver() {
    load_conf
    echo "选择驱动模式:"
    echo "1) auto"
    echo "2) qx"
    echo "3) dev"
    echo "4) proc"
    echo "5) rt"
    echo "6) dit"
    echo "7) ditpro"
    echo "8) pread"
    echo "9) syscall"
    printf "输入编号: "
    read choice || return
    case "$choice" in
        1) DRIVER_MODE=auto ;;
        2) DRIVER_MODE=qx ;;
        3) DRIVER_MODE=dev ;;
        4) DRIVER_MODE=proc ;;
        5) DRIVER_MODE=rt ;;
        6) DRIVER_MODE=dit ;;
        7) DRIVER_MODE=ditpro ;;
        8) DRIVER_MODE=pread ;;
        9) DRIVER_MODE=syscall ;;
        *) log "[!] 无效选择"; pause; return ;;
    esac
    save_conf
    write_state LAST_ACTION "select_driver:$DRIVER_MODE"
    log "[+] 驱动模式已设置: $DRIVER_MODE"
    pause
}

set_core_bin() {
    load_conf
    printf "输入核心程序路径 (当前: %s): " "$CORE_BIN"
    read newbin || return
    [ -n "${newbin:-}" ] || { log "[!] 路径不能为空"; pause; return; }
    CORE_BIN="$newbin"
    save_conf
    write_state LAST_ACTION "set_core_bin"
    log "[+] 核心路径已更新: $CORE_BIN"
    pause
}

wait_game_pid() {
    load_conf
    log "[...] 等待游戏进程: $GAME_PKG"
    while :; do
        pid=$(find_game_pid "$GAME_PKG" || true)
        if [ -n "${pid:-}" ]; then
            write_state LAST_GAME_PID "$pid"
            write_state LAST_ACTION "wait_game_pid"
            log "[+] 游戏 PID: $pid"
            pause
            return
        fi
        sleep 1
    done
}

show_game_pid_once() {
    load_conf
    pid=$(find_game_pid "$GAME_PKG" || true)
    if [ -n "${pid:-}" ]; then
        write_state LAST_GAME_PID "$pid"
        log "[+] 游戏 PID: $pid"
    else
        log "[-] 未找到游戏进程: $GAME_PKG"
    fi
    write_state LAST_ACTION "show_game_pid_once"
    pause
}

start_core() {
    load_conf
    stop_core_if_stale
    pid=$(core_pid || true)
    if [ -n "${pid:-}" ] && is_pid_running "$pid"; then
        log "[!] 核心进程已在运行: $pid"
        pause
        return
    fi

    if [ ! -x "$CORE_BIN" ]; then
        log "[!] 核心程序不存在或不可执行: $CORE_BIN"
        log "    请先在菜单里设置正确的核心路径。"
        pause
        return
    fi

    log "[...] 启动核心进程..."
    log "    driver=$DRIVER_MODE game_pkg=$GAME_PKG"

    # Phase-1: use env vars + optional extra args for legacy compatibility.
    # The future refactor core will consume these values via CLI.
    sh -c "SGAME_DRIVER='$DRIVER_MODE' SGAME_GAME_PKG='$GAME_PKG' SGAME_LOG_LEVEL='$LOG_LEVEL' nohup '$CORE_BIN' $EXTRA_ARGS >> '$LOG_FILE' 2>&1 & echo \$! > '$PID_FILE'" || {
        log "[-] 启动失败"
        pause
        return
    }

    pid=$(core_pid || true)
    write_state CORE_RUNNING 1
    write_state LAST_START_TS "$(now_ts)"
    write_state LAST_DRIVER "$DRIVER_MODE"
    write_state LAST_ACTION "start_core"

    log "[+] 核心进程已启动: ${pid:-unknown}"

    if [ "$AUTO_INIT_DRAW" = "1" ]; then
        echo "INIT_DRAW" >> "$CONTROL_FILE"
        log "[i] AUTO_INIT_DRAW=1, 已写入 INIT_DRAW (占位命令)"
    fi

    pause
}

stop_core() {
    stop_core_if_stale
    pid=$(core_pid || true)
    if [ -z "${pid:-}" ]; then
        log "[!] 没有找到核心 PID 文件"
        pause
        return
    fi
    if ! is_pid_running "$pid"; then
        rm -f "$PID_FILE"
        write_state CORE_RUNNING 0
        log "[i] 核心进程已不在运行，PID 文件已清理"
        pause
        return
    fi

    kill "$pid" 2>/dev/null || true
    sleep 1
    if is_pid_running "$pid"; then
        kill -9 "$pid" 2>/dev/null || true
    fi
    rm -f "$PID_FILE"
    write_state CORE_RUNNING 0
    write_state LAST_ACTION "stop_core"
    log "[+] 核心进程已停止"
    pause
}

init_draw() {
    # Phase-1 placeholder: keep operator workflow stable before control IPC exists.
    echo "INIT_DRAW" >> "$CONTROL_FILE"
    write_state LAST_ACTION "init_draw"
    log "[i] 已记录 INIT_DRAW 命令（阶段1占位）。"
    log "    等核心接入 control endpoint 后会变为实时控制。"
    pause
}

stop_draw() {
    echo "STOP_DRAW" >> "$CONTROL_FILE"
    write_state LAST_ACTION "stop_draw"
    log "[i] 已记录 STOP_DRAW 命令（阶段1占位）。"
    pause
}

toggle_auto_init_draw() {
    load_conf
    if [ "$AUTO_INIT_DRAW" = "1" ]; then
        AUTO_INIT_DRAW=0
    else
        AUTO_INIT_DRAW=1
    fi
    save_conf
    write_state LAST_ACTION "toggle_auto_init_draw:$AUTO_INIT_DRAW"
    log "[+] AUTO_INIT_DRAW=$AUTO_INIT_DRAW"
    pause
}

show_status() {
    load_conf
    stop_core_if_stale
    echo "======== STATUS ========"
    echo "-- Config --"
    echo "DRIVER_MODE=$DRIVER_MODE"
    echo "GAME_PKG=$GAME_PKG"
    echo "CORE_BIN=$CORE_BIN"
    echo "AUTO_INIT_DRAW=$AUTO_INIT_DRAW"
    echo "LOG_LEVEL=$LOG_LEVEL"
    echo "EXTRA_ARGS=$EXTRA_ARGS"
    echo
    echo "-- Runtime --"
    if [ -f "$STATE_FILE" ]; then
        cat "$STATE_FILE"
    else
        echo "(no state file)"
    fi
    echo
    echo "-- PID --"
    pid=$(core_pid || true)
    if [ -n "${pid:-}" ]; then
        echo "core.pid=$pid"
        if is_pid_running "$pid"; then
            echo "running=yes"
        else
            echo "running=no (stale pid file)"
        fi
    else
        echo "core.pid=(none)"
    fi
    echo
    echo "-- Last control commands (tail) --"
    tail -n 10 "$CONTROL_FILE" 2>/dev/null || true
    pause
}

tail_logs() {
    echo "======== LOG (tail -n 60) ========"
    tail -n 60 "$LOG_FILE" 2>/dev/null || true
    pause
}

menu_loop() {
    while :; do
        show_header
        echo "1) 选择驱动模式"
        echo "2) 设置核心路径"
        echo "3) 查询一次游戏PID"
        echo "4) 等待游戏PID"
        echo "5) 启动核心"
        echo "6) 初始化绘制 (占位)"
        echo "7) 停止绘制 (占位)"
        echo "8) 停止核心"
        echo "9) 切换自动初始化绘制"
        echo "10) 查看状态"
        echo "11) 查看日志"
        echo "0) 退出"
        printf "\n请输入选项: "
        read op || exit 0
        case "$op" in
            1) select_driver ;;
            2) set_core_bin ;;
            3) show_game_pid_once ;;
            4) wait_game_pid ;;
            5) start_core ;;
            6) init_draw ;;
            7) stop_draw ;;
            8) stop_core ;;
            9) toggle_auto_init_draw ;;
            10) show_status ;;
            11) tail_logs ;;
            0) exit 0 ;;
            *) log "[!] 无效选项"; pause ;;
        esac
    done
}

menu_loop

