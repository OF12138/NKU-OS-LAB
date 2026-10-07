# 三、内核引导运行验证 (make qemu)

### 3.1 运行环境声明
- **验证人**：刘昀皓 (nagilix)
- **宿主环境**：Ubuntu 22.04 LTS (WSL2 / Linux Kernel 5.15+)
- **QEMU 版本**：`QEMU emulator version 7.0.0`
- **构建测试命令**：在 `code/` 目录下执行 `make clean && make qemu`

### 3.2 引导流程与验证结论
在 QEMU 7.0.0 模拟环境下，系统复位后首先执行复位代码并加载 OpenSBI v1.0 固件（运行于 M 态）。OpenSBI 完成 Hart 域探测与 PMP 物理内存保护配置后，根据动态参数结构体中的配置（`next_addr = 0x80200000`，`next_mode = S-mode`），执行 `mret` 指令降级跳转。
uCore 内核在 `0x80200000` 处顺利接管控制权，执行 `kern_init` 并通过 SBI 字符输出接口在控制台打印引导标语。由于当前内核初始化结束后进入空转等待，终端由 QEMU 独占接管。

### 3.3 运行结果截图

![make qemu 运行截图](../images/T6-qemu-run.png)

运行截图完整展示了 OpenSBI v1.0 的 Banner、平台硬件拓扑参数以及底部的 `(THU.CST) os is loading ...`，证明 S 模式内核引导链路全线畅通。