# T5 分支任务：GDB 调试 QEMU 页表查询过程
- 负责人：openfar（调用路径与页表遍历）、lyp（TLB）    DDL：10-24    状态：待开始    依赖：T0
- 主要文件：report/sections/T5-*, report/images/T5-*

## 要求
指导书原文要点（双重 GDB：一个 gdb attach 到 QEMU 进程调试 QEMU 源码，一个 riscv64 gdb 调试 ucore；QEMU 用带调试信息的 4.1.1，见 temp/env.md）：
1. 理解调试流程涉及的 QEMU 源码，给出关键调用路径及路径上关键的分支语句，并通过调试演示某个访存指令的虚拟地址如何被翻译成物理地址。（openfar）
2. 单步调试页表翻译部分，解释关键操作流程。（openfar）
3. 在 qemu-4.1.1 源码中找到模拟 CPU 查找 TLB 的 C 代码，通过调试说明细节。（lyp）
4. QEMU 模拟的 TLB 与真实 CPU 的 TLB 有什么逻辑区别（提示：对比未开启虚拟地址空间与开启后的访存语句的调用路径）。（lyp）
5. 记录调试中有趣的细节，以及观察模拟器用软件模拟硬件时了解到的知识。（各写各的）
6. 记录哪些问题通过大模型解决：当时的情景、思路、和大模型的交互过程。（各写各的）

## 当前进度 / 下一步
- openfar 部分（要求 1、2 及各自的 5、6）已完成：report/sections/T5-walk.md、T5-walk.prompt.md，截图 T5-walk-bt/pte/tlbset.png。
- 下一步：lyp 按 temp/gdb-guide.md 第 6～7 节做 TLB 部分（要求 3、4 及各自的 5、6），写在 report/sections/T5-tlb.md。T5-walk.md 中数据写路径一段已把内联查 TLB 的细节留给 TLB 部分。

## 关键决策与结论
- 地址以 1a5fd52 的内核为准：kern_init 0xffffffffc02000d6，页表 PA 0x80208000，satp 0x8000000000080208。内核再改动导致地址变化时，T5-walk.md 和截图要重新实测。
- 截图不再弹出 Windows Terminal：三个终端放在 tmux 的不同窗口里，只渲染调试 QEMU 的 gdb 窗口（.claude/tools/tmux-render.py）。

## 验证结果
- 取指：get_physical_address(addr=0xffffffffc02000d6) 的调用栈 #0～#15；第一级 idx 0x1ff、pte_addr 0x80208ff8、pte 0x200000cf 为叶子；physical 0x80200000；tlb_set_page vaddr 0xffffffffc0200000、paddr 0x80200000、prot 7、size 4096。
- 数据写：sd ra,8(sp) 写 0xffffffffc0207ff8，路径 helper_le_stq_mmu → store_helper → tlb_fill，val = $ra = 0x80000a02。
- satp 现象：0x80200038 命中开启分页前的旧 TLB 表项（write_satp 只在 ASID 变化时 flush）。

## 迭代素材
- 第一版教程的地址来自 WSL 中过时的代码副本（在 Git Bash 里误跑 sync-wsl.sh，静默失败）→ 从编译列表缺少 buddy_pmm.c 发现 → 给脚本加运行环境检查，全部实验在新代码上重测。
- 截图：tmux send-keys 把 gdb 命令 `delete` 当成 Delete 键名，后续输入错乱 → 改用 send-keys -l 按字面发送；gdb 提示符下发 Ctrl-L 会留下 `quit` 残行 → 不清屏，改从窗格历史中按起止文本截取。

## 留言
