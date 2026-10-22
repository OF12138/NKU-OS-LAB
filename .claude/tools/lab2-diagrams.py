# 绘制 lab2 报告中的结构图与流程图：黑白、直角矩形、传统工科风格（matplotlib）
# 输出到 report/images/：T6-layers.png、T7-freelist.png、T7-dtb-flow.png、T7-probe-flow.png
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Polygon, FancyArrowPatch
from pathlib import Path

plt.rcParams["font.family"] = ["Times New Roman", "SimSun"]
OUT = Path(__file__).resolve().parents[2] / "report" / "images"
LW, FS = 1.0, 10.5
MONO = ["Consolas", "SimSun"]  # 代码和地址用 Consolas，其中的中文回落到宋体


class Fig:
    def __init__(self, w, h):
        self.fig = plt.figure(figsize=(w, h), dpi=200)
        self.ax = self.fig.add_axes([0, 0, 1, 1])
        self.ax.set_xlim(0, w)
        self.ax.set_ylim(0, h)
        self.ax.axis("off")

    def text(self, x, y, s, size=FS, ha="center", va="center", mono=False, bg=False):
        kw = dict(ha=ha, va=va, fontsize=size, linespacing=1.5)
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


# ---------------- T6：lab2 的分层结构 ----------------
def layers():
    f = Fig(10.4, 7.0)
    X, CW = 0.4, [2.3, 3.6, 3.7]  # 三列：层次 / 接口与数据结构 / 由谁实现
    xs = [X, X + CW[0], X + CW[0] + CW[1]]
    rows = [  # 自上而下
        ("任意大小的对象", "kmalloc / kfree", "SLUB（Challenge）"),
        ("连续的物理页", "alloc_pages / free_pages\nstruct pmm_manager", "First-Fit / Best-Fit / Buddy"),
        ("每一页的描述", "struct Page pages[]", "page_init（功能模块一）"),
        ("物理内存的范围", "get_memory_base\nget_memory_size", "dtb_init（框架）"),
        ("地址怎样被访问", "boot_page_table_sv39\nsatp（Sv39）", "entry.S（框架）\n分支任务：观察 QEMU 查表"),
    ]
    notes = [  # 相邻两层之间：上层依赖下层的什么
        "按对象大小分到 7 个缓存，一个 slab 占一页",
        "空闲块怎样组织、怎样挑选",
        "哪些页保留、哪些页空闲",
        "设备树给出内存从哪里开始、有多大",
    ]
    H, GAP, TOP = 0.78, 0.42, 6.25
    for k, head in enumerate(("层次", "接口与数据结构", "实现")):
        f.text(xs[k] + CW[k] / 2, TOP + 0.3, head, 11)
    for i, row in enumerate(rows):
        y = TOP - (i + 1) * H - i * GAP
        for k, s in enumerate(row):
            f.box(xs[k], y, CW[k], H, s, 10.5, mono=(k == 1))
        if i < len(rows) - 1:  # 下一层指向本层：本层建立在下一层之上
            ax_ = xs[1] + 0.5
            f.line([(ax_, y - GAP), (ax_, y)])
            f.text(ax_ + 0.15, y - GAP / 2, notes[i], 9.5, ha="left")
    f.text(sum(CW) / 2 + X, 0.28, "箭头方向：下层为上层提供服务；中间三层对应实现链 pmm_init → page_init → init_memmap → alloc_pages / free_pages", 9.5)
    f.save("T6-layers.png")


# ---------------- T7：First-Fit 空闲链表 ----------------
def freelist():
    f = Fig(10.4, 3.9)
    Y, H = 1.35, 1.6
    f.box(0.3, Y + 0.35, 1.7, 0.9, "free_list\n（哨兵节点）", 10)
    pages = [("A", 4, "A ~ A+3"), ("B", 2, "B ~ B+1"), ("C", 8, "C ~ C+7")]
    xs = [2.75, 5.3, 7.85]
    W = 2.15
    for x, (n, prop, frames) in zip(xs, pages):
        f.box(x, Y, W, H)
        f.ax.plot([x, x + W], [Y + H - 0.42, Y + H - 0.42], color="black", linewidth=LW)
        f.text(x + W / 2, Y + H - 0.21, f"Page {n}（块首页）", 10)
        f.text(x + 0.15, Y + 0.58, f"property = {prop}\nPG_property = 1\n物理页帧 {frames}", 9.5, ha="left", mono=True)
    # 相邻节点之间的 next（上）/ prev（下）
    lefts = [2.0] + [x + W for x in xs[:-1]]
    rights = xs
    for l, r in zip(lefts, rights):
        f.line([(l, Y + 1.0), (r, Y + 1.0)])
        f.line([(r, Y + 0.6), (l, Y + 0.6)])
    # 环形：C 的 next 回到哨兵，哨兵的 prev 指向 C
    xr = xs[-1] + W
    f.line([(xr, Y + 0.3), (xr + 0.2, Y + 0.3), (xr + 0.2, 0.75), (1.15, 0.75), (1.15, Y + 0.35)])
    f.text(5.4, 0.53, "C 的 next 指回哨兵（哨兵的 prev 指向 C），构成双向循环链表", 9.5)
    f.text(2.35, Y + 1.15, "next", 9, mono=True)
    f.text(2.35, Y + 0.45, "prev", 9, mono=True)
    f.text(5.4, 3.55, "链表只挂每个空闲块的首页，按物理地址升序：PA_A < PA_B < PA_C", 10)
    f.save("T7-freelist.png")


