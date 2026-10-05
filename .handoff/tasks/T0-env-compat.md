# T0 环境兼容
- 负责人：openfar    状态：完成    依赖：无
- 可改文件：code/Makefile, report/sections/T0-*.md

## 要求
- 修改 qemu 和 debug 两个目标，使内核在课程推荐的 QEMU 4.1.x 和较新版本下都能被 OpenSBI 跳转执行。
- 修改完成后，在「全组须知」里更新用法，并在报告中说明对框架做过的这处改动。

## 当前进度 / 下一步
- 已完成：qemu / debug 两个目标都改为 `-kernel $(kernel)`，直接加载 ELF 格式的 bin/kernel。Makefile 里加了注释说明原因。
- 新增（2026-10-05）：可选变量 `OPENSBI`，默认 `default`（QEMU 自带固件）。`make debug OPENSBI=<fw_dynamic.elf>` 换上自己编译的带符号固件，`make gdb OPENSBI=<同一路径>` 会额外 add-symbol-file。已在 QEMU 8.2.2 上验证两种方式都能启动内核。
- 报告材料：report/sections/T0-env-compat.md（供「实验环境 / 测试」部分引用）。提示词在 T0-env-compat.prompt.md。

## 关键决策与结论
- **根因**：原框架用 `-device loader` 把 ucore.img 放到 0x80200000。QEMU 4.x 自带的 OpenSBI 是 fw_jump，固定跳转到 0x80200000，所以能启动。新版 QEMU 自带的是 fw_dynamic，跳转地址由 QEMU 通过 a2 寄存器指向的 fw_dynamic_info 结构传入，而 loader 设备不向 QEMU 登记入口地址，于是 next_addr 为 0，内核不会运行。
- **方案**：改用 `-kernel bin/kernel`（ELF）。QEMU 按 ELF 的 LOAD 段把内核放到 0x80200000（readelf 显示 p_paddr = 0x80200000），并把入口地址传给 OpenSBI。
- **4.1.x 的兼容性**：没有实机验证。依据是 QEMU 4.1 源码：virt 机型在给了 -kernel 时会用 load_elf 加载，fw_jump 仍然跳到 0x80200000，与 ELF 入口一致。如果组员在 4.1.x 上发现问题，请在留言区反馈。
- bin/ucore.img 仍然会生成（qemu 目标依赖它），但运行时已不再使用。

## 验证结果
环境：WSL Ubuntu，QEMU 8.2.2，OpenSBI v1.3。
- `make qemu`：输出 `Domain0 Next Address : 0x0000000080200000`，随后打印 `(THU.CST) os is loading ...` ✅
- `make debug` + GDB 设置 `b *kern_entry` → `c`：停在 0x80200000 <kern_entry>，第一条指令是 `auipc sp,0x3` ✅

## 迭代素材
- 原 Makefile 在 QEMU 8.2 上跑 make qemu，只看到 OpenSBI 的 banner，没有内核输出。OpenSBI 显示 `Domain0 Next Address : 0x0`。
- 先确认内核镜像本身没问题：GDB 停在 0x1000 时，0x80200000 处已经是 kern_entry 的指令，说明问题出在跳转地址，而不是加载。
- 对比实验：把加载方式改成 `-kernel bin/kernel` 后，Next Address 变成 0x80200000，内核正常输出，从而定位到 fw_dynamic 需要由 QEMU 传入入口地址。

## 留言
