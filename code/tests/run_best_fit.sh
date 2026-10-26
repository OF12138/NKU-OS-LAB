#!/bin/bash
# 用当前工作区源码构建临时测试内核；原始代码与评测文件不变。
set -euo pipefail
CODE=$(cd "$(dirname "$0")/.." && pwd)
: "${QEMU:?请设置为 QEMU 4.1.1 可执行文件的路径}"
"$QEMU" --version | head -1 | grep -F 'version 4.1.1'
WORK=$(mktemp -d /tmp/lyp-t2-regression.XXXXXX)
trap 'rm -rf -- "$WORK"' EXIT
tar -C "$CODE" --exclude=bin --exclude=obj -cf - . | tar -C "$WORK" -xf -
cp "$CODE/tests/best_fit_regression.c" "$WORK/kern/mm/best_fit_regression.c"
python3 - "$WORK/kern/mm/pmm.c" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
source = p.read_text()
anchor = '    check_alloc_page();'
assert source.count(anchor) == 1
p.write_text(source.replace(anchor, anchor + '\n    extern void best_fit_regression_check(void);\n    best_fit_regression_check();'))
PY
make -C "$WORK" -j4 > "$WORK/build.log" 2>&1 || { cat "$WORK/build.log"; exit 1; }
result=0
timeout 5s make -C "$WORK" qemu > "$WORK/run.log" 2>&1 || result=$?
# 内核完成测试后空转，由 timeout 正常终止。
[[ $result == 0 || $result == 124 ]] || { cat "$WORK/run.log"; exit 1; }
cat "$WORK/run.log"
! grep -Eiq 'panic|FAIL' "$WORK/run.log"
[[ $(grep -c '^\[T2 regression\].*: PASS' "$WORK/run.log") == 3 ]]
