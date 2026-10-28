## 功能模块一：Trap 上下文恢复与 Trap 入口（RESTORE_ALL、__alltraps）

[PROMPT]

**任务**：在 `code/kern/trap/trapentry.S` 中补全宏 `RESTORE_ALL` 和入口 `__alltraps` 的缺失部分，使内核形成“Trap 进入 → SAVE_ALL 保存上下文 → 调用 C 函数 `trap()` → RESTORE_ALL 恢复上下文 → `sret` 返回原执行流”的完整闭环。`RESTORE_ALL` 根据当前 Trap frame 恢复被打断程序的处理器状态；`__alltraps` 把 SAVE_ALL 建好的 Trap frame 按 RISC-V 调用约定传给 `trap(struct trapframe *tf)`。

**操作要求**：你必须在项目中直接修改实际文件，而不是只展示代码片段。只在文件中两处 `#lab3 function1 YOUR CODE` 注释的下方写代码，并把这两处注释中的 `YOUR CODE` 替换为学号 `2411264`，保留 `lab3 function1`。不要修改 SAVE_ALL、`__trapret` 标号和其后的 `sret`、`struct trapframe`、`trap()` 的签名以及其他任何文件。关键步骤写简短的中文注释。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中提供的信息，以工程中的真实代码为准。

[RELY]

```asm
# code/kern/trap/trapentry.S（框架原样，省略部分重复行）
#include <riscv.h>

    .macro SAVE_ALL

    csrw sscratch, sp

    addi sp, sp, -36 * REGBYTES
    # save x registers
    STORE x0, 0*REGBYTES(sp)
    STORE x1, 1*REGBYTES(sp)
    STORE x3, 3*REGBYTES(sp)
    STORE x4, 4*REGBYTES(sp)
    ...                          # x5 ~ x30 同样依次存到 n*REGBYTES(sp)
    STORE x31, 31*REGBYTES(sp)

    # get sr, epc, badvaddr, cause
    # Set sscratch register to 0, so that if a recursive exception
    # occurs, the exception vector knows it came from the kernel
    csrrw s0, sscratch, x0
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, sbadaddr
    csrr s4, scause

    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

    .macro RESTORE_ALL

    #lab3 function1 YOUR CODE

    .endm

    .globl __alltraps
    .align(2)
__alltraps:
    SAVE_ALL

    #lab3 function1 YOUR CODE
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
    # return from supervisor call
    sret
```

```c
// code/libs/riscv.h（RV64 部分）
#if __riscv_xlen == 64
# define STORE    sd
# define LOAD     ld
# define LOG_REGBYTES 3
#endif
#define REGBYTES (1 << LOG_REGBYTES)
```

```c
// code/kern/trap/trap.h
struct pushregs {
    uintptr_t zero;  // Hard-wired zero
    uintptr_t ra;    // Return address
    uintptr_t sp;    // Stack pointer
    uintptr_t gp;    // Global pointer
    uintptr_t tp;    // Thread pointer
    uintptr_t t0;    // Temporary
    ...              // t1, t2, s0, s1, a0 ~ a7, s2 ~ s11, t3 ~ t6，即 x6 ~ x31 依次排列
    uintptr_t t6;    // Temporary
};

struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};

void trap(struct trapframe *tf);
```

```c
// code/kern/trap/trap.c
void idt_init(void) {
    extern void __alltraps(void);
    /* Set sup0 scratch register to 0, indicating to exception vector
       that we are presently executing in the kernel */
    write_csr(sscratch, 0);
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
}
```

- RISC-V 调用约定：第一个参数通过 `a0` 传递；`sp` 是被调用者保存寄存器，`trap()` 返回时 `sp` 与调用前相同；`jal` 会改写 `ra`。
- 处理器进入 S 态 Trap 时，硬件把被打断指令的地址写入 `sepc`，把 `sstatus.SIE` 保存到 `SPIE` 后清零，并把原特权级记入 `SPP`。`sret` 跳转到 `sepc`，并按 `SPP`、`SPIE` 恢复特权级和中断使能，不读取 `stval`（即 `sbadaddr`）和 `scause`。
- `trap()` 可能修改 Trap frame 中的字段，例如异常处理会推进 `tf->epc`。

[GUARANTEE]

```asm
    .macro RESTORE_ALL     # 输入：sp 指向 Trap frame；输出：恢复后的处理器执行状态
    ...
    .endm

__alltraps:                # SAVE_ALL 之后：把 Trap frame 作为 struct trapframe * 传给 trap()
    SAVE_ALL
    ...
__trapret:                 # trap() 返回后顺序执行到这里
    RESTORE_ALL
    sret
```

[SPECIFICATION]

## RESTORE_ALL

**Pre-Condition**:
- `sp` 指向 SAVE_ALL 建立的 Trap frame（共 36 × `REGBYTES` 字节），帧中第 n 个通用寄存器槽位存 xn，32～35 号槽位依次是 status、epc、badvaddr、cause。
- `trap()` 已返回，帧内容可能已被修改（例如 `epc`）。

**Post-Condition**:
- `sstatus`、`sepc` 等于帧中的 status、epc，随后的 `sret` 依据它们返回。
- x1、x3～x31 等于帧中对应槽位的值；x0 不需要恢复。
- `sp` 等于 Trap 发生前的值（帧中 2 号槽位）。

  **Case 1**（x2/sp 的特殊处理）:
  - SAVE_ALL 没有直接保存 x2：它先把原 sp 暂存到 `sscratch`，`sp` 下移开辟 Trap frame 后，再通过 `csrrw s0, sscratch, x0` 取出原 sp 存进 2 号槽位，同时把 `sscratch` 清零。所以 2 号槽位是 Trap 前的 sp，而当前 `sp` 是帧的基址。
  - 所有数据都要通过 `sp` 从帧中取，sp 必须最后恢复；提前恢复 sp 会让后续的加载全部读错地址。

  **Case 2**（CSR 的恢复顺序）:
  - 写 CSR 需要借用通用寄存器作中转。先用中转寄存器取出 status、epc 写回 `sstatus`、`sepc`，再恢复这些中转寄存器本身，不能让它们最后保留 CSR 的值。

  **Case 3**（不需要写回的字段）:
  - badvaddr、cause 只是提供给 `trap()` 的现场记录，`sret` 不使用它们，不需要写回 CSR。

**Requirements**:
- 恢复的布局与 SAVE_ALL 保存时完全一致，使用 `LOAD` 和 `n*REGBYTES(sp)` 的写法，不自行假设其他布局。

## __alltraps

**Pre-Condition**:
- `stvec` 指向 `__alltraps`（由 `idt_init()` 设置），Trap 发生后处理器跳到这里，SAVE_ALL 执行完时 `sp` 指向 Trap frame。

**Post-Condition**:
- 调用 `trap(tf)` 时 `a0` 等于 Trap frame 的地址。
- `trap()` 返回后顺序进入 `__trapret`，此时 `sp` 仍指向同一个 Trap frame。

**Requirements**:
- 不能在调用 `trap()` 前改动 `sp`，也不能在 `trap()` 返回后插入跳转，`__trapret` 紧跟在调用之后。
- 运行 `make grade` 时，Test 1“trap entry, register restore, and breakpoint EPC”（ebreak 前后 t0～t6、s0～s11、a1～a7 全部保持不变）、Test 2（非法指令返回）、Test 3（持续的时钟中断）都依赖本模块。
