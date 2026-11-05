# 把 report/sections/ 下的各 section 合并进 report/report.md 和 report/prompt.md
#
# 两种模式：
#   python merge-report.py --init [完成日期]   首次生成。report.md 已存在时拒绝执行（防止覆盖人工修改），
#                                              确需重建时加 --force。
#   python merge-report.py --fill              只把 section 内容填进“【待补】”占位处，其余文字一律不动；
#                                              prompt.md 只追加尚未收录的提示词。
# 共同处理：../images/ → ./images/；指向 *.prompt.md 的相对链接改为纯文本；
#           全文图号按出现顺序统一为“图 1、图 2 …”，正文中对旧图号的引用同步替换。
# 占位符格式：> **【待补】说明** <!-- fill:文件通配符 -->   （HTML 注释在 Typora 中不显示）
import datetime
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SEC = ROOT / "report" / "sections"
REPORT = ROOT / "report" / "report.md"
PROMPT = ROOT / "report" / "prompt.md"
ORDER = ["T0", "T1", "T2", "T3", "T4", "T5"]  # prompt.md 中的顺序，与报告正文一致


def include_prompts(text):
    """把 section 中的 <!-- include:xxx.prompt.md --> 展开成该提示词文件的内容（功能模块的「最终提示词」）"""
    def repl(m):
        body = (SEC / m.group(1)).read_text(encoding="utf-8").strip()
        body = re.sub(r"^#{1,2} .*\n+", "", body, count=1)  # 去掉提示词文件自己的标题
        return "````markdown\n" + body + "\n````"
    return re.sub(r"<!-- include:(\S+\.prompt\.md) -->", repl, text)


def read_sec(pattern):
    files = [f for f in sorted(SEC.glob(pattern)) if not f.name.endswith(".prompt.md")]
    if not files:
        return None
    return "\n\n".join(include_prompts(f.read_text(encoding="utf-8").strip()) for f in files) + "\n"


def sec(pattern, desc):
    return read_sec(pattern) or f"> **【待补】{desc}** <!-- fill:{pattern} -->\n"


def tidy(text):
    """只统一格式，不改文字：图片路径、<img> 写法、图题居中方式、指向 prompt 文件的链接"""
    text = re.sub(r'<img\s+src="(?:\.\./|\./)?images/([^"]+)"[^>]*/?>', r"![](./images/\1)", text)
    text = text.replace("](../images/", "](./images/")
    text = re.sub(r'<p align="center">\s*(!\[[^\]]*\]\([^)]*\))\s*</p>', r"\1", text)  # 包在 <p> 里的图片拆成独立一行，否则 GitHub 不渲染
    text = re.sub(r"<center>(图[^<]*)</center>", r'<p align="center">\1</p>', text)  # 只改图题，标题里的 <center> 保留
    return re.sub(r"\[([^\]]+)\]\((T\d+-[^)]+\.prompt\.md)\)", "prompt.md", text)


def replace_region(text, sec_file, start, end):
    """把 report.md 中从标题 start 开始、到标题 end 之前的内容替换为 sec_file 的当前内容"""
    i = text.index("\n" + start) + 1
    j = text.index("\n" + end, i) + 1
    content = include_prompts((SEC / sec_file).read_text(encoding="utf-8").strip())
    return text[:i] + content + "\n\n\n" + text[j:]


def refresh_prompts(existing):
    """用各 prompt 文件的当前内容刷新 prompt.md 中已有的段落"""
    parts = re.split(r"(<!-- prompt:\S+ -->\n)", existing)
    out, done = [parts[0]], []
    for k in range(1, len(parts), 2):
        name = re.search(r"prompt:(\S+)", parts[k]).group(1)
        body = parts[k + 1]
        sep = "\n\n---\n\n" if body.rstrip().endswith("---") or k + 2 < len(parts) else "\n"
        f = SEC / name
        if f.exists():
            body = f.read_text(encoding="utf-8").strip() + (sep if k + 2 < len(parts) else "\n")
            done.append(name)
        out += [parts[k], body]
    return "".join(out), done


def renumber(text):
    labels = re.findall(r"(?:<center>|<p align=\"center\">)图\s*([0-9A-Za-z\-]+)　", text)
    assert len(set(labels)) == len(labels), f"图号重复：{labels}"
    mapping = {old: str(i + 1) for i, old in enumerate(labels)}
    for old in sorted(mapping, key=len, reverse=True):
        text = re.sub(rf"图\s*{re.escape(old)}(?![0-9A-Za-z\-])", "图 @@" + mapping[old] + "@@", text)
    return text.replace("@@", ""), len(labels)


def part(pattern, start, end, desc):
    """取 section 文件中从标题 start 开始、到标题 end 之前的部分（end 为 None 时到文件末尾）"""
    text = read_sec(pattern)
    if text is None:
        return sec(pattern, desc)
    i = text.index(start)
    j = text.index(end, i) if end else len(text)
    return re.sub(r"\n-{3,}\s*$", "", text[i:j].strip()) + "\n"  # 去掉末尾的 --- 分隔线


