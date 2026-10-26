#!/bin/bash
# 把 Windows 仓库某个提交的 code/ 导出到 WSL 的 ~/Code/NKU-OS-LAB/<分支>/code，用于编译、调试和截图。
# 导出的是已提交的版本（git archive），不含工作区里未提交的改动。
# 用法: sync-wsl.sh [分支或提交，默认为 Windows 仓库当前所在分支]
REPO=/mnt/f/Code/OS/NKU-OS-LAB
# 只能在 WSL 里运行：在 Git Bash 里 $HOME 是 Windows 用户目录、/mnt/f 不存在，会在错误的位置 rm -rf
if ! grep -qi microsoft /proc/version 2>/dev/null || [ ! -d "$REPO/.git" ]; then
    echo "sync-wsl.sh 必须在 WSL 中运行，例如: wsl -e bash $REPO/.claude/tools/sync-wsl.sh" >&2; exit 1
fi
REF=${1:-$(git -C "$REPO" branch --show-current)}
[ -n "$REF" ] || { echo "取不到分支名，请显式传入分支或提交" >&2; exit 1; }
DST=$HOME/Code/NKU-OS-LAB/$REF
rm -rf "$DST" && mkdir -p "$DST"
git -C "$REPO" archive "$REF" code | tar -x -C "$DST"
echo "synced $REF -> $DST/code"
