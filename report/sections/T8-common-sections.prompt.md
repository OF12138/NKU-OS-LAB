## 公共章节：实验目的、实验环境、测试与验证、AI 协作经验

[PROMPT]

**任务**：为 lab2 实验报告撰写四个公共章节：实验目的、实验环境、测试与验证、AI 协作开发的经验。

**操作要求**：直接写入 `report/sections/T8-purpose.md`、`T8-env.md`、`T8-test.md`、`T8-summary.md`。章节标题由集成脚本添加，文件中不写一级标题。所有事实（版本号、输出内容、测试结果、协作方式）必须来自真实记录：运行结果从终端复制，截图在统一环境中实拍，成员信息向本人确认；不得编写未发生的过程或未出现的输出。

**输出要求**：Markdown，语言平实，与指导书和 `report-template.md` 的对应部分一致。图片放 `report/images/`，文件名以 `T8-` 开头，图题写成 `<p align="center">图 T8-x　…</p>`。

[RELY]

- 指导书“实验目的”：理解页表的建立和使用方法；理解物理内存的管理方法；理解页面分配算法。
- `temp/env.md`：全组统一 QEMU 4.1.1（OpenSBI v0.4），原因是框架的 `-device loader` 加载方式依赖 fw_jump 固件。
- 各成员的宿主系统、工具链、AI 工具和模型（向本人确认）。
- `make grade` 的实现：`tools/inject.py` 在 `pmm_init()` 末尾插入 `test_page_init()` 和 `test_best_fit()`，`tools/grade.sh` 检查 `[Test 1] Page Init: PASS` 和 `[Test 2] Best-Fit: PASS`。

```c
// code/tools/test_lab2.c（节选）
void
test_best_fit(void)
{
    if (pmm_manager != &best_fit_pmm_manager) {
        cprintf("[Test 2] Best-Fit: FAIL (pmm_manager=%s)\n", pmm_manager->name);
        return;
    }
    cprintf("[Test 2] Best-Fit: PASS\n");
}
```

- 各模块的验证记录：`.handoff/tasks/` 中 T1、T2、T4 的「验证结果」与「迭代素材」。

[GUARANTEE]

1. 实验目的：与指导书的三条目的对应，另加一条“用四段式提示词借助 AI 完成内核开发”。
2. 实验环境：说明统一 QEMU 版本的原因；软件环境表（成员、宿主系统、QEMU、工具链）；AI 工具表（成员、工具、模型）。
3. 测试与验证：`make qemu` 与 `make grade` 的真实截图；逐行解释 `make qemu` 输出对应的启动阶段；说明 `make grade` 两项检查各自实际检查的内容；列出各模块的补充测试及结果。
4. AI 协作经验：基于本 lab 真实发生的事例总结经验。

[SPECIFICATION]

## T8-test.md

**Pre-Condition**:
- T1、T2 已完成，统一环境下 `make grade` 可以运行。

**Post-Condition**:
- 两张截图对应已提交的代码，图中输出与正文引用的字符串完全一致。

  **Case 1**：`check best fit` 只检查 `pmm_manager` 指针，正文不得把它描述成检验分配、分裂或合并。
  **Case 2**：尚未完成的模块测试（Buddy、SLUB 的提示词二）写“待补”，不得预先编写结果。

## T8-summary.md

**Post-Condition**:
- 每一条经验都对应本 lab 中可以在仓库记录里找到的事例（环境统一、参考代码中的错误、官方测试覆盖不足、说明文字与真实输出不符、`[RELY]` 校验、注释前后的编译比对）。

**Requirements**:
- 知识点与 OS 原理的对照属于 T6，本任务不写。
