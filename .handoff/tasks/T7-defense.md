# T7 答辩准备
- 负责人：全员（openfar / lyp / nagilix）    状态：待开始    依赖：T1–T6 完成后进入复习
- 可改文件：本文件中「各人准备情况」下**自己的小节**

## 要求
- 往届每个 lab 都有答辩，助教会当面提问（往届问题记录见 `reference/lhz/Lab1/report/Lab0-Lab1答辩准备/os答辩.md`，注意那一届的 lab1 对应的是我们的 lab3 中断部分，只有一部分问题适用）。
- **每个人都要能回答下面的全部问题**，而不只是自己负责的部分。答辩时可能问到任何人，自己负责的部分要能讲到细节。
- 准备方式：先读完各 section（report/sections/），再对照下面的问题清单，自己用一两句话口述答案，答不上来的就去看对应 section 或问负责人。
- **前辈笔记有错，不能照搬**：例如他写“格式化输出在 M 态”，实际上格式化（cprintf → vprintfmt）在 S 态完成，只有逐字符输出那一步经 ecall 进入 M 态；又如他写“tail 会清理栈，假定被调函数会返回”，实际上 tail 是不保存返回地址的跳转，正是因为 kern_init 不会返回。

## 问题清单（括号内为参考出处）

**启动流程（T2）**
1. 加电后第一条指令在哪个地址？0x1000 处的几条指令做了什么？a0/a1/a2 分别是什么？
2. OpenSBI 是什么？运行在哪个特权级？它在启动时做了哪些事？
3. OpenSBI 怎样把控制权交给内核？（mret、mepc、mstatus.MPP）进入内核后特权级是什么？
4. 内核是谁、在什么时候放到 0x80200000 的？为什么 `watch *0x80200000` 不会触发？真机上又是谁加载内核？
5. M / S / U 三个特权级有什么区别？内核为什么不直接运行在 M 态？

**入口与栈（T1）**
6. `la sp, bootstacktop` 做了什么，为什么必须是第一条指令？（提示：进入内核时 sp 还指向 OpenSBI 的栈，这块区域被 PMP 禁止 S 态访问）
7. 内核栈有多大，栈底和栈顶在哪里？栈向哪个方向增长？`bootstack` 为什么放在 .data 段而不是 .bss 段？
8. `tail kern_init` 和 `call kern_init` 有什么区别？和 `noreturn` 有什么关系？
9. `kern_init` 中 `memset(edata, 0, end - edata)` 的作用是什么？edata、end 是在哪里定义的？

**链接与构建（T3）**
10. 为什么内核入口在 0x80200000？链接脚本中 `BASE_ADDRESS` 和 `ENTRY` 各起什么作用？怎样保证 kern_entry 位于最前面？
11. `make qemu` 做了哪些事？`bin/kernel` 和 `bin/ucore.img` 有什么区别？objcopy 起什么作用？
12. 我们为什么修改了 Makefile（T0）？新版 QEMU 和 4.1 的启动方式有什么不同？

**输出与 SBI（T2、T3）**
13. `cprintf` 的完整调用链是什么？格式化在哪个特权级完成，字符最终由谁输出？
14. `ecall` 之后 CPU 发生了什么？（mcause = 9、mtvec、mepc）为什么 S 态的 ecall 没有被委托给 S 态？
15. 为什么内核不能直接用 C 标准库的 printf？

**原理对照（T4）**
16. 本实验中的知识点与 OS 原理课中的哪些概念对应？有什么差异？
17. 原理课里哪些重要内容在本实验中没有体现？

**工具（全员）**
18. GDB 常用命令：`si` / `ni` / `b` / `watch` / `x` / `info registers` 各做什么？`make debug` 中 `-s -S` 的含义是什么？

## 各人准备情况
### openfar

### lyp

- 2026-10-07：已阅读 T0、T2 及本次完成的 T1/T3/T4，形成下列全部问题的书面复习答案；本机 GDB 实测入口、启动栈与 tail。T5/T6 尚无已完成 section，待其提交后补读整体逻辑与拓展。以下由 lyp 的 Codex bot 整理，不代表李云鹏本人已经口述练习或通过答辩。

