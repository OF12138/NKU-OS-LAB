# T3 核心模块理解
- 负责人：lyp    状态：待审    依赖：无
- 可改文件：report/sections/T3-*.md

## 要求
- 对应报告要求：说明自己对每个功能的核心函数或功能模块的理解。
- 覆盖以下三块：① tools/kernel.ld 与内存布局（入口地址、各段顺序、edata/end 等符号，以及为什么 kern_entry 恰好位于 0x80200000，可以用 nm 验证）；② 输出链：cprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → ecall；③ 构建流程：从 Makefile 到 bin/kernel（ELF）再到 bin/ucore.img（objcopy 生成的纯二进制），说明两者的区别。

## 当前进度 / 下一步

- 2026-10-08 同步：修订已提交为 dc57511；GitHub HTTPS 推送多次失败（curl 55 / 443 连接超时），尚未确认上传。网络恢复后先同步远端再推送；本地 T7 未提交修改保持原样。原文件及历史完整备份于 F:/Lab_2026/NKU-OS-LAB-backup-20261008-164350。

- 已按用户要求撤销 T2 图片复用，保留重写后的正文与最终提示词。已在真实 VSCode / tmux 中重新运行 GDB/readelf/nm，替换旧 HTML 截图为 T3-ecall-step.png、T3-elf-layout.png、T3-symbols.png 并配图分析。
- 状态：待审。下一步由 openfar 审核集成，统一图号；用户要求 lyp 独立补图，本轮新增图片由 lyp 维护。

## 关键决策与结论

- 当前 entry.S 使用普通 .text；entry.o 的链接输入顺序决定入口在首部，ENTRY 只设置 ELF 入口，不自动排序。
- 源码输出链包括 vcprintf/sbi_call；优化产物中两者已内联，格式化在 S 态，ecall 后由 M 态固件输出。
- make qemu 仍构建 ucore.img，但实际 -kernel 加载 ELF；QEMU 在 CPU 执行前完成装载，本实验 make grade 不适用。

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

- 首版偏重接口清单和核验声明，工具故障也进入正文 → 实验报告与协作记录混在一起 → 正文改为代码、观察、解释，环境调用问题保留在任务文件。

- 文档检查发现原样摘录 SBI 全局变量行也复制了源码尾空格 → git diff --check 报错 → 改用原样、无尾空格的 sbi_console_putchar 函数作为最小上下文，提示词与正文同步更新。

- Windows 到 WSL 的 bash -c 参数发生引号解析错误，随后 stdin 末尾 CR 被当成命令 → 改用 stdin 脚本及 Python 子进程传参，构建和 GDB 验证完成。
- 未确认用户时尝试 /home/lyp 导出遇到 Permission denied → 查询实际 WSL 用户 lenovo，改用其 Code/NKU-OS-LAB/lyp-lab1 目录。
- 在线指导书和参考报告称 OpenSBI 加载内核 → GDB 在复位处已读到入口指令 → 报告采用当前 -kernel 的 QEMU 装载事实。
- vcprintf/sbi_call 未出现在最终 nm 中 → -O2 内联与 --gc-sections → 区分源码调用链与实际符号。

- 2026-10-07 文档检查：section 本地链接、代码围栏、最终提示词完整性及 [RELY] 源码逐行一致检查通过；git diff --check 通过。

## 留言
- [openfar 2026-10-07] 审核通过，状态改为完成。已用本机构建核对 edata=end=0x80203008、栈区间与 [RELY] 引文；图号（图 T1-x/T3-x）和 section 内的相对链接在集成时统一处理。
- [openfar 2026-10-07] 代码加入学习注释后，已代为修改你提示词中的 [RELY] 说明（注明“省略注释”；T4 的 kern_init 大括号换行已同步）。代码本身的引文均经脚本逐行核对一致。
