```markdown
[PROMPT]
任务：针对操作系统 Lab 3 指导书中的两道思考题，进行系统、严谨且深入的原理剖析与报告撰写，输出符合 report-template.md 格式的 Markdown 文档。
操作要求：直接基于框架源码与 RISC-V 官方特权级规范，从硬件规范、编译器文法与操作系统内核并发机制三个维度展开深度解答。
输出要求：按层次清晰给出背景、问题现象、原理解析与场景反例，作为报告中的「练习：指导书思考题」小节。

[RELY]
// 1. RISC-V Privileged Architecture Specification (v1.9 vs v1.10) 关于 stvec 的定义演进：
// v1.9: stvec 为 SXLEN 位寄存器，保存 4 字节对齐的 trap vector base address，最低 2 位硬件固定为 0。
// v1.10+: stvec 拆分为 BASE[XLEN-1:2] 与 MODE[1:0]：
//   MODE=0 (Direct): 所有异常与中断统一跳转到 BASE
//   MODE=1 (Vectored): 同步异常跳转到 BASE，异步中断跳转到 BASE + 4 * cause
//   MODE>=2: Reserved

// 2. kern/sync/sync.h 中的中断保护宏与辅助内联函数：
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

// 3. kern/mm/pmm.c 中内存分配/释放对中断保护宏的使用：
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

[GUARANTEE]
必须系统、全面地回答指导书正文提出的两道核心思考题，不得遗漏任何子问题：
1. 思考题 1：stvec 最低两位的模式编码是 1.10 版才加入的，这个改动如何保证与旧版本兼容？
2. 思考题 2：sync.h 中 local_intr_save 宏用 do { } while (0) 包起来有什么作用？结合 pmm.c 的 alloc_pages / free_pages 说明为什么要先关中断再操作。

[SPECIFICATION]
针对思考题 1（stvec 向后兼容性）：
- Case 1（编码与数值兼容）：说明旧规范对基地址 4 字节对齐的要求，推导旧软件写入的地址最低两位必然为 00，如何天然匹配 1.10 中 MODE=0（Direct 模式）的编码。
- Case 2（地址计算一致性）：分析 1.10 在 Direct 模式下的地址计算公式（BASE << 2），证明其计算出的目标 PC 与旧软件预期地址严格相等。
- Case 3（控制流与架构回退）：分析单入口统一分发控制流模型的承接关系，以及硬件层面将最低两位硬连为 0 时的实现兼容性。

针对思考题 2（宏封装与关中断保护）：
- Case 1（do-while 语法保护）：给出不使用 do-while(0) 时在 if-else 单分支场景下的具体反例与预处理器展开结果，指出末尾多余分号导致编译器报 'else without a previous if' 错误的原因；说明局部作用域隔离与零运行时开销。
- Case 2（临界区与非原子操作）：结合 pmm_manager 的双向空闲链表 free_list、nr_free 计数及 Page 描述符的增删改、拆分与合并过程，说明多步操作非原子性导致的中间不一致状态。
- Case 3（中断重入危害）：详细推演在单核环境下，若不关中断导致异步中断打断 alloc_pages/free_pages，当中断服务程序或调度换入的新线程再次调用内存分配时，产生的双向链表破损、死循环、内核 Page Fault、Double Allocation 等致命故障。
- Case 4（嵌套安全性）：分析 __intr_save() 保存原始 SIE 标志位再关中断、__intr_restore() 按标志恢复的设计对支持临界区嵌套调用的重要意义。
```
