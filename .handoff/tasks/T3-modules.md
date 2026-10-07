# T3 核心模块理解
- 负责人：lyp    状态：进行中    依赖：无
- 可改文件：report/sections/T3-*.md

## 要求
- 对应报告要求：说明自己对每个功能的核心函数或功能模块的理解。
- 覆盖以下三块：① tools/kernel.ld 与内存布局（入口地址、各段顺序、edata/end 等符号，以及为什么 kern_entry 恰好位于 0x80200000，可以用 nm 验证）；② 输出链：cprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → ecall；③ 构建流程：从 Makefile 到 bin/kernel（ELF）再到 bin/ucore.img（objcopy 生成的纯二进制），说明两者的区别。

## 当前进度 / 下一步

- 已沿源码检查输出链，核对 Makefile、链接输入顺序和 ELF 段表；已整理三个独立模块的提示词。
- 已在本机 QEMU 6.2.0 / OpenSBI v0.9 验证 make qemu 输出加载信息；与 T2 的 8.2.2 环境分开记录。
- 下一步：完成链接布局、输出、构建三块报告，并附可复现命令和关键输出。

## 关键决策与结论

## 验证结果

- 2026-10-07：WSL Ubuntu-22.04-OS，SiFive GCC 10.2.0，make -j2 成功；objdump、nm、readelf 完成。GDB 批处理退出 0；make qemu 输出预期信息，8 秒后 timeout 以 124 停止持续运行的内核。

## 迭代素材

- Windows 到 WSL 的 bash -c 参数发生引号解析错误，随后 stdin 末尾 CR 被当成命令 → 改用 stdin 脚本及 Python 子进程传参，构建和 GDB 验证完成。
- 未确认用户时尝试 /home/lyp 导出遇到 Permission denied → 查询实际 WSL 用户 lenovo，改用其 Code/NKU-OS-LAB/lyp-lab1 目录。
- 在线指导书和参考报告称 OpenSBI 加载内核 → GDB 在复位处已读到入口指令 → 报告采用当前 -kernel 的 QEMU 装载事实。
- vcprintf/sbi_call 未出现在最终 nm 中 → -O2 内联与 --gc-sections → 区分源码调用链与实际符号。

## 留言
