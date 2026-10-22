# 把 tmux 窗格的内容渲染成浅色主题（One Half Light，与 Windows Terminal 浅色主题一致）的 PNG，不弹出任何窗口。
# 用法（在 Windows 上运行）：
#   wsl -e bash -c "tmux capture-pane -p -e -S -300 -t <会话:窗口> > /mnt/.../pane.ans"   # -e 保留颜色，-S 取历史记录
#   python tmux-render.py pane.ans out.png ["起始文本" ["结束文本"]]
# 给出起始文本时，从最后一个包含它的行开始截取；给出结束文本时，截到其后第一个包含它的行之前。
# 字体为 Cascadia Mono 26px，相当于 200% 缩放下的 13pt，适合放进报告。
import sys, re
from PIL import Image, ImageDraw, ImageFont
src, out = sys.argv[1], sys.argv[2]
lines = open(src, encoding='utf-8').read().rstrip('\n').split('\n')
if len(sys.argv) > 3:
    plain = [re.sub(r'\x1b\[[0-9;]*m', '', l) for l in lines]
    i = max(k for k, l in enumerate(plain) if sys.argv[3] in l)
    lines = lines[i:]
    if len(sys.argv) > 4:
        j = min(k for k, l in enumerate(plain[i:]) if sys.argv[4] in l)
        lines = lines[:j]
while lines and not re.sub(r'\x1b\[[0-9;]*m', '', lines[-1]).strip(): lines.pop()
pal = {30:'#383A42',31:'#E45649',32:'#50A14F',33:'#C18301',34:'#0184BC',35:'#A626A4',36:'#0997B3',37:'#A0A1A7'}
for k in list(pal): pal[k+60] = pal[k]
FG, BG = '#383A42', '#FAFAFA'
size = 26
font = ImageFont.truetype('C:/Windows/Fonts/CascadiaMono.ttf', size)
cw = font.getlength('M'); lh = int(size * 1.3); pad = 28
cols = max(len(re.sub(r'\x1b\[[0-9;]*m', '', l)) for l in lines)
img = Image.new('RGB', (int(cols * cw) + 2 * pad, len(lines) * lh + 2 * pad), BG)
d = ImageDraw.Draw(img)
for y, l in enumerate(lines):
    x, color = 0, FG
    for tok in re.split(r'(\x1b\[[0-9;]*m)', l):
        m = re.fullmatch(r'\x1b\[([0-9;]*)m', tok)
        if m:
            for c in (m.group(1) or '0').split(';'):
                c = int(c or 0)
                if c in (0, 39): color = FG
                elif c in pal: color = pal[c]
            continue
        for ch in tok:
            d.text((pad + x * cw, pad + y * lh), ch, font=font, fill=color)
            x += 1
img.save(out); print(out, img.size)
