#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array

// pages指针保存的是第一个Page结构体所在的位置，也可以认为是Page结构体组成的数组的开头
//数组起点应位于end之后并按页对齐
struct Page *pages;

// amount of physical memory (in pages)
size_t npage = 0; //npage表示内核可管理的物理页号上界

// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;

// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE; //nbase表示 DRAM 起始位置对应的物理页号，因此描述符数组需要覆盖[nbase, npage)中的每个物理页

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) 
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

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
    if (mem_size == 0 || mem_begin < DRAM_BASE || mem_end <= mem_begin) 
    {
        panic("page_init: invalid memory from DTB: base 0x%016lx, size 0x%016lx", mem_begin, mem_size);
    }

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n", mem_size, mem_begin, mem_end - 1);

    // 可管理上界：内核只能访问已映射的物理内存，上界是 KERNTOP 对应的物理地址。
    // 注意 KERNTOP 是虚拟地址，要和物理地址 PHYSICAL_MEMORY_END（= KERNTOP - va_pa_offset）比较
    uint64_t maxpa = mem_end;
    if (maxpa > PHYSICAL_MEMORY_END) 
    {
        maxpa = PHYSICAL_MEMORY_END; //aka 0x88000000
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
        //     pages[] 共 0x8000 项，每项 sizeof(struct Page) = 40 字节，约 1.25MiB，
        //     free_begin 就在这块数组之后，约为 0x802xxxxx ~ 0x803xxxxx，free_end = 0x88000000。
        init_memmap(pa2page(free_begin), (free_end - free_begin) / PGSIZE);
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) 
{
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
