#!/bin/bash
# 双 GDB 调试 QEMU 的一键脚本，由 dual-gdb.cmd（启动）和 dual-gdb-stop.cmd（退出）调用，一般不用手动运行。
# 用法: dual-gdb.sh <qemu|qemugdb|ucoregdb|stop> [code目录]
#   qemu      窗格 1：make debug（-s -S，QEMU 等待 gdb 连接）
#   qemugdb   窗格 2：普通 gdb attach 到 QEMU 进程，调试 QEMU 的 C 源码；等你设断点后输入 continue
#   ucoregdb  窗格 3：riscv gdb 调试 ucore；等窗格 2 continue 之后自动连接（make gdb）
#   stop      结束 QEMU 和两个 gdb，各窗格随进程退出而关闭
# code 目录默认是本仓库工作区的 code/（含未提交的修改，行号与编辑器一致）；
# 想用 sync-wsl.sh 导出的已提交版本，可设置环境变量 DUAL_GDB_CODE=~/Code/NKU-OS-LAB/lab2/code
ROLE=$1
REPO=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
CODE=${2:-${DUAL_GDB_CODE:-$REPO/code}}
STOPFLAG=/tmp/dual-gdb.stop
export PATH=$HOME/Environment/riscv/riscv-elf-toolchains/bin:$PATH
export QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64

qemu_pid() { pidof qemu-system-riscv64 | awk '{print $1}'; }

# stop 发来 SIGTERM 时直接以 0 退出：Windows Terminal 只在退出码为 0 时自动关闭窗格
trap 'exit 0' TERM

# 出错时停住，免得窗格一闪而过看不到错误信息；用 stop 主动退出时不停
pause_on_error() {
    if [ "$1" -ne 0 ] && [ ! -e "$STOPFLAG" ]; then
        echo; echo "[退出码 $1] 按回车关闭本窗格"; read -r _
    fi
}

# 前台命令结束后的收尾：被 stop 结束的一律以 0 退出（窗格自动关闭），否则出错时停住
finish() {
    [ -e "$STOPFLAG" ] && exit 0
    pause_on_error "$1"; exit "$1"
}

wait_qemu() {
    PID=""
    for _ in $(seq 1 600); do
        PID=$(qemu_pid); [ -n "$PID" ] && return 0; sleep 0.5
    done
    echo "超时：QEMU 没有启动，请看 QEMU 窗格里的编译错误"; pause_on_error 1; exit 1
}

case $ROLE in
qemu)
    rm -f "$STOPFLAG"
    cd "$CODE" || { pause_on_error 1; exit 1; }
    pkill -f qemu-system-riscv64; sleep 0.5
    echo "[1] QEMU: make debug  (code = $CODE)"
    make debug
    finish $?
    ;;
qemugdb)
    echo "[2] 调试 QEMU 的 gdb：等待 QEMU 启动（make debug 编译完成后）..."
    wait_qemu; sleep 1
    gdb -q -p "$PID" \
        -ex 'handle SIGPIPE nostop noprint' \
        -ex 'echo \n>>> 已 attach 到 QEMU（此时 QEMU 暂停）。在这里设断点，例如：\n' \
        -ex 'echo     break get_physical_address if addr == 0xffffffffc02000d6\n' \
        -ex 'echo >>> 然后输入 continue；右侧窗格会随后自动连接 ucore。\n\n'
    finish $?
    ;;
ucoregdb)
    echo "[3] 调试 ucore 的 gdb：等待左侧窗格设好断点并 continue ..."
    wait_qemu
    # 阶段 1：等 gdb 把 QEMU 停住（State = t，tracing stop）；阶段 2：等它 continue（State 不再是 t）
    until [ "$(awk '/^State/{print $2}' /proc/$PID/status 2>/dev/null)" = t ]; do sleep 0.3; done
    until [ "$(awk '/^State/{print $2}' /proc/$PID/status 2>/dev/null)" != t ]; do sleep 0.3; done
    sleep 1
    cd "$CODE" || { pause_on_error 1; exit 1; }
    make gdb
    finish $?
    ;;
stop)
    touch "$STOPFLAG"
    # 先 TERM（让各窗格脚本正常退出、窗格自动关闭），仍存活的再 KILL
    for sig in TERM KILL; do
        pkill -$sig -f qemu-system-riscv64
        pkill -$sig -f 'gdb -q -p'
        pkill -$sig -f riscv64-unknown-elf-gdb
        pkill -$sig -f 'dual-gdb.sh (qemu|qemugdb|ucoregdb)'   # 包括还在等待 QEMU 的窗格脚本
        sleep 1
    done
    echo "stopped"
    ;;
*)
    echo "用法: $0 <qemu|qemugdb|ucoregdb|stop> [code目录]"; exit 2
    ;;
esac
