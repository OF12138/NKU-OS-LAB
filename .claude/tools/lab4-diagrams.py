# 绘制 lab4 报告中的图：黑白、直角矩形、传统工科风格（matplotlib）
# 输出到 report/images/：T2-kstack-firstrun.png
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


# ---------------- T2：新线程的内核栈布局与首次运行链路 ----------------
def kstack_firstrun():
    f = Fig(12.6, 7.6)
    # 左：PCB 中与首次运行有关的字段
    f.text(1.55, 7.25, "proc_struct（PCB）", 10.5)
    f.box(0.3, 5.15, 2.5, 1.8, "", 10)
    f.text(1.55, 6.55, "context.ra = forkret", 10, mono=True)
    f.text(1.55, 6.05, "context.sp = proc->tf", 10, mono=True)
    f.text(1.55, 5.55, "tf = proc->tf", 10, mono=True)
    # 中：内核栈
    SX, SW = 4.0, 3.2
    f.text(SX + SW / 2, 7.25, "新线程的内核栈（KSTACKSIZE = 8 KiB）", 10.5)
    f.box(SX, 4.3, SW, 2.65, "", 10)
    f.text(SX + SW / 2, 6.68, "trapframe（copy_thread 复制）", 10)
    rows = ["epc = kernel_thread_entry", "status = SPP|SPIE，SIE = 0", "gpr.s0 = fn", "gpr.s1 = arg",
            "gpr.sp = proc->tf", "gpr.a0 = 0"]
    for i, s in enumerate(rows):
        f.text(SX + SW / 2, 6.3 - i * 0.33, s, 9.5, mono=True)
    f.text(SX + SW + 0.08, 6.95, "kstack + KSTACKSIZE（栈顶）", 9.5, ha="left", mono=True)
    f.text(SX + SW + 0.08, 4.3, "proc->tf", 9.5, ha="left", mono=True)
    f.box(SX, 1.0, SW, 3.3, "", 10)
    f.text(SX + SW / 2, 2.9, "空闲栈空间（向低地址增长）\nsret 之后 fn 在这里运行", 10)
    f.line([(SX + SW / 2, 4.05), (SX + SW / 2, 3.45)])
    f.text(SX + SW + 0.08, 1.0, "proc->kstack（低地址）", 9.5, ha="left", mono=True)
    # context.sp 与 tf 都指向 trapframe 的起始地址（栈中 trapframe 的低端）
    f.line([(2.8, 6.05), (3.45, 6.05), (3.45, 4.3)], arrow=False)
    f.line([(2.8, 5.55), (3.45, 5.55)], arrow=False)
    f.line([(3.45, 4.3), (SX, 4.3)])
    # 右：首次运行链路
    RX, RW = 9.6, 2.8
    steps = ["schedule → proc_run\nswitch_to(&prev->context,\n          &proc->context)",
             "恢复 ra、sp、s0～s11\nret → context.ra",
             "forkret()\nforkrets(current->tf)",
             "move sp, a0\n__trapret：RESTORE_ALL",
             "sret → tf->epc\n（S 态，开中断）",
             "kernel_thread_entry\na0 ← s1；jalr s0",
             "fn(arg)，例如\ninit_main(\"Hello world!!\")"]
    f.text(RX + RW / 2, 7.25, "首次被调度时的执行路径", 10.5)
    h, gap, top = 0.74, 0.2, 6.95
    for i, s in enumerate(steps):
        y = top - h - i * (h + gap)
        f.box(RX, y, RW, h, s, 9.5, mono=True)
        if i:
            f.line([(RX + RW / 2, y + h + gap), (RX + RW / 2, y + h)])
    f.save("T2-kstack-firstrun.png")


kstack_firstrun()
