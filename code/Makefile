# ===================================================================
# lab3 顶层 Makefile 简介
#
# 作用：用交叉编译器把 libs/ 与 kern/ 下的 .c/.S 编译、链接成内核
#       bin/kernel，再用 objcopy 剥成裸二进制 bin/ucore.img，
#       由 QEMU 把它加载到 0x80200000 运行（OpenSBI 跳转到该地址）。
#
# 常用目标：
#   make / make qemu   编译并在 QEMU 中运行
#   make debug / gdb   QEMU 停在入口等待调试（-s -S）/ 启动 GDB 连接 :1234
#   make test          以 -D ucore_test 重新编译内核并运行
#   make grade         拷贝到临时目录、注入测试后运行评分脚本
#   make clean         删除 obj/ 与 bin/
#
# 结构：1) 变量与工具链配置  2) 引入 tools/function.mk 提供的规则模板
#       3) 声明要编译的源文件目录  4) 生成 kernel、ucore.img 目标
#       5) 运行 / 调试 / 评分 / 清理等伪目标
# 具体的 “.c -> .o -> kernel” 规则不在本文件手写，而是由 function.mk
# 中的 add_files / create_target 等函数自动生成。
# ===================================================================

# ---------------- 基本变量 ----------------
# PROJ：项目名，只用于 handin 打包的文件名
PROJ	:= lab3
# EMPTY/SPACE/SLASH：空串、空格、路径分隔符，供 function.mk 拼接路径用
EMPTY	:=
SPACE	:= $(EMPTY) $(EMPTY)
SLASH	:= /

# V：命令前缀。默认 @ 隐藏命令回显，只显示 echo 的简短提示；make V= 可看到完整命令
V       := @

# 交叉工具链前缀（riscv64-unknown-elf-gcc / -ld / -objcopy / -gdb ...），可在命令行覆盖

#ifndef GCCPREFIX
GCCPREFIX := riscv64-unknown-elf-
#endif

# 模拟器：QEMU 未被外部指定时使用 PATH 中的 qemu-system-riscv64（lab2 统一用 4.1.1，
# 可在命令行 QEMU=<路径> 覆盖）；SPIKE 同理，为备选的 RISC-V 参考模拟器
ifndef QEMU
QEMU := qemu-system-riscv64
endif

ifndef SPIKE
SPIKE := spike
endif

# ---------------- 编译工具与选项 ----------------
# eliminate default suffix rules
.SUFFIXES: .c .S .h

# delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# define compiler and flags
# 主机编译器及选项，只用于 add_files_host（编译在宿主机上运行的小工具，本 lab 未用到）
HOSTCC		:= gcc
HOSTCFLAGS	:= -Wall -O2

# GDB：交叉调试器（make gdb 实际直接写了 riscv64-unknown-elf-gdb）
GDB		:= $(GCCPREFIX)gdb

# 交叉编译器与编译选项：
#   -mcmodel=medany    代码/数据可位于任意 ±2GB 范围，内核链接在 0x80200000 附近必需
#   -std=gnu99         C99 加 GNU 扩展（内联汇编等）
#   -Wno-unused -Werror  不报未使用警告，其余警告一律当错误
#   -fno-builtin -nostdinc  不用编译器内建函数与系统头文件，内核自带 libs/
#   -fno-stack-protector    不插入栈保护代码（内核里没有对应运行时）
#   -ffunction-sections -fdata-sections  每个函数/变量独立成段，配合链接 --gc-sections 裁剪
#   -g                 生成调试信息，供 GDB 使用
# DEFS 可从命令行传入（如 make DEFS=-DDEBUG）
CC		:= $(GCCPREFIX)gcc
CFLAGS  := -mcmodel=medany -std=gnu99 -Wno-unused -Werror 
CFLAGS	+= -fno-builtin -Wall -O2 -nostdinc $(DEFS)
CFLAGS	+= -fno-stack-protector -ffunction-sections -fdata-sections
CFLAGS	+= -g
# CFLAGS2：在 CFLAGS 基础上加 -D ucore_test，make test 时用于编译内核（用 = 延迟展开，以包含后面追加的 -I）
CFLAGS2 = $(CFLAGS) -D ucore_test
# CTYPE：参与编译的源文件后缀（C 与汇编 .S）
CTYPE	:= c S
# 链接器及选项：
#   -m elf64lriscv     输出 64 位小端 RISC-V ELF
#   -nostdlib          不链接标准库和启动文件
#   --gc-sections      丢弃未被引用的段（与上面 -ffunction-sections 配合）
LD      := $(GCCPREFIX)ld
LDFLAGS	:= -m elf64lriscv
LDFLAGS	+= -nostdlib --gc-sections

