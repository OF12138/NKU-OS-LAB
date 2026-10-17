# 检查 report/sections/*.prompt.md 中 [RELY] 的代码块是否与当前 code/ 一致
# 比较前双方都去掉注释、行尾空白和空行，因此“省略注释”的引文可以通过；代码本身有出入的会被报告。
# 标注为“修改前”的引文（描述历史版本）跳过。
# 用法：python check-rely.py
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CODE = ROOT / "code"


def norm(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    out = []
    for ln in text.splitlines():
        ln = re.sub(r"//.*$", "", ln)
        ln = re.sub(r"(^|\s)#\s.*$", "", ln)            # 汇编 / Makefile 中的 “# 注释”
        if re.match(r"\s*#(?!include|define|ifndef|endif|ifdef|else)", ln):
            ln = ""
        ln = re.sub(r"\s+", " ", ln).strip()
        if ln:
            out.append(ln)
    return out


sources = {p: norm(p.read_text(encoding="utf-8", errors="ignore"))
           for p in CODE.rglob("*") if p.is_file() and p.suffix in {".c", ".h", ".S", ".ld", ".mk", ""}}


ALL_LINES = {ln for lines in sources.values() for ln in lines}


bad = 0
for pf in sorted((ROOT / "report" / "sections").glob("*.md")):
    text = pf.read_text(encoding="utf-8")
    for m in re.finditer(r"\[RELY\](.*?)\[GUARANTEE\]", text, re.S):
        rely = m.group(1)
        for b in re.finditer(r"([^\n]*)\n```[a-z]*\n(.*?)```", rely, re.S):
            ctx, code = b.group(1), b.group(2)
            if "修改前" in ctx or "修改前" in code.splitlines()[0]:
                continue
            # 逐行检查：RELY 常把多个文件、同一文件不同位置的片段摘在一起，所以不要求连续
            missing = [ln for ln in norm(code) if ln not in ALL_LINES and ln != "..."]
            if missing:
                bad += 1
                print(f"✗ {pf.name}：源码中找不到以下行\n    " + "\n    ".join(missing[:6]))
print("全部通过" if bad == 0 else f"共 {bad} 处不一致")
