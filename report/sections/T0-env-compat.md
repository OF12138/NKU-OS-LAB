## 对实验框架的修改：QEMU 启动参数

**负责人：** openfar

框架 Makefile 中的 `qemu` 和 `debug` 目标原本用 `-device loader,file=bin/ucore.img,addr=0x80200000` 把内核镜像放进内存。在较新的 QEMU（我们使用的是 8.2.2，自带 OpenSBI v1.3）上，运行 `make qemu` 只能看到 OpenSBI 的启动信息，内核不会执行，并且 OpenSBI 报告 `Domain0 Next Address : 0x0`。

**原因**：课程推荐的 QEMU 4.1 自带 fw_jump 型 OpenSBI，它会无条件跳转到编译时写死的 0x80200000。新版 QEMU 默认使用 fw_dynamic 型固件，下一阶段的入口地址由 QEMU 在启动时通过 `fw_dynamic_info` 结构传递给 OpenSBI（结构的地址放在 a2 寄存器中，由 0x1000 处的复位代码设置）。`-device loader` 只负责把文件复制进内存，不会向 QEMU 登记入口地址，所以 OpenSBI 得到的 next_addr 是 0。

**修改**：两个目标都改为 `-kernel bin/kernel`，直接加载 ELF 格式的内核。QEMU 会根据 ELF 程序头把内核放到 0x80200000（链接脚本中 `BASE_ADDRESS` 指定的地址），并把 ELF 入口 `kern_entry` 作为 next_addr 传给 OpenSBI。修改后 `make qemu` 能正常输出 `(THU.CST) os is loading ...`，`make debug` 配合 `make gdb` 也可以在 `kern_entry` 处正常断下。
