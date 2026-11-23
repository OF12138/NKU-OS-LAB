# T6 分支任务：双 gdb 观察 ecall 与 sret
- 负责人：lyp    DDL：12-02    状态：待开始    依赖：T1（需要能跑通用户程序的系统调用）
- 主要文件：report/sections/T6-*, report/images/T6-*

## 要求
- 指导书「分支任务：gdb 调试系统调用以及返回」：用双 gdb（一个 attach QEMU，一个调试 ucore）观察用户态 `syscall` 中的 `ecall` 和返回用户态前的 `sret` 是如何被 QEMU 处理的，简单阅读涉及的 QEMU 源码并解释关键流程。
- 需要回答：QEMU 的指令翻译（TCG Translation）是什么，lab2 那次双 gdb 调试地址翻译是否也涉及相关内容。
- 记录调试中抓马有趣的细节、从模拟器“用软件模拟硬件”中学到的知识，以及通过大模型解决的问题（情景、思路、交互过程）。
- 用户程序符号：`add-symbol-file obj/__user_exit.out`。双 gdb 环境可参考 `.claude/tools/dual-gdb.sh` 与 lab2 分支的 `temp/gdb-guide.md`；QEMU 4.1.1 需带调试信息编译。
- 截图放 `report/images/T6-*.png`。交付：`report/sections/T6-gdb-ecall.md`。

## 当前进度 / 下一步
- 待开始。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
