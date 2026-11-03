# T2 功能模块二：中断与异常处理
- 负责人：lyp    DDL：11-02    状态：完成    依赖：T0；完整联调依赖 T1
- 主要文件：code/kern/trap/trap.c, report/sections/T2-trap-handler.md, report/sections/T2-trap-handler.prompt.md

## 要求
- 指导书「功能模块二」：在 `kern/trap/trap.c` 的 `//lab3 function2 YOUR CODE` 处实现
  `void interrupt_handler(struct trapframe *tf);` 和 `void exception_handler(struct trapframe *tf);`，不改函数签名，不改其他函数。
  - 时钟中断（IRQ_S_TIMER）：设置下一次时钟事件、`ticks` 每次加一、达到 `TICK_NUM` 整数倍时调用 `print_ticks()`；累计输出达到 `SHUTDOWN_AFTER` 次后调用 `sbi_shutdown()`。
  - 其他中断：区分类型并输出提示；无法识别的走 `print_trapframe(tf)`。
  - 非法指令、断点异常：按规定格式输出，并按**实际指令长度**（2 字节压缩指令 / 4 字节）更新 `tf->epc`；其他异常用 `print_trapframe(tf)` 兜底。
- 已知要点（写提示词时注意）：
  - 框架中没有定义 `SHUTDOWN_AFTER`，需要自己定义（数值要与 `make grade` Test 3 等待 200 ticks 不冲突）。
  - `make grade` 匹配的字符串：`Breakpoint at`、`Illegal instruction at`、`Supervisor software interrupt`、`Unknown interrupt: cause 15`，未知异常要输出 `cause    0x00000014`（即 print_trapframe）。
  - 评测用的 `ebreak` 可能被汇编成 2 字节的 `c.ebreak`，非法指令是 4 字节的 `.word 0x7b`，所以 epc 偏移要从指令编码低两位判断，不能写死。
- 提示词：按四段式写一份，[RELY] 引用框架原始代码（trapframe、IRQ_*/CAUSE_* 宏、print_trapframe、clock_set_next_event、ticks、sbi_shutdown）。
- 报告 section：模块功能描述、最终提示词（`<!-- include:T2-trap-handler.prompt.md -->`）、实现迭代过程。
- GDB 验证：对断点和非法指令各截一张图，展示进入处理函数时的 `tf->epc`、指令编码和返回后的 pc（截图放 `report/images/T2-*`）。

## 当前进度 / 下一步
- 2026-11-03：两个处理函数、模块报告及最终提示词完成；QEMU 4.1.1 下编译、正常运行、原始评分及两类异常的 GDB 取证均通过，额外完成真实软件中断补测。
- 交付：`T2-trap-handler.md`、`T2-trap-handler.prompt.md` 及五张 `T2-*.png`，等待报告集成；T3 的 lyp 知识点部分也已交付，见共同任务留言。
- 本任务无剩余运行环境阻塞。集成后最终评测仍由 openfar 按 T5 执行。

## 关键决策与结论
- 只修改 `trap.c` 功能模块二区域，保留标记并填写 `2414099`，未改现有其他函数、初始化锚点或评测文件。
- `SHUTDOWN_AFTER=10`；每 100 ticks 打印一次，打印十次关机，避免在 200 ticks 中断内过早关机。
- 清除 cause 最高位提取中断编号，兼容评测直接传入 `cause=15`；管理态软件中断清除 SSIP。
- 断点和非法指令先打印原始 EPC，再从指令首半字判断 2/4 字节并更新 `tf->epc`；仅适用于本实验可读内核代码及 16/32 位指令。其他异常只诊断，不声称恢复成功。
- 正式环境是 `Ubuntu-22.04-OS`，不能使用默认的 `Ubuntu-22.04-Compiler` 推断 QEMU 是否存在。进入 OS 发行版后设置 `PATH=/opt/riscv/bin:$PATH`、`QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`。
- `.claude/tools/sync-wsl.sh` 指向另一处源仓库，且不含未提交改动；本次采用当前 HEAD 归档加 `trap.c` 叠加，并校验哈希。所有评分注入发生在独立副本，不污染仓库源码。

