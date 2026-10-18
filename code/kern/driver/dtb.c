#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <defs.h>
#include <memlayout.h>


/*
2. 什么是 FDT

FDT 是 Flattened Device Tree（扁平设备树）。
设备树是一棵描述硬件的树，节点是 CPU、内存、串口等设备，节点的属性记录地址、中断号这类信息。
源文件是文本（.dts），编译成紧凑的二进制（.dtb）后就是 FDT，
它把树"拍平"成一段连续字节。QEMU 和 OpenSBI 生成它，内核读它，就不必把硬件信息写死在代码里。
*/


// 设备树头部结构
struct fdt_header 
{
    uint32_t magic;          // 0xd00dfeed (big-endian)
    uint32_t totalsize;      // 整个DTB的大小
    uint32_t off_dt_struct;  // 结构体部分的偏移
    uint32_t off_dt_strings; // 字符串部分的偏移
    uint32_t off_mem_rsvmap; // 内存保留映射的偏移
    uint32_t version;        // DTB版本
    uint32_t last_comp_version; // 最后兼容版本
    uint32_t boot_cpuid_phys;   // 启动CPU的物理ID
    uint32_t size_dt_strings;   // 字符串部分大小
    uint32_t size_dt_struct;    // 结构体部分大小
};

// FDT token定义
#define FDT_BEGIN_NODE  0x00000001
#define FDT_END_NODE    0x00000002
#define FDT_PROP        0x00000003
#define FDT_NOP         0x00000004
#define FDT_END         0x00000009

// 字节序转换函数
static uint32_t fdt32_to_cpu(uint32_t x) {
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
}

static uint64_t fdt64_to_cpu(uint64_t x) {
    return ((uint64_t)fdt32_to_cpu(x & 0xffffffff) << 32) | 
           fdt32_to_cpu(x >> 32);
}

/* *
 * extract_memory_info - 在设备树里找到 memory 节点的 reg 属性，取出内存起始地址和大小
 *
 * 参数：dtb_vaddr  DTB 在内核中的虚拟地址（各偏移量都是相对它的）
 *       header     指向 DTB 头部（已校验过魔数）
 *       mem_base / mem_size  输出：物理内存起始地址 / 字节数
 * 返回：0 找到；-1 没找到或设备树格式出错
 *
 * 设备树的二进制（FDT）布局：
 *     [ 头部 fdt_header ][ 内存保留表 ][ 结构块 ][ 字符串块 ]
 *   - 结构块：一串以 4 字节为单位的 token，描述节点树。每个 token 后面跟的内容：
 *       FDT_BEGIN_NODE(1)  后面是以 '\0' 结尾的节点名，补零到 4 字节对齐
 *       FDT_PROP(3)        后面依次是 长度 len(4B)、属性名在字符串块中的偏移 nameoff(4B)、
 *                          len 字节的属性值，补零到 4 字节对齐
 *       FDT_END_NODE(2)    当前节点结束，没有后续内容
 *       FDT_NOP(4)         空操作，跳过
 *       FDT_END(9)         整个结构块结束
 *   - 字符串块：属性名只存一份，属性里用偏移引用，省空间。
 *   所有整数都是大端，读出来要用 fdt32_to_cpu / fdt64_to_cpu 转换。
 *
 * 本函数做的事：从结构块开头顺序扫描；遇到名字以 "memory" 开头的节点就置位
 * in_memory_node；在该节点内遇到名为 "reg" 的属性，就把值解释成两个 8 字节整数
 * <base size>（假设根节点的 #address-cells 和 #size-cells 都是 2，QEMU virt 如此）。
 * 这是一个简化实现：不检查 cells 数；"memory" 前缀匹配也会命中 memory-xxx 之类的节点。
 * */
