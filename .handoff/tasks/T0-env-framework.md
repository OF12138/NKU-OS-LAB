# T0 环境与框架
- 负责人：openfar    DDL：10-17    状态：完成    依赖：—
- 主要文件：code/Makefile, temp/env.md

## 要求
- 导入 lab2 框架代码（labcode 页面的 lab2.zip）。
- 全组统一 QEMU 4.1.1，写安装脚本和教程（temp/env.md）。
- 约定 Buddy / SLUB 接入后如何切换分配器：默认必须是 best_fit（`make grade` 检查 Best-Fit），其他分配器通过编译开关选择。

## 当前进度 / 下一步
- [x] 导入框架（提交“T0: 导入 lab2 框架代码”）
- [x] QEMU 4.1.1 安装脚本 `.claude/tools/setup-qemu-4.1.1.sh` 与 `temp/env.md`，本机 Ubuntu 24.04 编译通过
- 分配器切换开关推迟到 T3 接入 Buddy 时实现，见 T3 任务文件。
- 最终 `make grade` 放在 T9。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