# ---------------- T7：设备树传递内存范围 ----------------
def dtb_flow():
    f = Fig(6.8, 6.2)
    X, W, H = 0.9, 3.0, 0.75
    cx = X + W / 2
    stages = [
        (5.0, "Bootloader / OpenSBI\n（M 态）"),
        (3.35, "ucore 内核入口\n（S 态）"),
        (1.95, "DTB 解析模块\n（dtb.c）"),
        (0.4, "可用物理内存区间\n[mem_begin, mem_begin + mem_size)"),
    ]
    for y, s in stages:
        f.box(X, y, W, H, s, 10.5)
    labels = [
        "① 初始化平台硬件，探测 RAM\n② 把硬件拓扑编码为 DTB\n③ 把 DTB 的物理地址放入 a1",
        "从 a1 取得 DTB 的基地址",
        "遍历 /memory 节点，读取 reg 属性",
    ]
    for (y1, _), (y2, _), s in zip(stages[:-1], stages[1:], labels):
        f.line([(cx, y1), (cx, y2 + H)])
        f.text(cx + 0.2, (y1 + y2 + H) / 2, s, 10, ha="left")
    f.save("T7-dtb-flow.png")


# ---------------- T7：裸机主动探测流程 ----------------
def probe_flow():
    f = Fig(10.2, 11.6)
    cx, W, H = 3.4, 4.2, 0.62
    X = cx - W / 2
    ex = 8.3  # 右侧“记录上界”一列
    f.box(X, 10.75, W, 0.5, "开始", 10.5)
    f.box(X, 9.75, W, H, "probe_pa = DRAM_BASE（0x80000000）\nstep = 1 MiB（或 4 KiB）", 10, mono=True)
    f.box(X, 8.75, W, H, "安装临时 trap 入口\nstvec → probe_trap_entry", 10, mono=True)
    f.box(X, 7.15, W, 1.05, "① 保存原值 old_val = *probe_pa\n② 写入魔数 0xAA55AA55\n③ fence\n④ 回读 read_val = *probe_pa", 10, mono=True, ha="left")
    f.diamond(cx, 6.2, 3.2, 0.9, "发生 Access Fault？", 10)
    f.diamond(cx, 4.9, 3.2, 0.9, "read_val == 魔数？", 10, mono=True)
    f.box(X, 3.2, W, 0.9, "⑤ 写入反转魔数 0x55AA55AA 并回读\n⑥ 检查 DRAM_BASE 处的基准值\n   是否被改写（地址回卷）", 10, mono=True, ha="left")
    f.diamond(cx, 2.2, 3.2, 0.9, "校验通过且未回卷？", 10)
    f.box(X, 0.55, W, 0.8, "⑦ 恢复原值 *probe_pa = old_val\nprobe_pa += step", 10, mono=True)
    # 主干
    for y1, y2 in [(10.75, 9.75 + H), (9.75, 8.75 + H), (8.75, 8.2), (7.15, 6.65), (5.75, 5.35),
                   (4.45, 4.1), (3.2, 2.65), (1.75, 1.35)]:
        f.line([(cx, y1), (cx, y2)])
    for y in (5.6, 4.27, 1.55):
        f.text(cx + 0.18, y, "否" if y == 5.6 else "是", 10, ha="left")
    # 回到循环开头（汇入 trap 入口与 ①～④ 之间的主干）
    f.line([(X, 0.95), (0.35, 0.95), (0.35, 8.47), (cx, 8.47)])
    f.text(0.5, 4.9, "下一个地址", 10, ha="left")
    # 右侧：三种结束条件都在这里记录上界
    EW, SX, SW = 2.6, 8.7, 2.0
    f.box(ex - EW / 2, 4.55, EW, H + 0.08, "到达物理内存边界\nmax_pa = probe_pa", 10, mono=True)
    f.box(SX - SW / 2, 3.25, SW, H, "恢复正常的 stvec", 10, mono=True)
    f.box(SX - SW / 2, 1.95, SW, H, "探测完成\n进入分配器初始化", 10)
    f.line([(cx + 1.6, 6.2), (ex, 6.2), (ex, 4.55 + H + 0.08)])
    f.text(cx + 1.75, 6.37, "是（trap 截断）", 10, ha="left")
    f.line([(cx + 1.6, 4.9), (ex - EW / 2, 4.9)])
    f.text(cx + 1.75, 5.07, "否（不是 RAM）", 10, ha="left")
    f.line([(cx + 1.6, 2.2), (7.25, 2.2), (7.25, 4.55)])
    f.text(cx + 1.75, 2.37, "否", 10, ha="left")
    f.line([(SX, 4.55), (SX, 3.25 + H)])
    f.line([(SX, 3.25), (SX, 1.95 + H)])
    f.save("T7-probe-flow.png")


layers()
freelist()
dtb_flow()
probe_flow()
