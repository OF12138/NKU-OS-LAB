# T3 功能模块三：复制用户进程
- 负责人：openfar    DDL：11-30    状态：待开始    依赖：T0
- 主要文件：code/kern/process/proc.c, code/kern/mm/vmm.c, code/kern/mm/pmm.c, report/sections/T3-*

## 要求
- 指导书「功能模块三」。需要更新和完善：`do_fork`、`copy_mm`（`kern/process/proc.c`）、`dup_mmap`（`kern/mm/vmm.c`）、`copy_range`（`kern/mm/pmm.c`）。
- 注意事项（指导书）：do_fork 要建立 trapframe、内核栈、进程关系（`set_links`）和 PID；copy_mm 区分共享与复制；dup_mmap 保持 vma 顺序和权限；copy_range 复制页内容并建立映射、维护引用计数。
- 评测：forktest 等用户程序（`I am child 31` … `forktest pass.`）。
- 交付：`report/sections/T3-fork-copy.md` 与 `T3-fork-copy.prompt.md`。

## 当前进度 / 下一步
- 待开始。

## 关键决策与结论

## 验证结果

## 迭代素材

## 留言
