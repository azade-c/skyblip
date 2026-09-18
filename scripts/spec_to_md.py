import re, sys

src, dst = sys.argv[1], sys.argv[2]
lines = open(src, encoding="utf-8").read().split("\n")

FOOTER = re.compile(r"^\s*ED Decision 2022/024/R\s*(Page \d+ of \d+)?\s*$")
HEADER = re.compile(r"^\s*ADS-L 4 S[DR]{2}-860 Issue \d\s")
PAGENO = re.compile(r"^\s*Page \d+ of \d+\s*$")
SUBPART = re.compile(r"^\s*(SUBPART [A-Z].*?)\s*$")
CLAUSE = re.compile(r"^\s*(ADS-L\.4\.SRD860\.[A-Z](?:\.\d+)*)\s{1,}(\S.*?)\s*$")
PLAIN_HEAD = re.compile(r"^\s*(PREAMBLE|DEFINITIONS AND ABBREVIATIONS|TABLE OF CONTENTS|APPENDIX.*)\s*$")
TOC_ENTRY = re.compile(r"\.{4,}\s*\d+\s*$|\s\.{4,}\s")
COLUMNS = re.compile(r"\S {2,}\S")


def without_page_furniture(lines):
    return [l.replace("\f", "").rstrip() for l in lines
            if not (FOOTER.match(l) or HEADER.match(l) or PAGENO.match(l))]


def without_repeated_blanks(lines):
    out = []
    for line in lines:
        if not line.strip() and out and not out[-1].strip():
            continue
        out.append(line)
    return out


body = without_repeated_blanks(without_page_furniture(lines))

out = []
para = []
block = []


def flush_para():
    if para:
        out.append(" ".join(w.strip() for w in para))
        out.append("")
        para.clear()


def flush_block():
    while block and not block[-1].strip():
        block.pop()
    if block:
        out.append("```")
        indent = min((len(b) - len(b.lstrip()) for b in block if b.strip()), default=0)
        out.extend(b[indent:] if b.strip() else "" for b in block)
        out.append("```")
        out.append("")
        block.clear()


def heading(line):
    if TOC_ENTRY.search(line):
        return None
    m = SUBPART.match(line)
    if m and "..." not in line:
        return "## " + m.group(1).replace("\u2014", "-").replace("\u2013", "-")
    m = PLAIN_HEAD.match(line)
    if m:
        return "## " + m.group(1)
    m = CLAUSE.match(line)
    if m:
        depth = m.group(1).count(".") - 4
        return "#" * min(3 + depth, 6) + f" {m.group(1)} {m.group(2)}"
    return None


i = 0
in_toc = False
while i < len(body):
    line = body[i]
    i += 1
    stripped = line.strip()

    head = heading(line)
    if head:
        flush_para()
        flush_block()
        in_toc = head.endswith("TABLE OF CONTENTS")
        out.append(head)
        out.append("")
        continue

    if in_toc:
        continue

    if not stripped:
        flush_para()
        block.append("")
        if not any(b.strip() for b in block):
            block.clear()
        continue

    tabular = bool(COLUMNS.search(stripped)) or stripped.startswith(("|", "+--")) or len(line) - len(line.lstrip()) >= 8
    if tabular:
        flush_para()
        block.append(line)
    else:
        if any(b.strip() for b in block):
            nxt = body[i] if i < len(body) else ""
            if COLUMNS.search(nxt.strip()):
                block.append(line)
                continue
            flush_block()
        else:
            block.clear()
        para.append(stripped)

flush_para()
flush_block()

text = re.sub(r"\n{3,}", "\n\n", "\n".join(out))
open(dst, "w", encoding="utf-8").write(text.strip() + "\n")