# OBJCOPY 用来把 ELF 转成裸二进制；OBJDUMP 用来生成反汇编和符号表
OBJCOPY := $(GCCPREFIX)objcopy
OBJDUMP := $(GCCPREFIX)objdump

# 常用 shell 命令的别名（TOUCH 的 -c 表示文件不存在时不创建）
COPY	:= cp
MKDIR   := mkdir -p
MV		:= mv
RM		:= rm -f
AWK		:= awk
SED		:= sed
SH		:= sh
TR		:= tr
TOUCH	:= touch -c

# OBJDIR：中间文件（.o/.d/.asm/.sym）目录；BINDIR：最终产物（kernel、ucore.img）目录
OBJDIR	:= obj
BINDIR	:= bin

# 由 function.mk 中的模板累积：全部 .o、全部 .d 依赖文件、全部最终目标
ALLOBJS	:=
ALLDEPS	:=
TARGETS	:=

# 引入规则模板库。提供 listf（列出目录下文件）、add_files（为一批源文件生成
# 编译规则并归入某个 packet）、create_target（把 packet 链接成目标）、
# read_packet、finish_all 等函数
include tools/function.mk

# listf_cc(目录)：列出目录下所有 .c/.S 文件
listf_cc = $(call listf,$(1),$(CTYPE))

# 交叉编译器版本的封装：add_files_cc(文件,packet名,额外选项,输出子目录)；
# add_files_cc2 使用 CFLAGS2（带 ucore_test）；create_target_cc 把 packet 链接成目标
# for cc
add_files_cc = $(call add_files,$(1),$(CC),$(CFLAGS) $(3),$(2),$(4))
add_files_cc2 = $(call add_files,$(1),$(CC),$(CFLAGS2) $(3),$(2),$(4))
create_target_cc = $(call create_target,$(1),$(2),$(3),$(CC),$(CFLAGS))

# 宿主机编译器版本的封装（本 lab 未使用）
# for hostcc
add_files_host = $(call add_files,$(1),$(HOSTCC),$(HOSTCFLAGS),$(2),$(3))
create_target_host = $(call create_target,$(1),$(2),$(3),$(HOSTCC),$(HOSTCFLAGS))

# 路径转换辅助：把 xxx.c 对应的 obj/xxx.o 换成 .asm/.out/.sym 后缀，
# 如 asmfile(kernel) = obj/kernel.asm，下面生成反汇编、符号表时用到
cgtype = $(patsubst %.$(2),%.$(3),$(1))
objfile = $(call toobj,$(1))
asmfile = $(call cgtype,$(call toobj,$(1)),o,asm)
outfile = $(call cgtype,$(call toobj,$(1)),o,out)
symfile = $(call cgtype,$(call toobj,$(1)),o,sym)

# for match pattern
# match(目标列表,正则列表)：用 awk 判断目标是否命中任一正则，命中输出 1，否则 0（下面决定是否 include .d 时用）
match = $(shell echo $(2) | $(AWK) '{for(i=1;i<=NF;i++){if(match("$(1)","^"$$(i)"$$")){exit 1;}}}'; echo $$?)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
# include kernel/user

