#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>
#include <dtb.h>

// pages指针保存的是第一个Page结构体所在的位置，也可以认为是Page结构体组成的数组的开头
//数组起点应位于end之后并按页对齐
struct Page *pages;
size_t npage = 0; //npage表示内核可管理的物理页号上界

uint_t va_pa_offset; // The kernel image is mapped at VA=KERNBASE and PA=info.base

// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE; //nbase表示 DRAM 起始位置对应的物理页号，因此描述符数组需要覆盖[nbase, npage)中的每个物理页

// virtual address of boot-time page directory
pde_t *boot_pgdir_va = NULL;
// physical address of boot-time page directory
uintptr_t boot_pgdir_pa;
/*
【lab4 新增】内核页表的根（Sv39 的一级页表）。
它不是动态分配出来的：就是 entry.S 里静态写在 .data 段中的 boot_page_table_sv39（4KiB，
随内核镜像一起被加载到物理内存），pmm_init 里把它的虚拟地址存入 boot_pgdir_va、物理地址存入 boot_pgdir_pa。
lab2/lab3 里对应的变量叫 satp_virtual / satp_physical，只是打印一下，没有拿来用；lab4 开始才真正往里面增删映射。
*/

// physical memory management
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
// 【lab2】选定物理内存管理器（这里是 default_pmm_manager，即 default_pmm.c 里的实现），
// 打印名字，并调用它的 init()（初始化空闲链表 free_list 和空闲页计数 nr_free）。
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
// 【lab2】把一段连续的物理页 [base, base+n) 交给管理器，由它把这些页挂进空闲链表。
// 只在 page_init 里调用一次，之后空闲页只通过 alloc_pages / free_pages 进出。
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
// 【lab2】分配 n 个连续物理页，返回第一页的 Page 描述符（失败返回 NULL）。注意返回的是描述符，不是地址，
// 要用 page2pa / page2kva 换算。管理器本身不是可重入的，所以外面套一层关中断，
// 避免分配过程中被时钟中断打断而破坏空闲链表。lab4 里 get_pte 分配页表页、kmalloc 分配内核栈都走这里。
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag); //先关中断

    {
        page = pmm_manager->alloc_pages(n);
    }

    local_intr_restore(intr_flag); //再开中断
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
// 【lab2】把 [base, base+n) 还给管理器（会尝试和相邻空闲块合并）。同样关中断保护。
// lab4 的 page_remove_pte 在引用计数减到 0 时调用它（经 free_page 宏）。
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);

    {
        pmm_manager->free_pages(base, n);
    }

    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
// 【lab2】返回当前空闲页的总数。lab4 的 check_pgdir / check_boot_pgdir 用它做“泄漏检查”：
// 测试前后空闲页数必须相等，否则说明有页表页或数据页没释放。
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码
/*
可以把初始化后的物理内存布局理解为：
OpenSBI 
占用区之后依次是内核镜像、
必要的对齐空隙、
Page描述符数组、
再次按页对齐产生的空隙，
以及最终可以分配的连续物理页。
物理内存末尾不足一页的部分同样不能交给分配器。

Q：where is the data of device tree? 
3. 我们的环境（QEMU virt）

没有现成的 .dtb 文件，QEMU 启动时根据模拟的机器配置（内存大小、CPU 数、串口等）现场生成设备树，写进客户机 RAM，再由 OpenSBI 把地址传给内核。位置一般在物理内存靠后的位置，地址由 boot_dtb 决定，dtb_init 会把它打印出来。

想看内容的话，可以让 QEMU 导出：


qemu-system-riscv64 -machine virt,dumpdtb=virt.dtb -nographic
dtc -I dtb -O dts virt.dtb
输出里能直接看到 memory@80000000 节点和它的 reg，也就是 dtb_init 解析的目标。

另外，DTB 放在内存里，所以它所在的页和普通物理页一样落在 page_init 交给分配器的范围里。内核解析完它之后，这块内存就可以被覆盖了。
*/

//lab2 function1 2411264

