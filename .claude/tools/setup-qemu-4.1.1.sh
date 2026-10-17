#!/bin/bash
# 在 WSL/Ubuntu 中编译带调试信息的 QEMU 4.1.1（指导书基准版本，自带 OpenSBI v0.4），不执行 make install，
# 不影响系统里已有的 qemu。日常 make qemu / make grade 和 lab2 分支任务（GDB 调试 QEMU 源码）共用这一个可执行文件。
# 用法: bash setup-qemu-4.1.1.sh [安装目录，默认 ~/Environment/qemu-4.1.1]
set -e
DIR=${1:-$HOME/Environment/qemu-4.1.1}
NEED="build-essential python3 pkg-config libglib2.0-dev libpixman-1-dev flex bison gdb"
MISSING=$(for p in $NEED; do dpkg -s $p >/dev/null 2>&1 || echo $p; done)
if [ -n "$MISSING" ]; then
    echo "缺少依赖，请先执行: sudo apt install -y $MISSING"; exit 1
fi
if [ ! -f "$DIR/configure" ]; then
    mkdir -p "$(dirname "$DIR")"
    cd "$(dirname "$DIR")"
    wget -nc https://download.qemu.org/qemu-4.1.1.tar.xz
    tar xJf qemu-4.1.1.tar.xz
    [ "$(basename "$DIR")" = qemu-4.1.1 ] || mv qemu-4.1.1 "$DIR"
fi
cd "$DIR"
# 只编 riscv64，够用且快；--enable-debug 关闭优化并保留符号，供 GDB 调试 QEMU 源码；
# --disable-werror 防止新版 gcc 的新警告让旧代码编译失败
./configure --target-list=riscv64-softmmu --enable-debug --disable-werror
make -j"$(nproc)"
QEMU_BIN="$DIR/riscv64-softmmu/qemu-system-riscv64"
"$QEMU_BIN" --version | head -1
echo
echo "编译完成。把下面这行加进 ~/.bashrc，ucore 的 Makefile 会通过 QEMU 变量使用它（Makefile 中是 ifndef QEMU）："
echo "export QEMU=$QEMU_BIN"