static int extract_memory_info(uintptr_t dtb_vaddr, const struct fdt_header *header, 
                              uint64_t *mem_base, uint64_t *mem_size) {
    // 头部里存的是结构块、字符串块相对 DTB 起点的偏移（大端），先转成本机字节序
    uint32_t struct_offset = fdt32_to_cpu(header->off_dt_struct);
    uint32_t strings_offset = fdt32_to_cpu(header->off_dt_strings);
    
    // 字符串块起点：属性名通过它加偏移来查
    const char *strings_base = (const char *)(dtb_vaddr + strings_offset);
    // 结构块起点：struct_ptr 是扫描指针，按 uint32_t（4 字节）为单位前进
    const uint32_t *struct_ptr = (const uint32_t *)(dtb_vaddr + struct_offset);
    
    // 当前是否位于 memory 节点内（遇到 BEGIN_NODE 匹配名字置 1，END_NODE 清 0）
    int in_memory_node = 0;
    
    while (1) {
        // 每轮先读一个 token，*struct_ptr++ 同时让指针后移 4 字节，指向 token 的后续内容
        uint32_t token = fdt32_to_cpu(*struct_ptr++);
        
        switch (token) {
            case FDT_BEGIN_NODE: {
                // token 后面紧跟节点名（C 字符串）；根节点的名字是空串
                const char *name = (const char *)struct_ptr;
                int name_len = strlen(name);
                
                // 检查是否是memory节点
                if (strncmp(name, "memory", 6) == 0) {
                    in_memory_node = 1;
                }
                
                // 跳过节点名（4字节对齐）：名字占 name_len+1 字节（含结尾 '\0'），
                // 向上取整到 4 的倍数 = (起点 + name_len + 1 + 3) & ~3，即下面的 +4
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + name_len + 4) & ~3);
                break;
            }
            
            case FDT_END_NODE:
                // 节点结束，离开 memory 节点（没有后续内容，指针不用动）
                in_memory_node = 0;
                break;
                
            case FDT_PROP: {
                // FDT_PROP 后依次是：属性值长度、属性名在字符串块中的偏移，然后才是属性值
                uint32_t prop_len = fdt32_to_cpu(*struct_ptr++);
                uint32_t prop_nameoff = fdt32_to_cpu(*struct_ptr++);
                const char *prop_name = strings_base + prop_nameoff; // 属性名
                const void *prop_data = struct_ptr;                  // 属性值的起始位置
                
                // 在memory节点中查找reg属性
                // reg = <起始地址 长度>，各 8 字节，所以至少要有 16 字节
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
                    const uint64_t *reg_data = (const uint64_t *)prop_data;
                    *mem_base = fdt64_to_cpu(reg_data[0]);
                    *mem_size = fdt64_to_cpu(reg_data[1]);
                    return 0; // 成功找到
                }
                
                // 不是想要的属性：跳过属性值（prop_len 字节，向上取整到 4 的倍数）
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + prop_len + 3) & ~3);
                break;
            }
            
            case FDT_NOP:
                break; // 空操作，直接读下一个 token
                
            case FDT_END:
                return -1; // 扫到结构块末尾都没找到 memory 的 reg
                
            default:
                return -1; // 遇到未知 token，说明设备树损坏或格式不认识，放弃
        }
    }
}

// 保存解析出的系统物理内存信息
static uint64_t memory_base = 0;
static uint64_t memory_size = 0;

/* *
 * dtb_init - 解析 OpenSBI 交给内核的设备树（DTB），得到物理内存的起始地址和大小
 *
 * 背景：内核不能写死 “内存从哪里开始、有多大”，这些由硬件/模拟器决定，
 *       固件（OpenSBI）会把描述硬件的设备树通过寄存器传给内核：
 *           a0 = hartid（当前 CPU 核编号）   a1 = 设备树的【物理】地址
 *       kern_entry（entry.S）开头已经把 a0、a1 存进全局变量 boot_hartid、boot_dtb，
 *       因为此后 a0、a1 会被覆盖。
 *
 * 调用时机：kern_init 开头，此时分页已开启（见 entry.S 的启动页表）。
 * 结果：解析出的 memory_base / memory_size 保存在本文件的静态变量里，
 *       之后由 pmm_init -> page_init 通过 get_memory_base()/get_memory_size() 取用。
 *
 * 流程：1) 打印 hartid 和 DTB 地址  
 *      2) 检查 DTB 地址非空
 *       3) 物理地址换算成内核虚拟地址 
 *       4) 校验魔数 
 *       5) 遍历设备树取出 memory 节点的 reg
 * 任何一步失败都只打印信息后返回，memory_base/size 保持 0（调用者需自行处理）。
 * */