// page_init - 根据设备树探测到的物理内存建立 Page 描述符数组，并把空闲页交给 pmm_manager
static void page_init(void) 
{
    extern char end[];  // 链接脚本提供的内核镜像结束地址（虚拟地址）

    // entry.S 的启动页表把 VA 0xffffffffc0000000 映射到 PA 0x80000000，偏移是固定的
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_begin = get_memory_base(); //物理地址
    uint64_t mem_size = get_memory_size(); //由之前的 dtb.init 负责获取。
    uint64_t mem_end = mem_begin + mem_size;
    if (mem_size == 0 || mem_begin < DRAM_BASE || mem_end <= mem_begin) panic("page_init: invalid memory from DTB: base 0x%016lx, size 0x%016lx", mem_begin, mem_size);
    

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n", mem_size, mem_begin, mem_end - 1);

    // 可管理上界：内核只能访问已映射的物理内存，上界是 KERNTOP 对应的物理地址。
    // 注意 KERNTOP 是虚拟地址，要和物理地址 PHYSICAL_MEMORY_END（= KERNTOP - va_pa_offset）比较
    uint64_t maxpa = mem_end;
    if (maxpa > KERNTOP - va_pa_offset) 
    {
        maxpa = KERNTOP - va_pa_offset; //aka 0x88000000
    }

    // npage 是物理页号的上界，pages[0] 对应物理页号 nbase（DRAM_BASE），所以数组长度是 npage - nbase
    npage = maxpa / PGSIZE; //右移三位，pagesize = 2**12 bit. 
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE); //pages 是 page 数组的头, 在内核之后，并且要对齐。
    if (PADDR((uintptr_t)pages) >= maxpa) 
    {
        panic("page_init: kernel image is beyond managed memory");
    }

    // 先把所有页标为保留：OpenSBI、内核镜像、Page 数组自身都保持这个状态，不进入空闲区
    for (size_t i = 0; i < npage - nbase; i++) 
    {
        SetPageReserved(pages + i); 
        /*
        其余的页
        位于 free_begin 到 free_end 之间的空闲页也被标成了保留，但这只是暂时的。
        后面 init_memmap → default_init_memmap 会对它们逐页执行 assert(PageReserved(p))，
        再把 flags 清零，即清掉 PG_reserved。
        */
    }

    // ---------------------------------------------------------------
    // 最后一步：算出真正可分配的物理内存区间 [free_begin, free_end)，交给 pmm_manager。
    //
    // 先分清三种“编号”（本函数里常混在一起）：
    //   VA   虚拟地址：内核代码里的指针值，如 pages、end，都是 0xFFFFFFFFC02xxxxx 这类高地址
    //   PA   物理地址：内存条上的真实位置，如 0x80200000；DTB 给出的 mem_begin/maxpa 是它
    //   PPN  物理页号 = PA >> 12（PGSHIFT），即“第几个 4KiB 物理页”；
    //        物理地址 0x80000000 的 PPN = 0x80000，就是 nbase
    // 它们的关系：VA = PA + va_pa_offset（0xFFFFFFFF40000000）；PA = PPN << 12 + 页内偏移。
    // 本段中的变量类型：
    //   pages            VA（struct Page * 指针）          
    //   freemem / free_begin / free_end   PA
    //   npage            PPN 的上界（页号，不是页数）       nbase     PPN（起始页号）
    //   npage - nbase    实际的页数 = pages[] 数组的元素个数
    //
    // 物理内存的布局（低 -> 高）：
    //   0x80000000 OpenSBI | 内核镜像（.text/.data/.bss） | 对齐空隙 | pages[] 数组 | 空闲页 ... | maxpa
    //                                                                              ^free_begin   ^free_end
    // 前面几部分在上面的循环里已被标为 PG_reserved，不能被分配；只有 pages[] 数组之后才可用。


    // ---------------------------------------------------------------
    // pages 是 struct Page 指针，加 (npage - nbase) 按“元素个数”移动，
    // 即指向最后一个描述符之后的位置（数组的末尾，仍是 VA）；
    // PADDR 再把这个内核虚拟地址换算成物理地址 freemem：即 pages[] 数组占用内存的末尾 PA。
    uintptr_t freemem = PADDR((uintptr_t)(pages + (npage - nbase)));

    uint64_t free_begin = ROUNDUP(freemem, PGSIZE);
    // 取整后 [free_begin, free_end) 两端都落在页边界，包含的全是完整的页。
    uint64_t free_end = ROUNDDOWN(maxpa, PGSIZE);

    if (free_begin < free_end) 
    {
        // pa2page(pa)（pmm.h）：物理地址 -> 该页对应的 Page 描述符指针（VA）。
        // (free_end - free_begin) / PGSIZE：空闲区的字节数除以页大小 = 空闲页数 n。
        // init_memmap 会把 [base, base + n) 这 n 个描述符从“保留”改为“空闲”，并挂进空闲链表
        // （default_pmm 的实现见 default_init_memmap）。

        // 例：QEMU 默认 128MB，maxpa = 0x88000000，npage = 0x88000，nbase = 0x80000，
        //     pages[] 共 0x8000 项，每项 sizeof(struct Page) = 64 字节，共 2MiB，
        //     （lab3 时是 40 字节；lab4 的 struct Page 多了 pra_page_link、pra_vaddr 两个字段）
        //     free_begin 就在这块数组之后，free_end = 0x88000000。
        init_memmap(pa2page(free_begin), (free_end - free_begin) / PGSIZE);
    }
}

