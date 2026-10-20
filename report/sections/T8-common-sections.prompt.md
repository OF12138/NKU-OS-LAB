# T8 反推提示词（Final Prompt）

本任务涵盖实验报告公共章节的起草与标准化整理，包含实验目的、环境配置、测试评测体系设计与实验总结反思两份标准化四段式提示词。

---

## 提示词一：实验目的与环境规范章节整理

```markdown
[PROMPT]
任务：根据操作系统 Lab 2 指导书及全组约定的环境规范（temp/env.md），梳理撰写实验报告中的“一、实验目的”与“二、实验环境”章节。
操作要求：紧密结合 RISC-V 64 Sv39 分页架构与 ucore 内核物理内存管理特性，清晰界定软硬件基础设施配置与三人 AI 协同矩阵。
输出要求：Markdown 格式，结构严谨，表格规范，严格零 Emoji，符合学术与工程报告排版标准。

[RELY]
// 来自 kern/mm/pmm.h
struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

extern const struct pmm_manager *pmm_manager;

void pmm_init(void);
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

[GUARANTEE]
必须覆盖以下内容要点：
1. 实验目的：
   - RISC-V Sv39 分页机制与物理内存转换（高半核映射与 `va_pa_offset` 换算）；
   - 设备树（DTB）动态内存探测与 `page_init` 物理页生命周期初始化；
   - 多策略连续内存分配算法（First-Fit / Best-Fit / Buddy System）；
   - 内核小对象缓存管理（SLUB 分配器）；
   - 软硬件联合仿真与 GDB 调试 QEMU 页表遍历；
   - 基于四段式契约的 AI 协同研发范式。
2. 实验环境：
   - 软硬件环境表：Ubuntu 24.04 LTS、统一 QEMU 4.1.1（OpenSBI v0.4）、`riscv64-unknown-elf-gcc` 工具链、GDB 调试器；
   - 团队协同矩阵：包含张远（openfar）、李云鹏（lyp）、刘昀皓（nagilix）三人的编程工具、底层模型与具体分工。

[SPECIFICATION]
- 标题层级严格遵循实验报告规范（一级标题为“# 一、实验目的”、“# 二、实验环境”）；
- 严禁包含防御性免责表述与任何表情符号；
- 专有名词采用标准中英文混排，中英文之间保留单一空格。
```

---

## 提示词二：测试评测体系设计与实验总结反思

```markdown
[PROMPT]
任务：梳理并起草操作系统 Lab 2 报告中的“五、测试与验证”框架及“六、实验总结与收获”初稿。
操作要求：深入剖析内核常态运行与自动化脚本评分（make grade）的评测机制；全面对照操作系统原理与 ucore 具体实现，列举未涉及的高级机制，并提炼团队在 AI 协同开发过程中的核心方法论。
输出要求：Markdown 格式，包含清晰的对比表格与深度工程感悟，严格零 Emoji。

[RELY]
// 来自 kern/mm/pmm.h
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

// 来自 kern/init/init.c
pmm_init();
slub_init();

[GUARANTEE]
必须回答或交付以下模块：
1. 测试与验证：
   - 阐明 `make qemu` 与 `make grade` 注入探针的评测机制（Page Init PASS 与 Best-Fit PASS 判据）；
   - 规范截图占位符与图题说明（预留待 T2 合并后回填）。
2. 对操作系统的理解：
   - 构建核心知识点与 OS 原理概念映射表（物理页帧管理、连续分配算法、伙伴系统、SLUB 小对象缓存、Sv39 分页）；
   - 深入分析至少 4 项本实验未涉及的工业级 OS 高级机制（反向映射 rmap、页面置换与换页、内存碎片紧凑迁移、写时复制 COW）。
3. AI 协作开发的经验：
   - 契约驱动研发模式（Contract-Driven Development）；
   - 文件级严格隔离与边界划分（Hard Execution Boundaries）；
   - 答辩防御性思维（Defense-Readiness）；
   - 统一基础设施环境对杜绝幽灵排障的重要性。

[SPECIFICATION]
- 对比表格必须包含“实验知识点”、“对应 OS 原理概念”、“含义、关系与设计差异分析”三列；
- 所有论述必须具有高度工程严谨性与理论说服力。
```