void dtb_init(void) 
{
    cprintf("DTB Init\n");
    cprintf("HartID: %ld\n", boot_hartid); // 这两个值之前由OPENSBI放在 a0, a1，后来经过 entry.S 放在 .data
    cprintf("DTB Address: 0x%lx\n", boot_dtb);
    
    // OpenSBI 若没有传入设备树，a1 为 0，后面无法解析，直接返回
    if (boot_dtb == 0) 
    {
        cprintf("Error: DTB address is null\n");
        return;
    }
    
    // 转换为虚拟地址：boot_dtb 是物理地址，而开启分页后 CPU 只能通过虚拟地址访问内存。
    // 启动页表把 VA 0xFFFFFFFFC0000000 起的 1GiB 映射到 PA 0x80000000 起，
    // 所以 VA = PA + PHYSICAL_MEMORY_OFFSET（0xFFFFFFFF40000000）。
    // 前提是 DTB 位于这 1GiB 内（QEMU virt 把它放在内存末尾附近，满足），否则会缺页。
    uintptr_t dtb_vaddr = boot_dtb + PHYSICAL_MEMORY_OFFSET;
    const struct fdt_header *header = (const struct fdt_header *)dtb_vaddr; //右边： (const struct fdt_header *)dtb_vaddr 是强制类型转换。dtb_vaddr 是 uintptr_t（一个整数，表示 DTB 的虚拟地址），转成"指向 fdt_header 的指针"。
    //这里没有拷贝任何数据，也不创建新结构体，只是告诉编译器"从这个地址起，按 fdt_header 的布局解释内存"。之后 header->magic 就等价于读该地址偏移 0 处的 4 字节

    // 验证DTB：头部第一个字段是魔数 0xd00dfeed。
    // DTB 内的所有多字节整数都是大端序，而 RISC-V 这里按小端运行，
    // 所以读出来必须用 fdt32_to_cpu 做字节序转换（本文件上方的辅助函数）。
    uint32_t magic = fdt32_to_cpu(header->magic);
    if (magic != 0xd00dfeed) 
    {
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
        return;
    }
    
    // 提取内存信息：extract_memory_info 顺着设备树的“结构块”逐个 token 扫描，
    // 找到名字以 "memory" 开头的节点（QEMU 里是 memory@80000000），
    // 读它的 reg 属性，格式为 <起始地址(8字节) 长度(8字节)>，均为大端。
    // 返回 0 表示找到，-1 表示没找到或设备树格式出错。
    // QEMU virt 默认 128MB 内存，即 Base=0x80000000、Size=0x8000000。
    uint64_t mem_base, mem_size;
    if (extract_memory_info(dtb_vaddr, header, &mem_base, &mem_size) == 0) 
    {
        cprintf("Physical Memory from DTB:\n");
        cprintf("  Base: 0x%016lx\n", mem_base);
        cprintf("  Size: 0x%016lx (%ld MB)\n", mem_size, mem_size / (1024 * 1024));
        // End 是最后一个可用字节的地址（闭区间），所以要减 1
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
        // 保存到全局变量，供 PMM 查询
        memory_base = mem_base;
        memory_size = mem_size;
    } 
    else 
    {
        // 没解析到 memory 节点：memory_base/size 保持初始值 0
        cprintf("Warning: Could not extract memory info from DTB\n");
    }
    cprintf("DTB init completed\n");
}

uint64_t get_memory_base(void) {
    return memory_base;
}

uint64_t get_memory_size(void) {
    return memory_size;
}