1. **复位地址和参数**：当前 QEMU virt 从 M 态的 0x1000 执行六条复位指令，取得 hart ID（a0）、设备树地址（a1）、fw_dynamic_info 地址（a2），再跳到 0x80000000 的 OpenSBI。QEMU 4.1 的旧方案只有五条，不设置 a2；不要把两版混用。
2. **OpenSBI**：运行在 M 态的 RISC-V 固件，建立自己的栈和陷阱入口，探测 hart/平台、初始化控制台和服务、配置 PMP 与委托，随后交给 S 态内核；运行期间继续提供 SBI 服务。
3. **交接**：OpenSBI 设置 mepc 为内核入口、mstatus.MPP 为 S 态，再执行 mret，PC 到 kern_entry，特权级变成 S。不是普通函数调用导致的自动降权。
4. **装载者**：当前 -kernel bin/kernel 由 QEMU 在 CPU 执行前按 ELF 装载，停在 0x1000 时入口指令已经存在，因此执行期的 watch 不能观察先前的装载。真机的装载由具体启动链决定，通常是固件、引导程序或其他加载阶段，不能一概归给 OpenSBI。
5. **M/S/U**：M 是固件管理层，S 是内核，U 是用户程序；可用的特权操作和资源访问权限不同。S 态让内核通过标准 SBI 获取 M 态服务，把平台固件职责与 OS 管理分开；M 态运行内核是设计上的另一种可能，不是硬件一概禁止。
6. **la 与先建栈**：将 bootstacktop 的地址装入 sp，为需要栈的 C 代码提供有效环境。本机入口前 sp=0x80017ee0，仍在 S 态不可访问的固件区域；本入口先设置 sp 是必要顺序，但一般入口允许先做不依赖栈的寄存器操作。
7. **栈区域**：KSTACKSIZE=2×4096=8192 字节，本次范围 [0x80201000,0x80203000)，高端标签是 bootstacktop，sp 向低地址增长。放在 .data 使已使用的栈不被 kern_init 的 BSS 清零覆盖。
8. **tail/call/noreturn**：call 建立 ra 返回地址，tail 不改写 ra，也不自动清理栈；本次 entry.o 为 auipc/jalr，最终 ELF 松弛为 c.j。noreturn 约束编译器，实际永不返回由 while(1) 保证。
9. **清零**：kernel.ld 在 .data/.sdata 后提供 edata，在 .bss 后提供 end，memset 清零 [edata,end)，满足 BSS 零初始化要求。本次二者都是 0x80203008，长度为零。
10. **入口位置**：BASE_ADDRESS 与位置计数器让 .text 从 0x80200000 开始，当前链接输入中 entry.o 在最前且 kern_entry 是其开头，因此入口在基址。ENTRY 只设置 ELF 入口字段；当前 entry.S 用普通 .text，不能声称单独入口段已经保证排序。
11. **构建与产物**：make qemu 先编译 .c/.S、链接 ELF、objcopy 出裸镜像，随后当前 QEMU 实际加载 ELF。bin/kernel 有头、入口、装载信息及调试符号；bin/ucore.img 是内容字节，不带这些元数据。
12. **T0 修复**：旧 loader 只复制镜像，配套 fw_jump 固定进入 0x80200000；fw_dynamic 需要 QEMU 提供下一阶段信息，否则原方案入口为零。改为 -kernel bin/kernel 让 QEMU 根据 ELF 登记入口，本机 6.2.0 已验证正常，T0 验证了 8.2.2。
13. **输出链**：cprintf→vcprintf→vprintfmt→cputch→cons_putc→sbi_console_putchar→sbi_call→ecall→OpenSBI 控制台服务→UART。格式化在 S 态，设备输出由固件完成；优化后部分中间函数已内联，没有独立符号。
14. **ecall 陷入**：当前 S 态 ecall 没被委托，硬件记录 mcause=9 与 mepc，进入 M 态陷阱入口；固件保存现场、处理请求并将返回 PC 前移到下一条指令，再 mret 回 S 态。若把该调用委托回 S 态，就无法按此链请求 M 态固件；这不同于把某些其他异常委托给内核。
15. **printf**：内核是 freestanding 环境，没有宿主 libc、用户态文件接口及系统调用支持，构建使用 nostdlib/nostdinc。所需格式化和输出由项目自己的 cprintf 与 SBI 完成。
16. **原理映射**：引导、特权级与陷阱、栈与调用约定、段布局与初始化、链接与装载、设备抽象均有具体对应。需区分 SBI/用户系统调用、启动栈/进程切换、段组织/页表权限；详见 T4。
17. **未覆盖**：物理页分配、分页与缺页处理、进程调度、用户系统调用、内核中断处理、同步互斥、文件系统、完整设备管理、IPC/网络。固件已经处理 ecall，因此不说“没有任何异常”。
18. **GDB**：si 单步机器指令并进入调用；ni 跨过调用；b 设置执行断点；watch 监视表达式值变化；x 检查指定地址的内存或指令；info registers 查看寄存器。QEMU -s 开默认 1234 调试端口，-S 让 CPU 启动后暂停，两者职责不同。

- 本人下一步：先用以上要点逐题口述，再核对源码和 section；T5/T6 完成后补读它们，练习不同 QEMU/固件版本下避免混用地址。该共享任务的总状态由全组后续统一确认，本次不替其他成员标记完成。

### nagilix

## 留言
