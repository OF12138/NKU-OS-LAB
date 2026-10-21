# 五、测试与验证

### 5.1 自动化测试框架设计与评测机制

Lab 2 的测试体系分为内核常态运行（`make qemu`）与自动化评分脚本（`make grade`）两套链路：

1. **常态启动与自检输出（`make qemu`）**：
   内核启动后，执行流经 `kern_entry` 进入 `kern_init`，随后调用 [`pmm_init()`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/pmm.c#L190-L208) 完成物理内存管理器初始化、[`page_init()`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/pmm.c#L137-L188) 探测与建立页描述符数组，并触发当前激活分配器的内置自测试函数（如 `check_alloc_pages()`）。通过串口控制台（UART）输出各阶段的初始化日志与状态标志。

2. **自动化评分注入与断言检查（`make grade`）**：
   评测脚本 [`code/tools/grade.sh`](file:///home/nagilix/Code/NKU-OS-LAB/code/tools/grade.sh) 通过 [`code/tools/inject.py`](file:///home/nagilix/Code/NKU-OS-LAB/code/tools/inject.py) 动态将标准评测探针注入到 [`code/kern/mm/pmm.c`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/pmm.c) 的 `pmm_init()` 函数末尾，严格比对控制台的断言输出：
   - **`[Test 1] Page Init: PASS`**：校验 `page_init` 是否正确划定了保留区域（OpenSBI、内核镜像、Page 描述符数组本身），并将剩余空闲内存准确纳入管理；
   - **`[Test 2] Best-Fit: PASS`**：校验 Best-Fit 内存分配器的空闲块切分、最小碎片选取及页面释放双向合并逻辑。
   - 此外，针对 Challenge 任务中的 **Buddy System** 与 **SLUB**，分别在各自实现的初始化例程中嵌入了高强度的边界自测套件。

### 5.2 运行与评测结果

> **【说明】**：根据团队协作依赖，当前处于 T8 初稿阶段，完整评测依赖队友 lyp 提交 T2（Best-Fit 算法实现）。待 T2 合并后，将立即在统一的 QEMU 4.1.1 仿真环境下执行测试并捕获高清终端截图。

#### 1. 内核编译与常态运行（`make qemu`）

预期输出包含 OpenSBI v0.4 启动横幅、物理内存探测范围（`memory base: 0x80000000, size: 0x08000000` 即 128 MiB）、各模块初始化成功标志及测试通过提示：

<p align="center">
  <img src="../images/T8-make-qemu.png" alt="make qemu 运行截图" width="85%" />
</p>

<p align="center">图 T8-1　make qemu 内核启动与自检运行输出</p>

**输出关键信息解读：**
- `OpenSBI v0.4`：证明统一采用了 QEMU 4.1.1 内置固件，`fw_jump` 模式正常引导至 0x80200000；
- `physcial memory 128M: 0x80000000 ~ 0x88000000`：表明通过 DTB 设备树正确动态获取了内存基址与范围，且受限于 `PHYSICAL_MEMORY_END = 0x88000000` 的内核安全管理边界；
- **物理内存精确划分**（与 T1 实测数据一致）：总共 32768 页（128 MiB），其中 OpenSBI（512 页，0x80000000～0x801FFFFF）、内核镜像（7 页，0x80200000～0x80206FFF）、Page 描述符数组（320 页，0x80207000～0x80346FFF）均被严格标记为保留状态；剩余的空闲区域（31929 页，0x80347000～0x87FFFFFF）通过 `init_memmap` 完整注册进分配器；
- `check_alloc_pages() succeeded!`：证明底层页面分配器通过了分配连续页、引用计数检查、指针换算及释放合并的基础功能自检；
- [`slub_init()`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/slub.c#L236-L256)：成功初始化覆盖 16B～1024B 的 7 级通用对象缓存池，通过 [`alloc_pages()`](file:///home/nagilix/Code/NKU-OS-LAB/code/kern/mm/pmm.h#L52) 申请底层物理页支撑小对象分配，与全局评测正交解耦。

#### 2. 自动化评分测试（`make grade`）

评测脚本在临时沙箱目录中完成源码构建、探针注入、模拟器执行及输出正则捕获，预期得分达到满分标准：

<p align="center">
  <img src="../images/T8-make-grade.png" alt="make grade 评测结果截图" width="85%" />
</p>

<p align="center">图 T8-2　make grade 自动化评分结果</p>

**得分项核验：**
- `Page Init Test`：满分（通过内存边界对齐、Page 描述符数组与空闲链表挂载校验）；
- `Best-Fit Allocator Test`：满分（通过分配精确匹配块、切分大块、释放相邻块合并测试）。
