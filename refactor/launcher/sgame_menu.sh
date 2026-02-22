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
CONTROL_SEQ_FILE="$RUNTIME_DIR/control.seq"
CONTROL_ACK_FILE="$RUNTIME_DIR/control.ack"

mkdir -p "$CONF_DIR" "$RUNTIME_DIR" "$LOG_DIR"
[ -f "$STATE_FILE" ] || : > "$STATE_FILE"
[ -f "$LOG_FILE" ] || : > "$LOG_FILE"
[ -f "$CONTROL_FILE" ] || : > "$CONTROL_FILE"
[ -f "$CONTROL_SEQ_FILE" ] || printf '0\n' > "$CONTROL_SEQ_FILE"
[ -f "$CONTROL_ACK_FILE" ] || : > "$CONTROL_ACK_FILE"

log() {
    printf '%s\n' "$*"
}

shell_quote() {
    # Quote values for shell-style KEY=VALUE config files.
    # This preserves spaces and special characters when we source user.env.
    printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"
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
DRIVER_MODE=$(shell_quote "$DRIVER_MODE")
GAME_PKG=$(shell_quote "$GAME_PKG")
CORE_BIN=$(shell_quote "$CORE_BIN")
AUTO_INIT_DRAW=$(shell_quote "$AUTO_INIT_DRAW")
LOG_LEVEL=$(shell_quote "$LOG_LEVEL")
EXTRA_ARGS=$(shell_quote "$EXTRA_ARGS")
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

next_control_seq() {
    seq=0
    if [ -f "$CONTROL_SEQ_FILE" ]; then
        seq=$(cat "$CONTROL_SEQ_FILE" 2>/dev/null || echo 0)
    fi
    case "$seq" in
        ''|*[!0-9]*) seq=0 ;;
    esac
    seq=$((seq + 1))
    printf '%s\n' "$seq" > "$CONTROL_SEQ_FILE"
    printf '%s\n' "$seq"
}

append_control_cmd() {
    cmd=$1
    seq=$(next_control_seq)
    ts=$(now_ts)
    printf '%s|%s|%s\n' "$seq" "$ts" "$cmd" >> "$CONTROL_FILE"
    write_state LAST_CONTROL_SEQ "$seq"
    write_state LAST_CONTROL_TS "$ts"
    write_state LAST_CONTROL_CMD "$cmd"
}

find_game_pid() {
    pkg="$1"
    pid=""
    if command -v pidof >/dev/null 2>&1; then
        pid=$(pidof "$pkg" 2>/dev/null | awk '{print $1}' || true)
        if [ -n "${pid:-}" ]; then
            printf '%s\n' "$pid"
            return 0
        fi
    fi
    pid=$(ps 2>/dev/null | awk -v p="$pkg" '$0 ~ p {print $2; exit}' || true)
    if [ -n "${pid:-}" ]; then
        printf '%s\n' "$pid"
        return 0
    fi
    pid=$(ps -A 2>/dev/null | awk -v p="$pkg" '$0 ~ p {print $2; exit}' || true)
    if [ -n "${pid:-}" ]; then
        printf '%s\n' "$pid"
        return 0
    fi
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
    last_game_pid=$(get_state LAST_GAME_PID || true)
    draw_req=$(get_state DRAW_REQUESTED || true)
    [ -n "${draw_req:-}" ] || draw_req=0
    running=0
    if [ -n "${pid:-}" ] && is_pid_running "$pid"; then
        running=1
    fi

    echo "========================================"
    echo "  视野共享重构启动器 (Phase-1)"
    echo "========================================"
    echo "驱动模式       : $DRIVER_MODE"
    echo "游戏包名       : $GAME_PKG"
    echo "核心路径       : $CORE_BIN"
    echo "核心PID        : ${pid:-N/A} (运行=$running)"
    echo "最近游戏PID    : ${last_game_pid:-N/A}"
    echo "自动初始化绘制 : $AUTO_INIT_DRAW"
    echo "绘制请求状态   : $draw_req"
    echo "日志文件       : $LOG_FILE"
    echo "控制ACK文件    : $CONTROL_ACK_FILE"
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

set_game_pkg() {
    load_conf
    printf "输入游戏包名 (当前: %s): " "$GAME_PKG"
    read newpkg || return
    [ -n "${newpkg:-}" ] || { log "[!] 包名不能为空"; pause; return; }
    GAME_PKG="$newpkg"
    save_conf
    write_state LAST_ACTION "set_game_pkg"
    log "[+] 游戏包名已更新: $GAME_PKG"
    pause
}

set_log_level() {
    load_conf
    echo "选择日志级别:"
    echo "1) error"
    echo "2) warn"
    echo "3) info"
    echo "4) debug"
    printf "输入编号 (当前: %s): " "$LOG_LEVEL"
    read choice || return
    case "$choice" in
        1) LOG_LEVEL=error ;;
        2) LOG_LEVEL=warn ;;
        3) LOG_LEVEL=info ;;
        4) LOG_LEVEL=debug ;;
        *) log "[!] 无效选择"; pause; return ;;
    esac
    save_conf
    write_state LAST_ACTION "set_log_level:$LOG_LEVEL"
    log "[+] 日志级别已设置: $LOG_LEVEL"
    pause
}

