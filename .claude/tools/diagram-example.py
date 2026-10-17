# 绘制 lab1 启动流程与物理内存布局示意图：黑白、直角矩形、传统工科风格（matplotlib）
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyArrowPatch

plt.rcParams["font.family"] = ["Times New Roman", "SimSun"]
plt.rcParams["hatch.linewidth"] = 0.6

W_U, H_U = 11.6, 7.7
fig = plt.figure(figsize=(W_U, H_U), dpi=200)
ax = fig.add_axes([0, 0, 1, 1])
ax.set_xlim(0, W_U)
ax.set_ylim(0, H_U)
ax.axis("off")
LW = 1.0
FS = 10.5
MONO = "Consolas"


def text(x, y, s, size=FS, ha="center", family=None, bg=False):
    kw = dict(ha=ha, va="center", fontsize=size, linespacing=1.5)
    if family:
        kw["fontfamily"] = family
    if bg:
        kw["bbox"] = dict(boxstyle="square,pad=0.15", facecolor="white", edgecolor="none")
    ax.text(x, y, s, **kw)


def box(x, y, w, h, s="", size=FS, hatch=None, align="center"):
    ax.add_patch(Rectangle((x, y), w, h, facecolor="white", edgecolor="black",
                           linewidth=LW, hatch=hatch, joinstyle="miter"))
    if s:
        if align == "center":
            text(x + w / 2, y + h / 2, s, size, bg=bool(hatch))
        else:
            text(x + 0.14, y + h / 2, s, size, ha="left")


def line(x1, y1, x2, y2, dashed=False, arrow=False, lw=LW, ls=None):
    if arrow:
        ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=13,
                                     linewidth=lw, color="black", linestyle="--" if dashed else "-",
                                     shrinkA=0, shrinkB=0))
    else:
        ax.plot([x1, x2], [y1, y2], color="black", linewidth=lw, linestyle=ls or ("--" if dashed else "-"))


# ---------------- (a) 物理地址空间 ----------------
text(2.45, 7.4, "(a) 物理地址空间（示意，非等比例）", 11.5)
X, W = 1.65, 2.6
regions = [  # (y, h, 起始地址, 内容, 斜线)
    (0.75, 0.7, "0x00001000", "MROM：复位代码\n+ fw_dynamic_info", None),
    (1.9, 0.7, "0x80000000", "OpenSBI 代码\nM:(R,X)   S/U:无", None),
    (2.6, 0.7, "0x80040000", "OpenSBI 数据 / 栈\nM:(R,W)   S/U:无", "///"),
    (3.75, 0.6, "0x80200000", "内核 .text / .rodata", None),
    (4.35, 0.6, "0x80201000", "bootstack（8 KB）", None),
    (5.4, 0.6, "0x87E00000", "设备树 DTB", None),
]
for y, h, addr, s, hat in regions:
    box(X, y, W, h, s, 10, hatch=hat)
    text(X - 0.1, y, addr, 9.5, ha="right", family=MONO)
for y in (1.65, 3.5, 5.15, 6.25):  # 区间之间的省略号
    ax.plot([X + W / 2] * 3, [y - 0.08, y, y + 0.08], "k.", markersize=2.5)
text(X - 0.1, 4.95, "0x80203000", 9.5, ha="right", family=MONO)
text(X + W + 0.1, 4.95, "← bootstacktop", 9.5, ha="left")
line(X, 6.55, X + W, 6.55)
text(X - 0.1, 6.55, "0x88000000", 9.5, ha="right", family=MONO)
text(X + W / 2, 6.8, "DRAM 末端（128 MB）", 9.5)
text(X + W + 0.1, 2.95, "← 进入内核时\n   sp = 0x80046eb0", 9.5, ha="left")
text(X + W / 2, 0.4, "斜线区：PMP 禁止 S/U 态访问", 9.5)

# 分隔线
line(5.75, 0.3, 5.75, 7.2, lw=0.6, ls=":")

# ---------------- (b) 启动接力 ----------------
BX, BW = 6.2, 4.6
text(BX + BW / 2, 7.4, "(b) 启动接力与特权级切换", 11.5)
stages = [  # (y, h, 内容, 特权级)
    (5.95, 1.0, "① MROM 复位代码\nPC = 0x1000（由 QEMU 生成）\na0 = hartid，a1 = DTB，a2 = &fw_dynamic_info", "[M 态]"),
    (3.4, 1.75, "② OpenSBI 固件\nPC = 0x80000000\n保存参数 → hart 抽签 → 重定位、清零 .bss\n"
                "→ 设置 mtvec → 探测 CSR 与 semihosting\n→ 初始化串口、定时器，配置 PMP 与委托\n→ 打印 banner", "[M 态]"),
    (1.15, 1.05, "③ ucore 内核\nPC = 0x80200000（kern_entry）\nla sp, bootstacktop → tail kern_init → cprintf", "[S 态]"),
]
LH = 0.18  # 10 号字、1.5 倍行距下的实测行距（逻辑单位），用于把特权级标签对齐到首行
for y, h, s, tag in stages:
    box(BX, y, BW, h, s, 10, align="left")
    n = len(s.split("\n"))
    text(BX + BW - 0.14, y + h / 2 + (n - 1) * LH / 2, tag, 10, ha="right")
AX = BX + 1.2
line(AX, 5.95, AX, 5.15, arrow=True)
text(AX + 0.15, 5.55, "jr t0（t0 = 0x80000000）", 10, ha="left")
line(AX, 3.4, AX, 2.2, arrow=True)
text(AX + 0.15, 2.8, "mret\nmepc = 0x80200000\nmstatus.MPP = 1（S 态）", 10, ha="left")
RX1, RX2 = BX + BW - 0.55, BX + BW - 0.25
line(RX1, 2.2, RX1, 3.4, dashed=True, arrow=True)
text(RX1 - 0.12, 2.8, "ecall\n(mcause = 9)", 10, ha="right")
line(RX2, 3.4, RX2, 2.2, dashed=True, arrow=True)
text(RX2 + 0.1, 2.8, "mret", 10, ha="left")
text(BX + BW / 2, 0.75, "实线：启动阶段的一次性控制转移；虚线：运行期的 SBI 服务调用", 9.5)
text(BX + BW / 2, 0.45, "地址取自 QEMU 8.2.2 / OpenSBI v1.3 的实测结果", 9.5)

fig.savefig(r"F:\Code\OS\NKU-OS-LAB\report\images\T2-0-boot-overview.png", facecolor="white")
print("ok")
