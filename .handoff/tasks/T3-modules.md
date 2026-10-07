# T3 核心模块理解
- 负责人：lyp    状态：待审    依赖：无
- 可改文件：report/sections/T3-*.md

## 要求
- 对应报告要求：说明自己对每个功能的核心函数或功能模块的理解。
- 覆盖以下三块：① tools/kernel.ld 与内存布局（入口地址、各段顺序、edata/end 等符号，以及为什么 kern_entry 恰好位于 0x80200000，可以用 nm 验证）；② 输出链：cprintf → vprintfmt → cputch → cons_putc → sbi_console_putchar → ecall；③ 构建流程：从 Makefile 到 bin/kernel（ELF）再到 bin/ucore.img（objcopy 生成的纯二进制），说明两者的区别。

## 当前进度 / 下一步

- 已完成 report/sections/T3-modules.md 的链接布局、输出链和构建三模块，三个最终提示词已内嵌并同步到 .prompt.md。
- 状态：待审。下一步由 openfar 审核，按功能模块合并入 report.md；提示词合并入 prompt.md。

- Git 交付：053629d（首批提示词/验证进展）已推送；c90afcc（最终报告/T7）已提交，但 GitHub 443 连接连续失败，尚未推送。下一步网络恢复后先 git pull --rebase，再 git push，之后由 openfar 审核集成。

## 关键决策与结论

- 当前 entry.S 使用普通 .text；entry.o 的链接输入顺序决定入口在首部，ENTRY 只设置 ELF 入口，不自动排序。
- 源码输出链包括 vcprintf/sbi_call；优化产物中两者已内联，格式化在 S 态，ecall 后由 M 态固件输出。
- make qemu 仍构建 ucore.img，但实际 -kernel 加载 ELF；QEMU 在 CPU 执行前完成装载，本实验 make grade 不适用。

## 验证结果

- 2026-10-07：WSL Ubuntu-22.04-OS，SiFive GCC 10.2.0，make -j2 成功；objdump、nm、readelf 完成。GDB 批处理退出 0；make qemu 输出预期信息，8 秒后 timeout 以 124 停止持续运行的内核。

## 迭代素材

- 文档检查发现原样摘录 SBI 全局变量行也复制了源码尾空格 → git diff --check 报错 → 改用原样、无尾空格的 sbi_console_putchar 函数作为最小上下文，提示词与正文同步更新。

- Windows 到 WSL 的 bash -c 参数发生引号解析错误，随后 stdin 末尾 CR 被当成命令 → 改用 stdin 脚本及 Python 子进程传参，构建和 GDB 验证完成。
- 未确认用户时尝试 /home/lyp 导出遇到 Permission denied → 查询实际 WSL 用户 lenovo，改用其 Code/NKU-OS-LAB/lyp-lab1 目录。
- 在线指导书和参考报告称 OpenSBI 加载内核 → GDB 在复位处已读到入口指令 → 报告采用当前 -kernel 的 QEMU 装载事实。
- vcprintf/sbi_call 未出现在最终 nm 中 → -O2 内联与 --gc-sections → 区分源码调用链与实际符号。

- 2026-10-07 文档检查：section 本地链接、代码围栏、最终提示词完整性及 [RELY] 源码逐行一致检查通过；git diff --check 通过。

## 留言