// enable_paging - 【lab4 新增，但当前框架里没有任何地方调用它】
// 把 boot_pgdir_pa 对应的页表装进 satp：MODE=8(Sv39) 在最高 4 位（0x8000... 即 8<<60），低 44 位是根页表的物理页号。
// 内核早在 entry.S 的 kern_entry 里就用同样的方式开启了分页（那时用的就是 boot_page_table_sv39），
// 所以 pmm_init 里不需要再开一次；这个函数是“换一张新页表时才用得上”的备用接口（切换页表后还需 sfence.vma 刷 TLB）。
static void enable_paging(void)
{
    write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
}

// boot_map_segment - setup&enable the paging mechanism
// 【lab4 新增，当前框架里同样没有调用者】把虚拟地址区间 [la, la+size) 逐页映射到物理地址 [pa, pa+size)，权限为 perm。
// 做法：对每一页调用 get_pte(pgdir, la, 1)（缺页表就现分配），然后写入 PTE = (pa>>12)<<10 | PTE_V | perm。
// 原版 ucore 用它建立 KERNBASE→物理内存 的线性映射；本框架里这个映射由 entry.S 的 1GiB 大页一次性完成，所以用不到。
// parameters
//  la:   linear address of this memory need to map (after x86 segment map)
//  size: memory size
//  pa:   physical address of this memory
//  perm: permission of this memory
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - allocate one page using pmm->alloc_pages(1)
// 【lab4 新增，当前未被调用】分配一页并返回它的内核虚拟地址（页面分配失败直接 panic）。
// 原版 ucore 用它来分配“页目录/页表”这种启动期必须成功的页。
// return value: the kernel virtual address of this allocated page
// note: this function is used to get the memory for PDT(Page Directory
// Table)&PT(Page Table)
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// pmm_init - setup a pmm to manage physical memory, build PDT&PT to setup
// paging mechanism
//         - check the correctness of pmm & paging mechanism, print PDT&PT
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other
    // size).
    // So a framework of physical memory manager (struct pmm_manager)is defined
    // in pmm.h
    // First we should init a physical memory manager(pmm) based on the
    // framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    // 【lab4 的 pmm_init 执行流】
    // 1) init_pmm_manager  选管理器、初始化空闲链表            （lab2 已有）
    // 2) page_init         探测内存、建 pages[] 数组、把空闲页交给管理器   （lab2 已有）
    // 3) check_alloc_page  测试管理器的分配/释放                  （lab2 已有）
    // 4) 取得内核页表的根：boot_pgdir_va/pa                    （lab4 新增，lab3 只打印不使用）
    // 5) check_pgdir / check_boot_pgdir  用 page_insert、get_pte 等测试页表操作（lab4 新增）
    // 6) kmalloc_init      初始化 kmalloc 的 SLOB 分配器（kern/mm/kmalloc.c，lab4 框架提供）
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a
    // pmm
    check_alloc_page();
    
    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    // boot_page_table_sv39 是 entry.S 里放在 .data 段的那张 4KiB 根页表（见上面全局变量处的说明）。
    // 它的符号地址是链接地址（虚拟地址 0xFFFFFFFFC02xxxxx），PADDR 减去 va_pa_offset 得到物理地址，
    // 这个物理页号正是 kern_entry 早先写进 satp 的值，所以 CPU 当前使用的就是这张表。
    // 这里并没有新建页表，也没有再次写 satp。
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // now the basic virtual memory map(see memalyout.h) is established.
    // check the correctness of the basic virtual memory map.
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - 找到某个虚拟地址对应的“最后一级页表项”，并返回这个页表项在内存中的地址。
//        - 如果查找途中缺少下级页表，它还可以根据参数决定是否创建。
// parameter:
//  pgdir:  根页表的内核虚拟地址，从这里开始查找
//  la:     要查询的虚拟地址
//  create: 	缺少下级页表时，是否允许分配内存创建它
// return vaule: 这个页表项在内存中的地址
/*
【lab4 新增】软件“走页表”：给定根页表 pgdir 和虚拟地址 la，返回 la 对应的第三级页表项（PTE）的指针，
可以通过它读写映射；这就是硬件 MMU 查表过程的软件版本。
Sv39 的 la 被切成 VPN[2]=PDX1(la) | VPN[1]=PDX0(la) | VPN[0]=PTX(la) | 页内偏移，三段各 9 位，每级表 512 项：
  pgdir[PDX1]        -> 第一级项 pdep1，指向第二级表
  第二级表[PDX0]     -> 第二级项 pdep0，指向第三级表
  第三级表[PTX]      -> 第三级项，即最终返回的 PTE
任何一级项无效（V=0）时：create 为 0 就返回 NULL；create 为 1 就 alloc_page 一页当下一级表，
清零（全 0 表示 512 项都无效）、引用计数置 1、并把上一级项设为指向它。
中间级的项只设 PTE_V（和 PTE_U），R/W/X 全为 0，硬件据此知道它是“指向下一级表”而不是叶子。
注意：get_pte 只返回 PTE 的位置，不会写这个 PTE，也不检查它是否有效；写 PTE 是 page_insert 的事。
一个陷阱：get_pte 假定每一级项都指向下一级表。但根页表 boot_pgdir_va[511]（内核所在的虚拟区间）是 entry.S 设置的
1GiB 大页（叶子项，R/W/X 非 0），对内核地址调用 get_pte 会把大页的物理地址误当成二级表，读出垃圾。
lab4 里 get_pte 只用于低地址（用户空间 / 测试用的 0、PGSIZE 等），不要拿它去查内核虚拟地址。
*/
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    // 查找第一级页表项（根表里的那一项）
    pde_t *pdep1 = &pgdir[PDX1(la)];
    // 如果第一级页表项无效，说明目前还没有可用的下一级页表，需要考虑创建。
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        // 不允许创建，直接返回 NULL；alloc_page() 分配物理页失败，也返回 NULL
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        // 把新分配的物理页的引用计数设置为 1，表示它已被引用
        set_page_ref(page, 1);
        // 把 struct Page * 转换成对应的物理页起始地址。
        uintptr_t pa = page2pa(page);
        // 把新分配的整个 4KB 物理页清零，因为我们打算用这个物理页存放一张新的页表
        memset(KADDR(pa), 0, PGSIZE);
        // 根据物理页号和标志位构造页表项的内容
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    // 找到第二级页表中的指定页表项
    // 第二级项：PDE_ADDR 从第一级项里取出下一级表的物理地址，KADDR 转成内核虚拟地址（CPU 的指针只能用虚拟地址），再按 PDX0 索引。
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    // 如果第二级页表项无效，说明目前还没有可用的下一级页表，需要考虑创建。
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        // 不允许创建，直接返回 NULL；alloc_page() 分配物理页失败，也返回 NULL
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        // 根据物理页号和标志位构造页表项的内容
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    // 返回最终三级页表的页表项地址
    // 第三级表按 PTX 索引，返回的就是 la 对应的 PTE 的地址（此时该 PTE 可能还是 0，即未映射）。
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
// 【lab4 新增】查询：la 当前映射到哪一个物理页，返回其 Page 描述符；未映射返回 NULL。
// 以 create=0 调用 get_pte（只查不建），PTE 有效才用 pte2page 取出 Page。
// 若 ptep_store 非空，顺便把 PTE 指针带出去（方便调用者直接改这个 PTE）。
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

