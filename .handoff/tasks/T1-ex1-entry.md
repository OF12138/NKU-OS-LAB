# T1 练习1：理解内核启动中的程序入口操作
- 负责人：lyp    状态：待审    依赖：无
- 可改文件：report/sections/T1-*.md

## 要求
- 指导书原题：阅读 kern/init/entry.S，结合内核启动流程，说明 `la sp, bootstacktop` 完成了什么操作、目的是什么；`tail kern_init` 完成了什么操作、目的是什么。
- 答案要以实际代码和反汇编为准（`riscv64-unknown-elf-objdump -d bin/kernel`），说明伪指令展开后的真实指令。
- 可以展开的方向：栈的大小和对齐方式（memlayout.h / mmu.h）；为什么 bootstack 放在 .data 段而不是 .bss 段（提示：kern_init 一开始就把 edata 到 end 这段内存清零）；tail 和 call 的区别，以及它和 kern_init 的 noreturn 属性有什么关系。

## 当前进度 / 下一步

- 2026-10-08 同步：修订已提交为 dc57511；GitHub HTTPS 推送多次失败（curl 55 / 443 连接超时），尚未确认上传。网络恢复后先同步远端再推送；本地 T7 未提交修改保持原样。原文件及历史完整备份于 F:/Lab_2026/NKU-OS-LAB-backup-20261008-164350。

- 已按用户要求撤销 T2 图片复用，保留重写后的正文与最终提示词。已在真实 VSCode / tmux / GDB 中重新调试，替换旧 HTML 截图并生成 report/images/T1-entry-step.png，已配图解释 sp 和 ra。
- 状态：待审。下一步由 openfar 审核集成，统一图号；用户要求 lyp 独立补图，本轮新增图片由 lyp 维护。

## 关键决策与结论

- la 加载地址，最终 auipc sp,0x3 + addi sp,sp,0；tail 在 entry.o 为 auipc/jalr x0，最终松弛为 c.j，跳转前后 ra 不变。
- 栈范围 [0x80201000,0x80203000)，8192 字节；本次 edata=end=0x80203008，清零长度为零。
- 地址取自 SiFive GCC 10.2.0 / QEMU 6.2.0 / OpenSBI v0.9，固件地址不与 T2 的 8.2.2 产物混用。

## 验证结果

- 2026-10-08：在独立 worktree 基于远端最新 lab1 整理本地修订，清理 STATUS/T3 的残留冲突；保留其他成员提交及总报告引用的旧 JPG。下一步由 openfar 审核新 section 并更新总报告。

- 2026-10-07：在用户打开的 VSCode / WSL Ubuntu-22.04-OS / tmux 会话中，通过 tmux 命令控制真实 GDB，原生捕获 VSCode 窗口后仅裁剪终端区域；四张新 PNG 替换旧 HTML 展示页 JPG。未复用或修改 T2 图片。
- T1：入口断点 0x80200000；两次 si 后 sp=0x80203000；tail 后 PC=0x8020000a，ra=0x800078cc 不变。T3：ecall 前 priv=1、a7=1、a0=0x28；单步后 priv=3、mcause=9、mepc=0x80200492、mtvec=PC=0x80000520；返回 PC=0x80200496、priv=1，QEMU 输出字符 `(`。
- 当前 bin/kernel 的 nm 与 readelf -W -l/-S 已重新执行，段和符号地址与正文一致。四张裁剪图已逐图打开核对，命令、关键输出完整保留。
- 原构建验证：SiFive GCC 10.2.0，make -j2 成功；make qemu 输出加载提示，8 秒后 timeout 以124停止内核。此次截图未修改代码。

## 迭代素材

- 本地领先 1 个提交、落后 11 个提交且文件混有远端内容 → 分支与工作区不同步，留下文本冲突标记 → 先完整备份，再基于远端整理 lyp 的修改，避免重复提交其他成员内容。

- 用户指出 HTML 展示页截图不符合要求 → 通过真实 tmux 会话单步 GDB，原生捕获 VSCode 窗口并裁剪终端区域，正文与提示词同步更正。

- 用户明确要求不复用 T2 图片 → 已删除全部复用引用和对应解读，改为独立运行 lyp 本机实验并采集输出展示页截图。

- 首版只写单步结果，没有引用现成截图 → 将采集分工误当成章节无需配图 → 补入入口换栈实测截图、图题和逐图分析，删除审计式声明。

- 最终 tail 并非固定 auipc/jalr → 链接松弛生成 c.j → 同时解释目标文件重定位与最终机器码。
- 本次 edata=end=0x80203008 → 无存活 BSS 内容 → 区分清零语义和本次零长度调用。

- 2026-10-07 文档检查：section 本地链接、代码围栏、最终提示词完整性及 [RELY] 源码逐行一致检查通过；git diff --check 通过。

## 留言
- [openfar 2026-10-07] 审核通过，状态改为完成。已用本机构建核对 edata=end=0x80203008、栈区间与 [RELY] 引文；图号（图 T1-x/T3-x）和 section 内的相对链接在集成时统一处理。
- [openfar 2026-10-07] 代码加入学习注释后，已代为修改你提示词中的 [RELY] 说明（注明“省略注释”；T4 的 kern_init 大括号换行已同步）。代码本身的引文均经脚本逐行核对一致。