## 验证结果
- 基线：`b95765a` 的 `code/` 归档 + 当前 `trap.c`。正式验证副本位于 OS 发行版的 `~/Code/NKU-OS-LAB/lab3/lyp-runtime.dWPean/`，普通内核在其 `code/`，原始日志为 `build.log`、`qemu.log`、`grade.log`。
- 环境：SiFive GCC 10.2.0、SiFive GDB 10.1、QEMU 4.1.1 / OpenSBI v0.4，保留框架编译选项与 `-Werror`。另在 Compiler 发行版 GCC 15.1.0 下编译通过。
- `make -j4`：退出码 0，成功生成 `bin/kernel` 和 `bin/ucore.img`。
- 源文件与验证副本 SHA-256 一致：`a881001c91fd8c5f140c9aeb94a5140ed7d69131826d521ca72e2688ad37878c`。
- `timeout 25s make qemu`：退出码 0，日志精确包含十行 `100 ticks`，第十次输出后正常关机；截图 `T2-qemu-run.png`。
- `make grade GRADE_TMP=<该验证目录>/grade-work`：35/35、25/25、40/40，`Total Score: 100/100`；仅覆盖已有临时目录变量，未修改评测；截图 `T2-make-grade.png`。
- GDB 压缩断点：`cause=3`，首半字 `0x9002`，原始 EPC `0xffffffffc0201b30`，帧 EPC / sepc / sret 后 PC 均为 `0xffffffffc0201b32`；截图 `T2-breakpoint-epc.png`。
- GDB 非法指令：`cause=2`，编码 `0x0000007b`，原始 EPC `0xffffffffc0201ae0`，帧 EPC / sepc / sret 后 PC 均为 `0xffffffffc0201ae4`；截图 `T2-illegal-epc.png`。
- 两个取证构建分别在 `gdb-breakpoint-capture/`、`gdb-illegal-capture/`，各含 `commands.gdb`、`gdb.log`、`qemu.log`、测试镜像及符号；由原 `inject.py` 与测试宏构建，使用硬件断点，不修改故障编码或帧 EPC。
- 真实软件中断补测：来宾执行 CSR 指令使能 SSIE 并设置 SSIP 后，捕获 `cause=0x8000000000000001`；处理后 SSIP 从 1 清到 0，帧 EPC 和 sret 后 PC 均为 `0xffffffffc0200832`，无推进。证据在 `gdb-software-enabled/`；截图 `T2-software-interrupt.png`。
- 图片从真实日志渲染，保留数值和输出内容，仅展开制表符及折行；评分图摘取原日志的构建、运行和评分行。
- 收尾检查：`python .claude/tools/check-rely.py` 全部通过，`git diff --check` 通过；五张图片引用均有效，代码围栏成对，评测文件、`init.c`、`trapentry.S` 均无改动。
- 在线指导书读取失败，本次按任务摘录、原框架和原始评测核对；远端同步此前因 GitHub 443 连接失败未完成。

## 迭代素材
- 实现前发现关机阈值缺失且测试等待 200 ticks、断点可能压缩、合成中断帧可能无最高位 → 分别选择十次打印关机、首半字判断长度、清位提取原因；属于设计核对，不虚构评分失败。
- 初次只检查默认 Compiler 发行版，误判本机缺少 QEMU 4.1.1 → 枚举 WSL 发行版后发现 OS 发行版已有统一环境，显式指定发行版即可运行。
- 同步脚本硬编码源路径与当前仓库不一致 → 从当前仓库归档到独立构建目录，只叠加本次 `trap.c`，校验后编译。
- 真实软件中断补测直接写 CSR 导致旧版 GDB 异常退出（返回 -6） → 改让来宾单步执行临时 CSR 指令，再恢复原来宾指令与入口，不改仓库代码。
- 首次补测只设置 SSIP、未打开 SSIE，捕获到时钟原因码 5 → 增加软件中断局部使能，捕获真实原因码 1，并验证清挂起及原地址返回。
- 临时补测脚本内 GDB 的花括号与 Python f-string 冲突，出现 `SyntaxError: f-string: invalid syntax` → 转义脚本模板花括号后执行；不涉及内核实现变更。
- 首版终端渲染把制表符显示成方框 → 重新采集分段明确的 GDB 输出，展开制表符并按 110 列折行后渲染，已检查图片可读性。

## 留言
