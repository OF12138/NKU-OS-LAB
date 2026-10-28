# T1 功能模块一：Trap 上下文恢复与 Trap 入口
- 负责人：openfar    DDL：11-01    状态：完成    依赖：T0
- 主要文件：code/kern/trap/trapentry.S, report/sections/T1-trapentry.md, report/sections/T1-trapentry.prompt.md

## 要求
- 指导书「功能模块一」：在 `kern/trap/trapentry.S` 中实现 `RESTORE_ALL` 和 `__alltraps`（`#lab3 function1 YOUR CODE` 两处）。
  - RESTORE_ALL 必须与已给的 SAVE_ALL 对应；SAVE_ALL 对 x2(sp) 的处理与其他寄存器不同，不能照搬同样的恢复方式。
  - `__alltraps` 按 RISC-V 调用约定把 Trap frame 地址作为 `trap(struct trapframe *tf)` 的参数，`trap()` 返回后进入 `__trapret`，RESTORE_ALL 后 `sret`。
  - 不得修改 SAVE_ALL、`struct trapframe` 和 `trap()` 的签名。
- 提示词：按四段式写一份，[RELY] 引用框架原始代码（SAVE_ALL、trapframe/pushregs、trap()、sstatus/sepc 相关宏）。
- 报告 section：模块功能描述、最终提示词（`<!-- include:T1-trapentry.prompt.md -->`）、实现迭代过程；说明 sp 的特殊处理、哪些 CSR 需要恢复（sstatus/sepc）而 sbadaddr/scause 不必恢复的原因、`mv a0, sp` 的作用。
- GDB 验证：在 `__alltraps`、`trap`、`__trapret`、`sret` 处断点，截图展示 Trap frame 在栈上的布局和 sret 前后 pc/sp 的变化（截图放 `report/images/T1-*`）。

## 当前进度 / 下一步
- 已完成（10-28）：代码、提示词 `T1-trapentry.prompt.md`、section `T1-trapentry.md`、截图 `T1-gdb-trap-{entry,return}.png`。
- T2 合入后由 T5 重跑正式的 `make grade`。

## 关键决策与结论
- RESTORE_ALL：先 `LOAD s1/s2` 取 status/epc 写回 `sstatus`/`sepc`，再按布局恢复 x1、x3～x31，最后 `LOAD x2, 2*REGBYTES(sp)`；不写回 stval/scause，不恢复 sscratch（保持 0，表示在内核态）。
- `__alltraps`：`move a0, sp` + `jal trap`，之后顺序落入 `__trapret`。
- 对 T2 的接口：`trap()` 返回后 `tf->epc` 会写回 `sepc`，所以异常处理只需改 `tf->epc`。

## 验证结果
- WSL 临时副本（trap.c 放临时处理函数，未提交）：`make` 通过；`make qemu` 每秒打印 `100 ticks`；`make grade` 100/100（QEMU 4.1.1）。
- GDB（图 1-1、1-2）：第一次时钟中断 scause=0x8000000000000005；Trap frame 地址 0xffffffffc0204ed0，比原 sp 0xffffffffc0204ff0 低 0x120；帧中 gpr.sp=原 sp、sscratch=0；sret 后 pc=sepc=kern_init+72，sp 恢复。
- `check-rely.py` 全部通过。

## 迭代素材
- `Error: unknown CSR 'sbadaddr'`（SAVE_ALL 中，binutils 2.45 删了旧名）→ T0 在 riscv.h 对汇编 `#define sbadaddr stval`。
- 框架缺 T2 的两个处理函数无法编译 → 验证时用临时副本放最简处理函数。
- 反例 A：sp 最先恢复 → `make qemu` 照样打印 ticks，但 grade 25/100（Test 1、3 失败）→ 提示词 Case 1 写明 sp 的保存路径并要求最后恢复，验收以 grade 为准。
- 反例 B：不写回 sepc → grade 40/100（时钟中断正常，ebreak/非法指令死循环）→ [RELY] 注明 trap() 会改 tf->epc，Post-Condition 要求写回 sepc。
- 工具：tmux-gdb.sh 的 `pkill -f qemu-system-riscv64` 会匹配到调用方 `bash -c "export QEMU=.../qemu-system-riscv64 ..."` 而把自己杀掉；`send-keys` 不加 `-l` 时 `delete` 被当成键名 → 已修脚本。

## 留言
