# 统一实验环境：QEMU 4.1.1

## 为什么统一到 4.1.1

* 框架的 Makefile 和 `tools/grade.sh` 都用 `-device loader,file=bin/ucore.img,addr=0x80200000` 加载内核。这依赖旧版 OpenSBI（fw_jump）固定跳转到 0x80200000。在 QEMU 8.2（OpenSBI v1.3，fw_dynamic）上，这种加载方式会让 OpenSBI 的 Next Address 为 0，内核根本不运行（lab1 实测，见 lab1 报告练习 2）；其他版本的行为也各不相同，`make grade` 的结果因此不可比。
* 指导书的预期输出（OpenSBI v0.4）和 lab2 分支任务（GDB 调试 QEMU 源码）都基于 qemu-4.1.1。
* 带调试信息的 4.1.1 既能日常运行，也能直接用于分支任务，只需编译一次。

## 安装步骤（WSL / Ubuntu）

```bash
# 1. 在仓库根目录执行。缺依赖时脚本会打印需要的 sudo apt install 命令，装完重新执行即可
bash .claude/tools/setup-qemu-4.1.1.sh
#    源码和编译产物在 ~/Environment/qemu-4.1.1（可以传参数换目录），耗时几分钟
#    不执行 make install，系统里原有的 qemu 不受影响

# 2. 让 Makefile 使用它（Makefile 中是 ifndef QEMU，grade.sh 也通过 make print-qemu 取到它）
echo 'export QEMU=$HOME/Environment/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64' >> ~/.bashrc
source ~/.bashrc
```

## 自检

```bash
$QEMU --version            # QEMU emulator version 4.1.1
cd code && make qemu       # OpenSBI 横幅显示 v0.4，Ctrl+A 再按 X 退出
make grade                 # 评测也会用这个 QEMU
```

## 说明

* 已在 Ubuntu 24.04、gcc 13.3、Python 3.12 上编译通过。编译时 `scripts/tracetool` 会打印一些 `SyntaxWarning`，无害。
* 脚本只编 `riscv64-softmmu`，并加了 `--enable-debug`（关闭优化、保留符号，供 GDB 调试 QEMU 源码）和 `--disable-werror`（防止新版 gcc 的新警告导致旧代码编译失败）。
* 交叉工具链不要求统一，riscv64-unknown-elf-gcc 10～15 均可。
