#!/bin/bash
# 在 WSL 中启动单栏 tmux 会话 t2，进入 code 目录后依次输入命令（用于截取 make qemu / make grade 等终端截图）。
# 用法: tmux-run.sh <code目录(WSL路径)> "<命令1>" "<命令2>" ...
# 截图: 之后在 PowerShell 中运行 capture-wt.ps1 -Out xxx.png -Cols 110 -Lines 56
# 会自动设置交叉工具链 PATH 和 QEMU=~/Environment/qemu-4.1.1/...（全组统一的 4.1.1），并清屏后再输入命令。
DIR=$1; shift
pkill -f qemu-system-riscv64; tmux kill-server 2>/dev/null; sleep 0.5
tmux new-session -d -s t2 -x 120 -y 60 -c "$DIR"
tmux set -t t2 status off
sleep 1.5
tmux send-keys -t t2 "export PATH=\$HOME/Environment/riscv/riscv-elf-toolchains/bin:\$PATH QEMU=\$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64; clear" Enter
sleep 0.8
for c in "$@"; do tmux send-keys -t t2 "$c" Enter; sleep 0.6; done
