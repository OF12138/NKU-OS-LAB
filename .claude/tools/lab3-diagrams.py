# 绘制 lab3 报告中的流程图：黑白、直角矩形、传统工科风格（matplotlib）
# 输出到 report/images/：T3-trap-flow.png
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, FancyArrowPatch
from pathlib import Path

plt.rcParams["font.family"] = ["Times New Roman", "SimSun"]
OUT = Path(__file__).resolve().parents[2] / "report" / "images"
LW, FS = 1.0, 10.5
MONO = ["Consolas", "SimSun"]  # 代码和地址用 Consolas，其中的中文回落到宋体


class Fig:  # 与 lab2-diagrams.py 中的同名类相同
    def __init__(self, w, h):
        self.fig = plt.figure(figsize=(w, h), dpi=200)
        self.ax = self.fig.add_axes([0, 0, 1, 1])
        self.ax.set_xlim(0, w)
        self.ax.set_ylim(0, h)
        self.ax.axis("off")

    def text(self, x, y, s, size=FS, ha="center", va="center", mono=False, bg=False, rotation=0):
        kw = dict(ha=ha, va=va, fontsize=size, linespacing=1.5, rotation=rotation)
        if mono:
            kw["fontfamily"] = MONO
        if bg:
            kw["bbox"] = dict(boxstyle="square,pad=0.12", facecolor="white", edgecolor="none")
        self.ax.text(x, y, s, **kw)

    def box(self, x, y, w, h, s="", size=FS, mono=False, ha="center", lw=LW):
        self.ax.add_patch(Rectangle((x, y), w, h, facecolor="white", edgecolor="black",
                                    linewidth=lw, joinstyle="miter"))
        if s:
            tx = x + w / 2 if ha == "center" else x + 0.15
            self.text(tx, y + h / 2, s, size, ha=ha, mono=mono)

    def diamond(self, cx, cy, w, h, s, size=FS, mono=False):
        self.ax.add_patch(Polygon([(cx - w / 2, cy), (cx, cy + h / 2), (cx + w / 2, cy), (cx, cy - h / 2)],
                                  closed=True, facecolor="white", edgecolor="black", linewidth=LW,
                                  joinstyle="miter"))
        self.text(cx, cy, s, size, mono=mono)

    def line(self, pts, arrow=True, dashed=False):
        """折线，pts 为 [(x, y), ...]；arrow 为 True 时最后一段带箭头"""
        ls = "--" if dashed else "-"
        for (x1, y1), (x2, y2) in zip(pts[:-2], pts[1:-1]):
            self.ax.plot([x1, x2], [y1, y2], color="black", linewidth=LW, linestyle=ls,
                         solid_joinstyle="miter")
        (x1, y1), (x2, y2) = pts[-2], pts[-1]
        if arrow:
            self.ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=12,
                                              linewidth=LW, color="black", linestyle=ls,
                                              shrinkA=0, shrinkB=0))
        else:
            self.ax.plot([x1, x2], [y1, y2], color="black", linewidth=LW, linestyle=ls)

    def save(self, name):
        self.fig.savefig(OUT / name, facecolor="white")
        plt.close(self.fig)
        print("saved", name)


# ---------------- T3：Trap 处理全流程 ----------------
def trap_flow():
    f = Fig(10.0, 12.9)
    X, W = 1.4, 6.6
    cx = X + W / 2
    LX = 8.8  # 右侧“由谁完成”标注
    f.box(X, 11.45, W, 1.2, "初始化（kern_init）\nidt_init：sscratch = 0，stvec = __alltraps\n"
          "clock_init：sie.STIE = 1，设置第一次时钟事件\nintr_enable：sstatus.SIE = 1", 10, mono=True)
    f.box(X, 10.35, W, 0.55, "正常执行：while (1) 空闲循环 / 评测插入的测试代码", 10)
    f.box(X, 8.75, W, 1.05, "硬件响应 Trap（时钟到期 / ebreak / 非法指令）\n"
          "sepc ← 被打断或出错的指令地址；scause、stval ← 原因与附加信息\n"
          "SPP ← 原特权级，SPIE ← SIE，SIE ← 0；pc ← stvec", 10, mono=True)
    f.box(X, 7.05, W, 1.15, "SAVE_ALL（__alltraps）\nsscratch ← sp；sp −= 288（36 个 8 字节槽位）\n"
          "保存 x1、x3～x31；原 sp 存入 2 号槽，sscratch ← 0\n保存 sstatus、sepc、stval、scause", 10, mono=True)
    f.box(X, 6.0, W, 0.5, "move a0, sp；jal trap → trap_dispatch(tf)", 10, mono=True)
    dy = 5.05
    f.diamond(cx, dy, 2.8, 0.8, "cause 最高位为 1？", 10)
    IX, IW, EX, EW, BY, BH = 0.85, 3.85, 5.0, 3.6, 2.55, 1.75
    f.box(IX, BY, IW, BH, "interrupt_handler\n时钟：设下一次事件，ticks++，\n每 100 次打印，打印 10 次后关机\n"
          "S 态软件中断：清 SSIP 并打印\n未知中断：打印 + print_trapframe", 10, mono=True)
    f.box(EX, BY, EW, BH, "exception_handler\n断点 / 非法指令：打印地址，\n"
          "按指令低两位 epc += 2 或 4\n其他异常：print_trapframe", 10, mono=True)
    f.box(X, 1.3, W, 0.85, "RESTORE_ALL（__trapret）\nsstatus、sepc ← 帧中的值；恢复 x1、x3～x31，最后恢复 sp",
          10, mono=True)
    f.box(X, 0.3, W, 0.5, "sret：pc ← sepc，特权级 ← SPP，SIE ← SPIE", 10, mono=True)
    # 主干
    for y1, y2 in [(11.45, 10.9), (10.35, 9.8), (8.75, 8.2), (7.05, 6.5), (6.0, dy + 0.4), (1.3, 0.8)]:
        f.line([(cx, y1), (cx, y2)])
    ic, ec = IX + IW / 2, EX + EW / 2
    f.line([(cx - 1.4, dy), (ic, dy), (ic, BY + BH)])
    f.text(cx - 1.5, dy + 0.18, "是（中断）", 10, ha="right")
    f.line([(cx + 1.4, dy), (ec, dy), (ec, BY + BH)])
    f.text(cx + 1.5, dy + 0.18, "否（异常）", 10, ha="left")
    f.line([(ic, BY), (ic, 2.15)])
    f.line([(ec, BY), (ec, 2.15)])
    # 返回被打断的执行流
    f.line([(X, 0.55), (0.35, 0.55), (0.35, 10.62), (X, 10.62)])
    f.text(0.2, 7.0, "回到 sepc 处继续执行", 10, rotation=90)
    # 右侧标注
    for y, s in [(9.27, "硬件"), (7.62, "trapentry.S"), (4.2, "trap.c"), (1.72, "trapentry.S"), (0.55, "硬件")]:
        f.text(LX, y, s, 10, ha="left")
    f.save("T3-trap-flow.png")


trap_flow()
