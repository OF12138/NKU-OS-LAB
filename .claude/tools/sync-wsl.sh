#!/bin/bash
# 把 Windows 仓库某个提交的 code/ 导出到 WSL 的 ~/Code/NKU-OS-LAB/<分支>/code，用于编译、调试和截图。
# 导出的是已提交的版本（git archive），不含工作区里未提交的改动。
# 用法: sync-wsl.sh [分支或提交，默认为 Windows 仓库当前所在分支]
REPO=/mnt/f/Code/OS/NKU-OS-LAB
REF=${1:-$(git -C "$REPO" branch --show-current)}
DST=$HOME/Code/NKU-OS-LAB/$REF
rm -rf "$DST" && mkdir -p "$DST"
git -C "$REPO" archive "$REF" code | tar -x -C "$DST"
echo "synced $REF -> $DST/code"