set_extra_args() {
    load_conf
    echo "输入额外启动参数 (当前: $EXTRA_ARGS)"
    echo "说明: 按 shell 空格分词；直接回车可清空。"
    printf "> "
    read newargs || return
    EXTRA_ARGS="$newargs"
    save_conf
    write_state LAST_ACTION "set_extra_args"
    log "[+] EXTRA_ARGS 已更新: ${EXTRA_ARGS:-<empty>}"
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
    game_pid=$(find_game_pid "$GAME_PKG" || true)
    if [ -n "${game_pid:-}" ]; then
        write_state LAST_GAME_PID "$game_pid"
        log "    检测到游戏PID: $game_pid"
    else
        log "    当前未检测到游戏PID，核心将自行等待游戏。"
    fi

    # Phase-1: use env vars + optional extra args for legacy compatibility.
    # The future refactor core will consume these values via CLI.
    (
        export SGAME_DRIVER="$DRIVER_MODE"
        export SGAME_GAME_PKG="$GAME_PKG"
        export SGAME_LOG_LEVEL="$LOG_LEVEL"
        export SGAME_CONTROL_FILE="$CONTROL_FILE"
        export SGAME_CONTROL_ACK_FILE="$CONTROL_ACK_FILE"
        export SGAME_DRAW_DEFAULT=0
        if [ -n "${game_pid:-}" ]; then
            export SGAME_GAME_PID="$game_pid"
        fi
        # Intentional split for operator-provided argument line.
        # shellcheck disable=SC2086
        nohup "$CORE_BIN" $EXTRA_ARGS >> "$LOG_FILE" 2>&1 &
        echo $! > "$PID_FILE"
    ) || {
        log "[-] 启动失败"
        pause
        return
    }

    pid=$(core_pid || true)
    sleep 1
    if [ -n "${pid:-}" ] && is_pid_running "$pid"; then
        write_state CORE_RUNNING 1
    else
        write_state CORE_RUNNING 0
        log "[!] 核心进程启动后很快退出，请查看日志。"
    fi
    write_state LAST_START_TS "$(now_ts)"
    write_state LAST_DRIVER "$DRIVER_MODE"
    write_state LAST_ACTION "start_core"
    write_state LAST_LOG_LEVEL "$LOG_LEVEL"

    log "[+] 核心进程已启动: ${pid:-unknown}"

    if [ "$AUTO_INIT_DRAW" = "1" ]; then
        append_control_cmd "INIT_DRAW"
        write_state DRAW_REQUESTED 1
        log "[i] AUTO_INIT_DRAW=1, 已写入 INIT_DRAW (实时控制队列)"
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
    append_control_cmd "INIT_DRAW"
    write_state DRAW_REQUESTED 1
    write_state LAST_ACTION "init_draw"
    log "[i] 已写入 INIT_DRAW 命令（实时控制队列）。"
    log "    若核心已接入M2控制通道，将在运行中立即生效。"
    pause
}

stop_draw() {
    append_control_cmd "STOP_DRAW"
    write_state DRAW_REQUESTED 0
    write_state LAST_ACTION "stop_draw"
    log "[i] 已写入 STOP_DRAW 命令（实时控制队列）。"
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
    echo "======== 状态 ========"
    echo "-- 配置 --"
    echo "DRIVER_MODE=$DRIVER_MODE"
    echo "GAME_PKG=$GAME_PKG"
    echo "CORE_BIN=$CORE_BIN"
    echo "AUTO_INIT_DRAW=$AUTO_INIT_DRAW"
    echo "LOG_LEVEL=$LOG_LEVEL"
    echo "EXTRA_ARGS=$EXTRA_ARGS"
    echo
    echo "-- 运行态 --"
    if [ -f "$STATE_FILE" ]; then
        cat "$STATE_FILE"
    else
        echo "(无状态文件)"
    fi
    echo
    echo "-- 进程 --"
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
    echo "-- 最近控制命令 (tail) --"
    tail -n 10 "$CONTROL_FILE" 2>/dev/null || true
    echo
    echo "-- 控制结果ACK (tail) --"
    tail -n 10 "$CONTROL_ACK_FILE" 2>/dev/null || true
    pause
}

tail_logs() {
    echo "======== 日志 (tail -n 60) ========"
    tail -n 60 "$LOG_FILE" 2>/dev/null || true
    pause
}

clear_control_queue() {
    : > "$CONTROL_FILE"
    printf '0\n' > "$CONTROL_SEQ_FILE"
    : > "$CONTROL_ACK_FILE"
    write_state LAST_ACTION "clear_control_queue"
    log "[+] 已清空控制命令队列与ACK"
    pause
}

menu_loop() {
    while :; do
        show_header
        echo "1) 选择驱动模式"
        echo "2) 设置核心路径"
        echo "3) 设置游戏包名"
        echo "4) 设置日志级别"
        echo "5) 设置额外启动参数"
        echo "6) 查询一次游戏PID"
        echo "7) 等待游戏PID"
        echo "8) 启动核心"
        echo "9) 初始化绘制 (实时控制)"
        echo "10) 停止绘制 (实时控制)"
        echo "11) 停止核心"
        echo "12) 切换自动初始化绘制"
        echo "13) 查看状态"
        echo "14) 查看日志"
        echo "15) 清空控制命令队列"
        echo "0) 退出"
        printf "\n请输入选项: "
        read op || exit 0
        case "$op" in
            1) select_driver ;;
            2) set_core_bin ;;
            3) set_game_pkg ;;
            4) set_log_level ;;
            5) set_extra_args ;;
            6) show_game_pid_once ;;
            7) wait_game_pid ;;
            8) start_core ;;
            9) init_draw ;;
            10) stop_draw ;;
            11) stop_core ;;
            12) toggle_auto_init_draw ;;
            13) show_status ;;
            14) tail_logs ;;
            15) clear_control_queue ;;
            0) exit 0 ;;
            *) log "[!] 无效选项"; pause ;;
        esac
    done
}

menu_loop