//LAB4 function1 2414099
// page_insert - 在页表项中写入物理页号，建立映射，并设置这个映射的权限。
//             - 如果原来已有映射，就更新同页权限或替换旧页，同时维护引用计数并刷新 TLB。
// parameter:
//  pgdir:  根页表的内核虚拟地址，从这里开始查找
//  page:   要映射的物理页对应的 Page 结构体指针
//  la:     要建立或更新映射的虚拟地址
//  perm:   这个映射的权限标志位
// return value: 成功返回 0；创建所需页表时内存不足，返回 -E_NO_MEM
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL){return -E_NO_MEM;}

    // 先持有新页，避免同页重复插入时引用计数暂时归零而被释放。
    page_ref_inc(page);
    if (*ptep & PTE_V) // 检查当前页表项是否有效
    {
        struct Page *old_page = pte2page(*ptep);
        if (old_page == page) {page_ref_dec(page);} // 原来映射的就是同一个物理页
        else {page_remove(pgdir, la);} // 原来映射的是另一个物理页
    }

    // 写入新的页表项
    *ptep = pte_create(page2ppn(page), perm);
    // 刷新 TLB
    tlb_invalidate(pgdir, la);
    return 0;
}

// page_remove - 清除页表项，撤销映射，并刷新对应的 TLB 缓存。
//             - 减少原物理页的引用计数，归零时释放它；如果原来没有映射，则直接返回。
// parameter:
//  pgdir:  根页表的内核虚拟地址，从这里开始查找
//  la:     要撤销映射的虚拟地址
// return value: 无返回值
void page_remove(pde_t *pgdir, uintptr_t la)
{
    // 查找页表项
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL && (*ptep & PTE_V))
    {
        // 找到原来映射的物理页
        struct Page *page = pte2page(*ptep);
        // 清除映射
        *ptep = 0;
        tlb_invalidate(pgdir, la);
        // 减少物理页引用计数，必要时释放
        if (page_ref_dec(page) == 0)
        {
            free_page(page);
        }
    }
}






// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
// 【lab4 新增】修改页表项后必须让 TLB 里缓存的旧翻译失效，否则 CPU 仍按旧映射访问。
// sfence.vma 带一个地址参数时只刷新该虚拟地址的条目，比不带参数的 flush_tlb()（刷新全部）开销小。
// pgdir 参数目前没用到（只有一个地址空间；将来要区分“是否正在使用的页表”时才有意义）。
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    // flush_tlb();
    // The flush_tlb flush the entire TLB, is there any better way?
    asm volatile("sfence.vma %0" : : "r"(la));
}

// 【lab2】调用管理器自带的 check()，验证分配/释放/合并是否正确。
static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

/*
【lab4 新增】测试 get_pte / get_page / page_insert / page_remove 的正确性，并检查没有页泄漏。
流程：先查 0 地址未映射 -> 在 0 处映射 p1，检查 ref=1、三级页表已建好 -> 在 PGSIZE 处映射 p2 带 U/W，
检查中间级项被设上 PTE_U -> 把 PGSIZE 改映射 p1：p1 的 ref 变 2，p2 的 ref 降到 0 被释放，权限被改写 ->
page_remove 两处，ref 归零 -> 最后手动释放两张中间页表（二、三级表是 get_pte 分配的，page_remove 不会释放它们），
清掉 boot_pgdir_va[0]，flush_tlb，并断言空闲页数回到测试前。
这里用的是低地址 0、PGSIZE：对应根表第 0 项，本来就是空的，所以不会碰到内核的 1GiB 大页。
*/
static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir_va[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

/*
【lab4 新增】测试“内核已有的基本映射”是否正确，并再测一次 page_insert 的实际效果：
1) 本意是遍历 KERNBASE 到物理内存末尾的每一页，用 get_pte 查出 PTE，断言 PTE 里的物理地址正好等于 i。
   ——但循环变量 i 是 int，ROUNDDOWN(KERNBASE, PGSIZE) 赋给它会被截断成负数，比较时又转回无符号的巨大值，
   于是 i < npage*PGSIZE 一开始就不成立，这个循环实际一次都不会执行（按代码推断，未实测）。
   即使执行，对内核地址调用 get_pte 也会踩到 get_pte 注释里说的大页陷阱。
2) 断言 boot_pgdir_va[0]==0：低地址没有映射。
3) 把同一页 p 映射到 0x100 和 0x100+PGSIZE 两个虚拟页（ref=2），经由 0x100 写入字符串，从 0x100+PGSIZE 读出，
   证明两个虚拟地址确实指向同一物理页；再通过内核虚拟地址 page2kva(p) 把首字节清零，两个映射处读到的都是空串。
   （这里的 0x100 是低地址，访问它靠的就是刚建立的映射，说明新页表项立即生效。）
4) 释放 p 和两张中间页表，清 boot_pgdir_va[0]，刷新 TLB，检查没有页泄漏。
*/
static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
    {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - use string 'u,r,w,-' to present the permission
// 【lab4 新增，当前未被调用】把权限位转成 "u r w" 形式的字符串，是 print_pgdir 的辅助函数。
// 注意 print_pgdir 在 pmm.h 里有声明，但本文件里并没有定义（框架已把它删掉）。
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous
// 【lab4 新增，当前未被调用】print_pgdir 的辅助函数：在一级表或页表的 [start, right) 范围里，
// 跳过无效项，找出下一段“权限相同且连续有效”的项，通过 left_store/right_store 返回这一段的起止下标，返回值是这段的权限。
// linear addr space
//                  - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                  - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
// paramemters:
//  left:        no use ???
//  right:       the high side of table's range
//  start:       the low side of table's range
//  table:       the beginning addr of table
//  left_store:  the pointer of the high side of table's next range
//  right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with
//  perm permission
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