# ---------------- 声明要编译的源文件 ----------------
# 公共库 libs/：头文件目录加入 -I，源文件归入 packet “libs”（输出到 obj/libs/）
INCLUDE	+= libs/

CFLAGS	+= $(addprefix -I,$(INCLUDE))

LIBDIR	+= libs

$(call add_files_cc,$(call listf_cc,$(LIBDIR)),libs,)

# -------------------------------------------------------------------
# kernel

# 内核部分：
# KINCLUDE：内核头文件搜索路径（-I）；KSRCDIR：内核源文件所在目录
# （kern/arch 只提供头文件，目录里没有需要编译的源文件）
KINCLUDE	+= kern/debug/ \
			   kern/driver/ \
			   kern/trap/ \
			   kern/mm/ \
			   kern/arch/

KSRCDIR		+= kern/init \
			   kern/libs \
			   kern/debug \
			   kern/driver \
			   kern/trap \
			   kern/mm

KCFLAGS		+= $(addprefix -I,$(KINCLUDE))

# make test 时用带 ucore_test 的选项编译内核，否则用普通选项；两者都归入 packet “kernel”
ifeq ($(MAKECMDGOALS),test)
$(call add_files_cc2,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
else
$(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
endif
# KOBJS：内核与公共库的全部 .o
KOBJS	= $(call read_packet,kernel libs)

# create kernel target
# 生成内核目标 bin/kernel：
# 依赖全部 .o 以及链接脚本 tools/kernel.ld（决定内存布局、入口地址）；
# 链接后顺便产生 obj/kernel.asm（反汇编，便于对照源码）和 obj/kernel.sym
# （符号表：只留 “地址 符号名”，sed 去掉表头及中间的段信息）
kernel = $(call totarget,kernel)

$(kernel): tools/kernel.ld

$(kernel): $(KOBJS)
	@echo + ld $@
	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)

# 把 kernel 登记进 TARGETS
$(call create_target,kernel)

# -------------------------------------------------------------------
# create ucore.img
# 生成 bin/ucore.img：用 objcopy 去掉符号和 ELF 头，得到纯二进制镜像，
# QEMU 通过 -device loader 把它原样放到 0x80200000
UCOREIMG	:= $(call totarget,ucore.img)


$(UCOREIMG): $(kernel)
	$(OBJCOPY) $(kernel) --strip-all -O binary $@

$(call create_target,ucore.img)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

# finish_all：根据 ALLOBJS 生成 “创建 obj/ bin/ 等输出目录” 的规则，并得到 ALLDEPS
$(call finish_all)

# 这些目标不需要（也不应）读入 .d 头文件依赖，其余目标才 -include
# （clean 时若还去包含 .d 会触发多余的生成）
IGNORE_ALLDEPS	= clean \
				  dist-clean \
				  grade \
				  touch \
				  print-.+ \
				  handin

ifeq ($(call match,$(MAKECMDGOALS),$(IGNORE_ALLDEPS)),0)
-include $(ALLDEPS)
endif

# ---------------- 默认目标与运行 / 调试 ----------------
# files for grade script

# 默认目标：直接 make 会构建所有目标（bin/kernel 和 bin/ucore.img）
TARGETS: $(TARGETS)

.DEFAULT_GOAL := TARGETS
# 以下目标不是文件。（debug、gdb 没有列入 .PHONY，目录下不要出现同名文件。）
# SWAPIMG、SFSIMG 在本 lab 中未定义，展开为空，是后续 lab 的遗留接口
.PHONY: qemu spike test
# make qemu：运行内核。-machine virt 使用 virt 开发板；-nographic 串口接终端；
# -bios default 使用 QEMU 自带的 OpenSBI（M 态固件，随后跳到内核）；
# -device loader 把 ucore.img 载入物理地址 0x80200000（OpenSBI 的跳转地址）
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000

# make debug：同 qemu，但加 -s（在 tcp:1234 开 gdbserver）和 -S（启动即暂停，等 GDB 连接）
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000\
		-s -S

# make gdb：另开终端运行，加载 bin/kernel 的符号并连接 QEMU 的 1234 端口
gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'

# make test：命令与 qemu 相同；区别在于上面 MAKECMDGOALS=test 时内核是带 -D ucore_test 编译的
test: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000
# make spike：用 Spike 模拟器直接运行镜像（备选）
spike: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(SPIKE) $(UCOREIMG)

# ---------------- 评分 ----------------
.PHONY: grade touch

# grade 脚本用到的临时文件名、打包名、make 选项和临时目录
GRADE_GDB_IN	:= .gdb.in
GRADE_QEMU_OUT	:= .qemu.out
HANDIN			:= proj$(PROJ)-handin.tar.gz

TOUCH_FILES		:= kern/trap/trap.c

MAKEOPTS		:= --quiet --no-print-directory

GRADE_TMP	:= /tmp/lab3_grade

# make grade：先 clean，把源码复制到 /tmp/lab3_grade，用 tools/inject.py 注入测试代码，
# 在该目录里运行 tools/grade.sh 评分，完成后删除临时目录（不污染工作区）
# grade.sh 直接调用 PATH 里的 qemu-system-riscv64；QEMU 给的是路径时，把它所在目录放到 PATH 最前面，
# 保证评分和 make qemu 用的是同一个 QEMU（4.1.1）
GRADE_PATH	:= $(if $(findstring /,$(QEMU)),PATH="$(dir $(QEMU)):$$PATH" )

grade:
	$(V)$(MAKE) $(MAKEOPTS) clean
	$(V)$(RM) -rf $(GRADE_TMP)
	$(V)$(MKDIR) $(GRADE_TMP)
	$(V)tar -C . --exclude=.git --exclude=.vscode --exclude=vcs -cf - . | tar -C $(GRADE_TMP) -xf -
	$(V)python3 tools/inject.py $(GRADE_TMP)
	$(V)cd $(GRADE_TMP) && $(GRADE_PATH)$(SH) tools/grade.sh
	$(V)$(RM) -rf $(GRADE_TMP)

# make touch：更新 TOUCH_FILES 中文件的时间戳（本 lab 为空，保留自框架）
touch:
	$(V)$(foreach f,$(TOUCH_FILES),$(TOUCH) $(f))

# make print-xxx：打印变量 XXX 的值，调试 Makefile 用（例如 make print-kobjs）
print-%:
	@echo $($(shell echo $(patsubst print-%,%,$@) | $(TR) [a-z] [A-Z]))

# ---------------- 清理与打包 ----------------
.PHONY: clean dist-clean handin packall tags
# clean：删除 obj/、bin/ 及评分和 cscope/tags 的临时文件
clean:
	$(V)$(RM) $(GRADE_GDB_IN) $(GRADE_QEMU_OUT) cscope* tags
	-$(RM) -r $(OBJDIR) $(BINDIR)

# dist-clean：在 clean 基础上再删除打包文件
dist-clean: clean
	-$(RM) $(HANDIN)

# handin / packall：clean 后把整个目录打成 proj$(PROJ)-handin.tar.gz（框架遗留，实际提交走 git）
handin: packall
	@echo Please visit http://learn.tsinghua.edu.cn and upload $(HANDIN). Thanks!

packall: clean
	@$(RM) -f $(HANDIN)
	@tar -czf $(HANDIN) `find . -type f -o -type d | grep -v '^\.*$$' | grep -vF '$(HANDIN)'`

# tags：用 cscope、ctags 生成代码索引
tags:
	@echo TAGS ALL
	$(V)rm -f cscope.files cscope.in.out cscope.out cscope.po.out tags
	$(V)find . -type f -name "*.[chS]" >cscope.files
	$(V)cscope -bq 
	$(V)ctags -L cscope.files
