"""把标准库 oledfont.h 的字模导成本工程要的 C 数组。

工程里 bsp_oled.c 的 font6x8/font8x16 是坏的（'A'、'0' 画出来是噪点），
这里直接取参考工程里那套久经使用的 0806/1608 字模，按本工程的布局重排：

  索引   = ch - 32，覆盖 ASCII 32..126 共 95 个字形
  8x16   = 每字形 16 字节，前 8 字节是上半页（row 0..7）的 8 列，
           后 8 字节是下半页（row 8..15）的 8 列
  6x8    = 每字形 6 字节，一个页面的 6 列
  bit 0  = 该页最上面那个像素

参考工程 OLED_ShowChar() 的取字节顺序与上述布局完全一致，所以数据可直接搬。

用法:
    python tools/gen_oled_font.py            # 就地改写 bsp_oled.c
    python tools/gen_oled_font.py --check    # 只把字模打印成点阵图看
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TARGET = os.path.join(ROOT, "stm32f103", "bsp", "bsp_oled.c")
REFERENCE = (r"C:\Users\FFZNB\Desktop\课设\天然气监测系统 - 仿真"
             r"\HARDWARE\OLED\oledfont.h")

CHAR_FIRST = 32
CHAR_COUNT = 95

HEADER = """ASCII 32..126，列优先，bit 0 是 page 里最上面那个像素。
 * 取自参考工程的标准库字模，由 tools/gen_oled_font.py 生成，不要手工编辑。"""


def extract(path, marker, width):
    """抓出 `marker` 那个二维数组里所有宽度为 width 的 {..} 行。"""
    with open(path, encoding="utf-8", errors="replace") as handle:
        src = handle.read()
    start = src.index(marker)
    open_brace = src.index("{", start)
    depth = 0
    end = -1
    for index in range(open_brace, len(src)):
        if src[index] == "{":
            depth += 1
        elif src[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
    glyphs = []
    for row in re.findall(r"\{([^{}]*?)\}", src[open_brace:end + 1]):
        values = [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]+", row)]
        if len(values) == width:
            glyphs.append(values)
    return glyphs


def load_reference():
    wide = extract(REFERENCE, "asc2_1608[][16]", 16)
    small = extract(REFERENCE, "asc2_0806[][6]", 6)
    print("reference: asc2_1608 = %d 个字形, asc2_0806 = %d 个字形"
          % (len(wide), len(small)))
    if len(wide) < CHAR_COUNT:
        raise SystemExit("asc2_1608 不足 %d 个字形（ASCII 32..126）" % CHAR_COUNT)
    if len(small) < CHAR_COUNT:
        # 小号字体运行时没被用到，参考工程里少几个尾部字形不影响，补空补满。
        print("提示: asc2_0806 只有 %d 个，尾部 %d 个字形补空。"
              % (len(small), CHAR_COUNT - len(small)))
        small = small + [[0] * 6] * (CHAR_COUNT - len(small))
    return small[:CHAR_COUNT], wide[:CHAR_COUNT]


def render(table, width, text=None):
    """把字形拼成点阵图。text 给定时只画这几个字符，否则画整张表。

    width 是字形宽度，也决定高度：6 宽的字体占 1 个 page（8 行），
    8 宽的占 2 个 page（16 行），与 draw_char() 的 pages 取值一致。
    """
    glyphs = ([table[ord(ch) - CHAR_FIRST] for ch in text] if text is not None
              else table)
    pages = len(table[0]) // width
    lines = []
    for row in range(pages * 8):
        page, bit = divmod(row, 8)
        lines.append("".join("#" if (glyph[page * width + col] >> bit) & 1 else "."
                             for glyph in glyphs for col in range(width)))
    return lines


def glyph_comment(index):
    ch = index + CHAR_FIRST
    if ch == CHAR_FIRST:
        return " /* ' ' */"
    return " /* '%c' */" % ch


def emit(name, table, width, stride):
    """width 是像素宽，stride 是每个字形占的字节数（8x16 是 16，6x8 是 6）。"""
    out = ["/* %s: %s */" % (name, HEADER),
           "static const uint8_t %s[SSD1306_CHAR_COUNT][%d] = {" % (name, stride)]
    for index, glyph in enumerate(table):
        body = ", ".join("0x%02X" % b for b in glyph)
        tail = "," if index + 1 < len(table) else ""
        out.append("    {%s}%s%s" % (body, tail, glyph_comment(index)))
    out.append("};")
    return "\n".join(out)


def comment_start(source, anchor):
    """若 anchor 正上方紧挨着一段块注释，返回它的起点，否则返回行首。"""
    line_start = source.rfind("\n", 0, anchor) + 1
    open_comment = source.rfind("/*", 0, line_start)
    if open_comment < 0:
        return line_start
    between = source[open_comment:line_start]
    if "*/" not in between and set(between) <= set("/* \t\r\n"):
        return open_comment
    return line_start


def splice(source, name, text):
    marker = "static const uint8_t %s[" % name
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    end = -1
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                end = source.index(";", index)
                break
    return source[:comment_start(source, start)] + text + source[end + 1:]


def main():
    small, wide = load_reference()
    if "--check" in sys.argv:
        at = sys.argv.index("--check")
        text = sys.argv[at + 1] if len(sys.argv) > at + 1 else "A0! MQ47"
        for label, table, width in (("font8x16", wide, 8), ("font6x8", small, 6)):
            print("=== %s  %r ===" % (label, text))
            for line in render(table, width, text):
                print("   ", line)
        return
    with open(TARGET, encoding="utf-8") as handle:
        source = handle.read()
    for name, table, width, stride in (("font6x8", small, 6, 6),
                                       ("font8x16", wide, 8, 16)):
        source = splice(source, name, emit(name, table, width, stride))
    with open(TARGET, "w", encoding="utf-8", newline="") as handle:
        handle.write(source)
    print("wrote", TARGET)


if __name__ == "__main__":
    main()
