# T5 软件 TLB 调试复现

## 版本与证据

- 内核：`c0844a70e6002b5ffe6dab133ef6ac37f13a2dd3` 的 `code/`，默认 Best-Fit。
- WSL：Ubuntu-22.04-OS，用户 lenovo；交叉工具链位于 `/opt/riscv/bin`，GCC 10.2.0。
- QEMU：`~/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`，带 debug_info；宿主机为 x86-64。
- 内核地址：`kern_init=0xffffffffc02000d8`，保存 ra 的 `sd=0xffffffffc02000ee`，栈写地址 `0xffffffffc0207ff8`，根页表 PA `0x80208000`。
- 真实交互窗格：`T5-miss.ans`、`T5-fast.ans`、`T5-victim.ans`、`T5-modes.ans`。它们由 `tmux capture-pane -p -e -S -` 直接生成。
- 基线检查：`T5-make.log`、`T5-qemu.log`、`T5-grade.log`，默认评分 20/20。

本次使用的 QEMU 源码无需修改。GDB 命令中的行号对应 QEMU 4.1.1。换工具链或内核版本后，先重新确定访存地址；宿主机指针和生成代码地址每轮都可能变化。

## 编译

在 WSL 的登录 shell 中执行，确保 `/opt/riscv/bin` 已加入 PATH：

```bash
REPO=/mnt/f/Lab_2026/NKU-OS-LAB
TASK_BUILD=$HOME/Code/NKU-OS-LAB/lyp-t5-c0844a7
mkdir -p "$TASK_BUILD"
git -C "$REPO" archive c0844a7 code | tar -x -C "$TASK_BUILD"
cd "$TASK_BUILD/code"
make -j4
riscv64-unknown-elf-nm bin/kernel | grep -E ' (kern_init|boot_page_table_sv39|boot_hartid|bootstacktop)$'
riscv64-unknown-elf-objdump -d bin/kernel | head -100
```

`make grade` 会清理当前目录的编译产物，所以评分后需要重新 `make`，再运行调试。

## 在真实 tmux 窗口中执行

PowerShell 中依次运行（每次启动一个新 QEMU，使用本机端口 12345）：

```powershell
wsl -d Ubuntu-22.04-OS -- bash -lc 'python3 /mnt/f/Lab_2026/NKU-OS-LAB/temp/T5-tmux.py miss'
wsl -d Ubuntu-22.04-OS -- bash -lc 'python3 /mnt/f/Lab_2026/NKU-OS-LAB/temp/T5-tmux.py fast'
wsl -d Ubuntu-22.04-OS -- bash -lc 'python3 /mnt/f/Lab_2026/NKU-OS-LAB/temp/T5-tmux.py victim'
wsl -d Ubuntu-22.04-OS -- bash -lc 'python3 /mnt/f/Lab_2026/NKU-OS-LAB/temp/T5-tmux.py modes'
```

脚本创建 `lyp-t5-<实验名>` 会话，其中 `qemu`、`host`、`guest` 是三个窗口。`host` 用宿主机 GDB 附加到 QEMU，`guest` 用交叉 GDB 连接 gdbstub；调试命令从同名 `.gdb` 文件读取，以 `tmux send-keys -l` 逐行输入。结束时保存完整宿主机 GDB 窗格，然后 detach 并关闭本次 QEMU，释放端口；保留 GDB 窗口供检查。

本机 `ptrace_scope=1`，所以附加使用 `sudo -n gdb`，需要已有的 sudo 权限。脚本不更改系统 ptrace 设置。如果已有同名 tmux 会话，脚本会停止，避免覆盖已有窗口；检查完该实验后可用 `tmux kill-session -t lyp-t5-miss` 等命令清理对应会话，再重跑。

手动复现时也可以打开同样的三个窗口，按 `.gdb` 文件逐条执行。`echo HOST_READY` 是批处理同步标记，交互调试不需要；`set trace-commands` 和末尾的汇总 `printf` 也不影响实验。真正的结果来自断点、单步和 `p`/`x` 命令。

| 实验 | 核心检查 |
| :--- | :--- |
| miss | 主表第 7 项无效 → 填入栈页 → `tlb_hit=true` → 宿主机内存写入 `0x80000a02` |
| fast | 第二次 Bare 写的标签相等 → ZF=1 → `jne` 不跳转 → 写入 `0x82200000` |
| victim | 返回 true；主表第 0 项与 victim 第 0 项交换设备树页、代码页 |
| modes | 同为 S 态数据写；Bare 直接返回同值 PA，Sv39 遍历页表；两次均填软件 TLB |

## 从真实窗格生成报告图

在 Windows 仓库根目录运行，Python 需要 Pillow，字体为 Windows 的 Cascadia Mono。以下命令只裁取原始窗格中的连续区域，并沿用 openfar 的浅色渲染工具：

```powershell
python .claude/tools/tmux-render.py temp/T5-miss.ans report/images/T5-tlb-miss.png '(gdb) p/x addr' '(gdb) bt 4'
python .claude/tools/tmux-render.py temp/T5-miss.ans report/images/T5-tlb-fill.png '(gdb) p/x env_tlb($env)->f[1].table[$idx].addr_write' '(gdb) break accel/tcg/cputlb.c:1607'
python .claude/tools/tmux-render.py temp/T5-fast.ans report/images/T5-tlb-fast.png '(gdb) p/x $env->satp' '(gdb) python'
python .claude/tools/tmux-render.py temp/T5-victim.ans report/images/T5-tlb-victim.png '(gdb) p $hit'
python .claude/tools/tmux-render.py temp/T5-modes.ans report/images/T5-tlb-modes.png '(gdb) p/x $bare_satp'
```

报告中的 `addend`、宿主机指针和生成代码偏移与本次 `.ans` 一致。若重新运行并更新截图，应同步检查正文中的这些值。

## 集成注意

`report/sections/T5-tlb.md` 接在 T5-walk 后面，图片编号从 T5-4 延续到 T5-8；`T5-tlb.prompt.md` 为独立四段式提示词。现有 `merge-report.py --fill` 已识别 `T5-tlb*.md` 占位符，可由集成负责人合并。

两部分编译器产生的入口地址相差 2 字节，报告已经说明各自的测量范围。另需核对旧章节中关于 Bare 切换的规范表述：QEMU 4.1.1 的旧 TLB 行为是实测事实，但当前 RISC-V 规范要求 Bare 与其他模式之间切换立即生效，不能用“未 fence 可沿用旧翻译”概括这一切换。
