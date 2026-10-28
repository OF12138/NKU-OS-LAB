#!/bin/bash
# 在 WSL 中启动 tmux 会话 t2：左栏 make debug，右栏 make gdb，然后依次向 GDB 输入命令。
# 用法: tmux-gdb.sh <code目录(WSL路径)，如 ~/Code/NKU-OS-LAB/lab1/code> "<gdb命令1>" "<gdb命令2>" ...
# 可选: 环境变量 MAKEARGS 会追加到 make debug / make gdb 后面，例如 MAKEARGS="OPENSBI=..."
# 截图: 之后在 PowerShell 中运行 capture-wt.ps1 -Out xxx.png -Cols 170 -Lines 40
export PATH=$HOME/Environment/riscv/riscv-elf-toolchains/bin:$PATH
DIR=$1; shift
cd "$DIR" && make -s >/dev/null
# 只匹配以 qemu 程序开头的命令行：调用方的 bash -c 里若含 "QEMU=.../qemu-system-riscv64"，-f 会把调用方自己杀掉
pkill -f "^[^ ]*qemu-system-riscv64"; tmux kill-server 2>/dev/null; sleep 0.5
tmux new-session -d -s t2 -x 190 -y 50 -c "$DIR"
tmux set -t t2 status off
tmux split-window -h -t t2 -c "$DIR"
tmux resize-pane -t t2:0.0 -x 82
sleep 1.5                                   # 等 shell 提示符就绪，避免命令被提前回显
tmux send-keys -t t2:0.0 "make debug $MAKEARGS" Enter; sleep 1
tmux send-keys -t t2:0.1 "make gdb $MAKEARGS" Enter; sleep 3
tmux send-keys -t t2:0.0 C-l; tmux send-keys -t t2:0.1 C-l; sleep 0.3
# -l 按字面发送，否则 delete、end 这类词会被当成键名
for c in "$@"; do tmux send-keys -t t2:0.1 -l "$c"; tmux send-keys -t t2:0.1 Enter; sleep 0.6; done
sleep 1
