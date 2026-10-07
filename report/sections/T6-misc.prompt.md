# T6 任务交付提示词 (Prompt Handoff)

## 提示词 1：现代笔记本启动流程横向对比分析推演

[PROMPT]
任务：分析现代笔记本启动流程（UEFI 规范）并与 RISC-V 真实硬件启动链进行横向对比，输出报告章节至 report/sections/T6-extension.md。
操作要求：严格依据 T2 练习对 RISC-V 启动链的实测结论，提炼计算机固件引导的通用设计哲学；采用严谨的技术文档风格，禁止使用口语化表达。

[RELY]
- 团队 T2 启动链结论：
  加电复位 0x1000 (MROM) -> 0x80000000 (OpenSBI) -> mret 降权跳转 0x80200000 (S-Mode kern_entry)。
- 固件交接数据结构：
  struct fw_dynamic_info {
      unsigned long magic;
      unsigned long version;
      unsigned long next_addr;
      unsigned long next_mode;
      unsigned long options;
      unsigned long boot_hart;
  };
- 真机启动拓扑：
  片上 ROM（ZSBL） -> U-Boot SPL -> OpenSBI（M 态） -> U-Boot（S 态） -> Linux 内核（S 态）
- 工业规范：UEFI Specification v2.10, ARM Trusted Firmware Design (TF-A)。

[GUARANTEE]
必须完整产出以下内容并写入文件：
1. 现代 x86 UEFI 规范的五大阶段时序解析（SEC -> PEI -> DXE -> BDS -> OS Loader -> ExitBootServices）。
2. RISC-V 真机、x86 UEFI 与 ARM64 TF-A 的五层横向对比映射表格。
3. 计算机底层引导的两大核心设计哲学（最小依赖原则、特权级降级收敛）。

[SPECIFICATION]
- Pre-Condition: T2-ex2-gdb.md 中关于 MROM、OpenSBI 以及真机启动拓扑的分析已完成。
- Post-Condition: 生成 report/sections/T6-extension.md，符合 Markdown 标准规范，无多余外层代码块嵌套。
- Requirements: 对比维度必须覆盖特权级别、无 DRAM 阶段处理机制、运行时服务请求通道（SBI / SMI / SMC）。

---

## 提示词 2：实验报告非代码核心章节系统化构建

[PROMPT]
任务：依据 NKU-OS-LAB 规约，生成实验目的、实验环境表、QEMU 运行记录及实验总结初稿，分别写入对应的 report/sections/T6-*.md 文件中。
操作要求：实验环境表严格填报张远（2411264）、李云鹏（2414099）、刘昀皓（2413074）三位成员的软硬件与 AI 协同矩阵；运行记录引用真实截图 ../images/T6-qemu-run.png 并注明 QEMU 7.0.0 版本；实验总结采用团队视角并预留成员补充槽位。

[RELY]
- 团队规范：AGENTS.md, report-template.md, .handoff/tasks/T6-misc-sections.md
- 内核入口与字符输出的声明（原样摘自 code/kern/init/init.c、code/libs/stdio.h、code/libs/sbi.h，省略行尾注释）：
```c
int kern_init(void) __attribute__((noreturn));
int cprintf(const char *fmt, ...);
void sbi_console_putchar(unsigned char ch);
```
- 实测参数：Ubuntu 22.04 LTS (WSL2), QEMU emulator version 7.0.0, OpenSBI v1.0。
- 其他成员的环境：张远为 QEMU 8.2.2 / OpenSBI v1.3 / GCC 15.1.0，李云鹏为 QEMU 6.2.0 / OpenSBI v0.9 / GCC 10.2.0。

[GUARANTEE]
必须交付以下规范文件：
1. report/sections/T6-purpose.md：包含 4 条实验目的，分别对应启动流程与特权级切换、内核构建与内存布局、启动栈 / BSS 清零与 SBI 输出、GDB 调试；只写 lab1 实际涉及的内容（本实验没有开启分页，也没有内核的中断处理）。
2. report/sections/T6-env.md：包含三位成员各自的软件环境表（QEMU 及自带 OpenSBI 的版本、工具链）和符合 report-template.md 规范的 AI 工具表，每一项都要与成员实际使用的情况一致。
3. report/sections/T6-qemu-run.md：包含环境声明、流程分析及内嵌 ../images/T6-qemu-run.png 的截图引用。
4. report/sections/T6-summary.md：以团队视角出发的总结初稿，包含启动时序认知、异步工程协同及人机协同反思槽位。

[SPECIFICATION]
- Pre-Condition: 本地通过 make qemu 跑通内核并已将截图归档至 report/images/T6-qemu-run.png。
- Post-Condition: 所有生成的 section 文件均可在本地正确打开，图片引用路径统一为 ../images/T6-qemu-run.png。
- Requirements: 保持纯正技术文档语言，杜绝主观口语化修饰。