def init(date):
    header = f"""# <center>操作系统lab_实验报告</center>

<h5 align="center">实验名称：Lab 3: 断，都可以断（中断与异常处理）  完成日期：{date}</h5 align="center">
<h5 align="center">小组成员：2411264-张远、2414099-李云鹏、2413074-刘昀皓</h5 align="center">



# 小组分工

| 成员 | 负责的练习/模块/代码/报告 |
|------|----------------|
| 2411264-张远 | 环境与框架（补齐 lab2 代码、统一 QEMU 4.1.1）、功能模块一（RESTORE_ALL 与 `__alltraps`）、实验整体逻辑分析、报告集成与最终评测 |
| 2414099-李云鹏 | 功能模块二（interrupt_handler 与 exception_handler）、对操作系统的理解 |
| 2413074-刘昀皓 | 练习：指导书思考题、实验目的、实验环境、测试与验证、AI 协作开发的经验 |
"""
    t4 = "T4-common.md"
    parts = [
        header,
        "\n\n" + part(t4, "# 一、实验目的", "# 二、实验环境", "实验目的（T4，nagilix）"),
        "\n\n" + part(t4, "# 二、实验环境", "# 五、测试与验证", "实验环境（T4，nagilix）"),
        "\n\n# 三、实验整体逻辑分析\n\n" + sec("T3-logic.md", "整体逻辑（T3，openfar）"),
        "\n\n# 四、实验内容与实现\n\n" + sec("T1-*.md", "功能模块一（T1）")
        + "\n\n" + sec("T2-*.md", "功能模块二（T2）")
        + "\n\n" + sec("T4-questions.md", "练习：指导书思考题（T4，nagilix）"),
        "\n\n" + part(t4, "# 五、测试与验证", "# 六、实验总结与收获", "测试与验证（T4，nagilix）"),
        "\n\n# 六、实验总结与收获\n\n" + sec("T3-knowledge.md", "对操作系统的理解（T3，lyp）") + "\n\n"
        + part(t4, "## AI 协作开发的经验", None, "AI 协作开发的经验（T4，nagilix）"),
    ]
    return "".join(parts)


def fill(text):
    filled = []

    def repl(m):
        content = read_sec(m.group(2))
        if content is None:
            return m.group(0)
        filled.append(m.group(2))
        return content.rstrip("\n")

    text = re.sub(r"> \*\*【待补】([^*]+)\*\* <!-- fill:([^ ]+) -->", repl, text)
    return text, filled


def prompts(existing):
    """返回需要写入的 prompt.md 全文；existing 为现有内容（None 表示新建）"""
    have = set(re.findall(r"<!-- prompt:(\S+) -->", existing or ""))
    blocks = [] if existing else ["# Lab 3 提示词汇总\n\n各任务按指导书的四段式模板整理的最终版提示词。\n"]
    added = []
    for t in ORDER:
        for f in sorted(SEC.glob(f"{t}-*.prompt.md"), key=lambda f: ("-test." in f.name, f.name)):  # 同一任务先实现、后测试
            if f.name not in have:
                blocks.append(f"<!-- prompt:{f.name} -->\n" + f.read_text(encoding="utf-8").strip() + "\n")
                added.append(f.name)
    body = (existing.rstrip("\n") + "\n\n---\n\n" if existing else "") + "\n\n---\n\n".join(blocks)
    return body, added


args = [a for a in sys.argv[1:] if not a.startswith("--")]
flags = {a for a in sys.argv[1:] if a.startswith("--")}

if "--init" in flags:
    if REPORT.exists() and "--force" not in flags:
        sys.exit("report.md 已存在，为避免覆盖人工修改已中止。只补缺失部分请用 --fill；确需重建请加 --force。")
    text, n = renumber(tidy(init(args[0] if args else datetime.date.today().isoformat())))
    REPORT.write_text(text.rstrip() + "\n", encoding="utf-8", newline="\n")
    p, added = prompts(None)
    PROMPT.write_text(p, encoding="utf-8", newline="\n")
    print(f"已生成 report.md（图 {n} 张）和 prompt.md（{len(added)} 份提示词）")
elif "--fill" in flags:
    text, filled = fill(REPORT.read_text(encoding="utf-8"))
    text, n = renumber(tidy(text))
    REPORT.write_text(text.rstrip() + "\n", encoding="utf-8", newline="\n")
    p, added = prompts(PROMPT.read_text(encoding="utf-8") if PROMPT.exists() else None)
    PROMPT.write_text(p, encoding="utf-8", newline="\n")
    print(f"填入：{'、'.join(filled) or '无'}；prompt.md 新增：{'、'.join(added) or '无'}；全文图 {n} 张")
elif "--replace" in flags:
    # 用法：--replace <section 文件> "<起始标题行>" "<结束标题行>"，可重复多组（每组 3 个参数）
    text = REPORT.read_text(encoding="utf-8")
    for k in range(0, len(args), 3):
        text = replace_region(text, *args[k:k + 3])
        print(f"已替换：{args[k]}（{args[k + 1]} → {args[k + 2]} 之前）")
    text, n = renumber(tidy(text))
    REPORT.write_text(text.rstrip() + "\n", encoding="utf-8", newline="\n")
    print(f"全文图 {n} 张")
elif "--prompts" in flags:
    p, done = refresh_prompts(PROMPT.read_text(encoding="utf-8"))
    PROMPT.write_text(p, encoding="utf-8", newline="\n")
    print("prompt.md 已刷新：" + "、".join(done))
else:
    sys.exit("请指定 --init、--fill、--replace 或 --prompts")

missing = re.findall(r"【待补】([^*]+)", REPORT.read_text(encoding="utf-8"))
print("仍缺：" + ("、".join(missing) if missing else "无"))
