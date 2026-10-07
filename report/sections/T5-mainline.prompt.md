## 实验整体逻辑分析

[PROMPT]

**任务**：撰写 lab1 实验报告的「三、实验整体逻辑分析」，写入 `report/sections/T5-mainline.md`，包括「本章节的逻辑主线」和「功能的逐步实现」两部分。

**操作要求**：直接写入真实的报告文件，不修改 `code/` 下的任何文件。论述要以框架代码和已完成的练习 1、练习 2 及功能模块分析为依据；与这些 section 中的结论保持一致，不重复它们的细节，而是指明对应的位置。

**输出要求**：使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中的信息，按 `report-template.md` 第三部分的格式组织。

[RELY]

```asm
# code/kern/init/entry.S（省略注释）
kern_entry:
    la sp, bootstacktop

    tail kern_init
```

```c
// code/kern/init/init.c（省略行尾注释）
int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
   while (1)
        ;
}
```

```ld
/* code/tools/kernel.ld（省略行尾注释） */
ENTRY(kern_entry)

BASE_ADDRESS = 0x80200000;
```

- 已完成的分析：练习 1（入口建栈与尾跳转）、练习 2（复位代码 → OpenSBI → mret 交接，以及 SBI 调用链）、功能模块（链接脚本与内存布局、格式化输出与 SBI 服务、构建与镜像加载）、对实验框架的修改（新版 QEMU 需要 `-kernel` 加载 ELF）。
- 实测结论：进入内核时 sp 仍指向被 PMP 保护的 OpenSBI 栈；本次构建 `edata = end`，BSS 为空。

[GUARANTEE]

必须交付的内容：

1. 本章的核心主题，以及它要解决的问题。
2. 一条贯穿全章的主线，说明各部分之间的关系。
3. 按顺序列出本章涉及的各项功能，说明每一步为什么排在这个位置，并给出对应的代码文件和报告位置。

[SPECIFICATION]

## 逻辑主线

**Pre-Condition**:
- 练习 1、练习 2 和功能模块的 section 已经完成并通过审核。

**Post-Condition**:
- 用“控制权的接力”串起复位代码、OpenSBI、`entry.S`、`kern_init` 和 SBI 输出，并指出每一棒为下一棒准备了什么条件。
- 指出构建时的地址约定（`0x80200000`、`kern_entry`）与运行时的跳转目标必须一致，并以 T0 中遇到的问题为例说明。

## 功能的逐步实现

**Post-Condition**:
- 按“构建与链接 → 装载与固件交接 → 入口建栈 → C 运行环境初始化 → 格式化输出”的顺序展开，每一步都说明它依赖前面哪些步骤。

  **Case 1**:
  - lab1 不需要编写新代码，要说明这里的“实现顺序”指的是框架中各层功能被构建和执行的顺序，而不是我们的编码顺序。

  **Case 2**:
  - 涉及 BSS 清零时，要如实说明本次构建中 BSS 为空，同时解释这一步对一般内核的必要性。

**Requirements**:
- 结论必须与其他 section 一致，不引入它们没有验证过的说法。
- 结尾说明当前内核停在死循环中的原因，以及后续实验将补充的内容。
