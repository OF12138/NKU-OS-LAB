## make qemu 运行验证

**验证人**：2413074-刘昀皓

**环境**：WSL2 Ubuntu 22.04，QEMU 7.0.0（自带 OpenSBI v1.0）。在 `code/` 目录下执行 `make clean && make qemu`。

QEMU 启动后，CPU 先执行复位代码，再跳到 OpenSBI。OpenSBI 在 M 态完成平台初始化、配置 PMP 内存保护之后，按 QEMU 传入的启动信息（`Next Address = 0x80200000`，`Next Mode = S-mode`）执行 `mret`，切换到 S 态并跳到内核入口。内核执行 `kern_init`，通过 SBI 的字符输出服务在终端打印出加载信息，随后进入死循环，所以 QEMU 会一直运行，需要按 `Ctrl+A` 再按 `X` 退出。

![make qemu 运行截图](../images/T6-qemu-run.png)

<p align="center">图 T6-1　make qemu 的运行结果（QEMU 7.0.0 / OpenSBI v1.0）</p>

截图中可以看到 OpenSBI 打印的平台信息，其中 `Domain0 Next Address` 为 `0x0000000080200000`、`Next Mode` 为 `S-mode`，最后一行是内核输出的 `(THU.CST) os is loading ...`，说明内核已经在 S 态正常运行。
