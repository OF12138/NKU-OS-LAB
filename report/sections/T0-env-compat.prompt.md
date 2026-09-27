## 环境兼容：修改 QEMU 启动方式

[PROMPT]

**任务**：修改 `code/Makefile` 中的 `qemu` 和 `debug` 两个目标，使 lab1 内核在较新版本的 QEMU（自带 fw_dynamic 型 OpenSBI，例如 QEMU 8.2 / OpenSBI 1.3）上能够被 OpenSBI 跳转执行，同时保持对课程推荐的 QEMU 4.1（fw_jump 型 OpenSBI）的兼容。

**操作要求**：你必须在项目中直接修改实际文件，而不是只展示代码片段。只修改这两个目标的 QEMU 启动参数，不要改动编译、链接规则和其他目标。在修改处用中文注释说明原因。

**输出要求**：直接进行文件操作，使用下面 [RELY]、[GUARANTEE]、[SPECIFICATION] 中提供的信息，以工程中的真实代码为准。

[RELY]

```makefile
# code/Makefile（修改前）
kernel = $(call totarget,kernel)
UCOREIMG	:= $(call totarget,ucore.img)

qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000

debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000\
		-s -S
```

```ld
/* code/tools/kernel.ld */
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
```

- 现象：在 QEMU 8.2.2 上运行 `make qemu`，OpenSBI 输出 `Domain0 Next Address : 0x0000000000000000`，之后没有任何内核输出。
- `bin/kernel` 是链接得到的 ELF 文件，其 LOAD 段的物理地址从 0x80200000 开始；`bin/ucore.img` 是 objcopy 生成的纯二进制文件，不带地址和入口信息。

[GUARANTEE]

必须修改的目标：

```makefile
qemu:   # 启动 QEMU 运行内核
debug:  # 以 -s -S 启动 QEMU，等待 GDB 连接
```

不得新增或删除其他目标，不得修改 `make gdb` 的行为。

[SPECIFICATION]

## qemu

**Pre-Condition**:
- `bin/kernel` 和 `bin/ucore.img` 已经由默认目标构建完成。

**Post-Condition**:
- QEMU 以 virt 机型、默认 OpenSBI 固件启动，内核位于物理地址 0x80200000，OpenSBI 完成初始化后跳转到 `kern_entry`，终端输出 `(THU.CST) os is loading ...`。

  **Case 1**:
  - 使用 fw_dynamic 型 OpenSBI（新版 QEMU）时，QEMU 必须知道内核的入口地址，并通过 fw_dynamic_info 传给 OpenSBI，OpenSBI 报告的 Next Address 应为 0x80200000。

  **Case 2**:
  - 使用 fw_jump 型 OpenSBI（QEMU 4.1）时，OpenSBI 固定跳转到 0x80200000，内核必须恰好被加载在这个地址上。

## debug

**Pre-Condition**:
- 同 qemu。

**Post-Condition**:
- 与 qemu 使用相同的内核加载方式，另外加上 `-s -S`：CPU 在复位地址 0x1000 暂停，并在 1234 端口等待 GDB 连接。通过 `make gdb` 连接后执行 `b *kern_entry` 和 `c`，应停在 0x80200000。

**Requirements**:
- 同一份 Makefile 必须在两类 QEMU 上都能工作，不能依赖某个特定的 QEMU 版本号。
