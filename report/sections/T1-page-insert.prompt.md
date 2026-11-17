# T1 虚拟内存页操作：最终技术提示词

本文件按已完成的接口设计和验证结果整理，用于复现实验功能。

## 建立、更新与撤销页映射

[PROMPT]

任务：在 lab4 的 `code/kern/mm/pmm.c` 中实现 `page_insert` 与 `page_remove`，为现有 Sv39 页表提供物理页映射维护。直接修改真实文件，保留框架及现有学习注释，将本模块标记写为 `//LAB4 function1 2414099`。仿照 `get_pte` 的函数说明格式，补充中文功能描述、parameter 参数说明和 return value 返回值说明。复用现有接口，不改评测脚本及测试函数。输出实现、处理分支的说明和实际验证结果。

[RELY]

以下摘自框架导入提交 `a7043b3`，省略注释；不引用本任务实现。`get_pte` 定位末级页表项，`create` 为真时可分配中间页表，分配失败返回 `NULL`；`page_ref_inc/dec` 返回修改后的引用计数；`pte_create` 自动设置 `PTE_V`。使用框架已有的 `tlb_invalidate` 刷新对应地址的 TLB。

`code/kern/mm/memlayout.h`：

```c
typedef uintptr_t pte_t;
typedef uintptr_t pde_t;
struct Page {
    int ref;
    uint_t flags;
    unsigned int property;
    list_entry_t page_link;
    list_entry_t pra_page_link;
    uintptr_t pra_vaddr;
};
```

`code/kern/mm/pmm.h` 中相关声明与宏：

```c
extern struct Page *pages;
extern pde_t *boot_pgdir_va;
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
```

`code/kern/mm/pmm.h` 中相关内联接口（节选）：

```c
static inline ppn_t
page2ppn(struct Page *page)
{
    return page - pages + nbase;
}
static inline struct Page *
pte2page(pte_t pte)
{
    if (!(pte & PTE_V))
    {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}
static inline int
page_ref_inc(struct Page *page)
{
    page->ref += 1;
    return page->ref;
}
static inline int
page_ref_dec(struct Page *page)
{
    page->ref -= 1;
    return page->ref;
}
static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}
```

`code/kern/mm/mmu.h` 和 `code/libs/error.h` 中权限位与错误码：

```c
#define PTE_V     0x001
#define PTE_R     0x002
#define PTE_W     0x004
#define PTE_U     0x010
#define E_NO_MEM            4
```

[GUARANTEE]

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

[SPECIFICATION]

`page_insert`：

- Pre-Condition：`pgdir` 是有效根页表；`page` 是已分配、可由框架管理的物理页；已有映射引用计数一致；`perm` 是调用者给出的权限组合；沿用框架单核执行条件与普通页映射约定。
- Post-Condition：成功返回 0，目标虚拟页映射到 `page`，末级 PTE 使用 `pte_create(page2ppn(page), perm)` 构造，引用计数与有效映射一致，更新后使对应 TLB 失效；定位 PTE 失败返回 `-E_NO_MEM`，不增减目标页引用、不改旧数据映射。
- Case 1：目标未映射，新增一次引用并写入 PTE。
- Case 2：目标已映射同一物理页，只更新权限，最终引用计数和空闲页数不变，不能释放该页。
- Case 3：目标映射其他物理页，移除旧映射并减少旧页引用；旧页引用归零时释放，仍有其他引用则保留；目标页新增一次引用。
- Case 4：`get_pte` 分配失败，不产生目标地址的有效末级映射。已有辅助函数可能保留已建成的中间页表，不要求修改其分配策略或回滚其他级页表。
- Requirements：不得把 `perm=0` 擅自改成读写权限；保留框架测试的 PTE 表示约定。先获得新页引用再处理旧映射，防止同页重复插入时误释放。

`page_remove`：

- Pre-Condition：根页表有效，已有映射引用计数一致。
- Post-Condition：若存在有效末级 PTE，则清零、刷新对应 TLB、减少物理页引用，并仅在引用归零时释放；若不存在有效映射则直接返回。
- Case 1：缺中间页表，不分配任何页表或数据页。
- Case 2：PTE 存在但无效，重复调用无副作用。
- Case 3：同页有多地址映射，只撤销当前映射；最后一个引用撤销后释放该物理页。
- Requirements：复用 `get_pte(pgdir, la, 0)`；不回收中间页表，避免影响其他映射；在物理页交还分配器之前清除 PTE 并使 TLB 失效。

验证：运行框架 `check_pgdir`、`check_boot_pgdir` 和未修改的官方 `test_lab4_pmm_basic`；补充实际地址访问、同页权限更新、共享页替换、分配失败、重复删除与空闲页数恢复检查。若其他任务尚未实现，在独立副本使用明确标注的启动桩验证 T1，并将模块得分与最终集成得分分别说明。
