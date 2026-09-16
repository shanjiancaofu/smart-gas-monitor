"""把 SSD1306 的帧缓冲 dump 渲染成文字，用来在看不到屏幕时确认面板内容。

配合 tools/ocd_dump_pages.gdb 使用：那个脚本把
1024 字节的帧缓冲写到文件，这里拿字模表逐格做匹配，还原出屏幕上的字符。

两种字号都支持，自动挑更像的那个：
  大号 8x16 —— 一行占两个 page，一屏 4 行（实时页/报警页/故障页/历史页）
  小号 6x8  —— 一行占一个 page，一屏 8 行（参数页）

用法:
    python tools/render_oled.py oled_dump.bin [更多 dump ...]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "..", "stm32f103", "bsp", "bsp_oled.c")

WIDTH = 128
PAGES = 8
# 匹配容差按每格位数给：字模和现场都可能有极少数噪点，但不足以让相邻字形混淆。
TOLERANCE = 0.15


def load_font(name):
    with open(SOURCE, encoding="utf-8", errors="replace") as handle:
        src = handle.read()
    start = src.index("static const uint8_t %s[" % name)
    brace = src.index("{", start)
    depth = 0
    for index in range(brace, len(src)):
        if src[index] == "{":
            depth += 1
        elif src[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
    sizes = []
    for row in re.findall(r"\{([^{}]*?)\}", src[brace:end + 1]):
        values = [int(v, 16) for v in re.findall(r"0x[0-9a-fA-F]+", row)]
        sizes.append(values)
    sizes = [g for g in sizes if len(g) in (6, 16)]
    if not sizes:
        raise SystemExit("没读到 %s" % name)
    stride = len(sizes[0])
    return sizes, stride


def geometry(stride):
    """由每字形字节数推出 (字形宽, 占几个 page)。

    大号 8x16 是 16 字节 = 8 宽 × 2 个 page，小号 6x8 是 6 字节 = 6 宽 × 1 个。
    行距必须从这里推，不要另外写死——之前就是把它跟「字形宽 8」搞混，大号被
    当成占一个 page，行号跑到 8 去了。
    """
    width = 6 if stride == 6 else 8
    return width, stride // width


def read_cell(fb, col, row, stride):
    """取一个字符格：该列在各 page 上的字节，按 page 顺序排。"""
    width, height_pages = geometry(stride)
    return bytes(fb[(row + p) * WIDTH + col + c]
                 for p in range(height_pages) for c in range(width))


def decode(fb, font, stride):
    """按某种字号解码整屏，返回 (平均位差比例, 文本行)。

    单格匹配不上的记为 '.'：宁可标成未知，也不要猜一个形状相近的字。
    """
    width, height_pages = geometry(stride)
    rows = range(0, PAGES, height_pages)
    lines = []
    total = 0.0
    cells = 0
    for row in rows:
        line = []
        # 步长是字形宽，不是 stride：大号字体 stride 是 16（8 宽 × 2 个 page），
        # 拿它当步长会隔一个字取一格。
        for col in range(0, WIDTH - (width - 1), width):
            cell = read_cell(fb, col, row, stride)
            best, score = None, None
            for index, glyph in enumerate(font):
                diff = sum(bin(a ^ b).count("1") for a, b in zip(cell, glyph))
                if score is None or diff < score:
                    score, best = diff, index
            ratio = score / (len(cell) * 8)
            total += ratio
            cells += 1
            line.append(chr(best + 32) if ratio <= TOLERANCE else ".")
        lines.append("".join(line).rstrip())
    return (total / cells if cells else 0.0), lines


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__.strip().splitlines()[-1].strip())
    wide, wide_stride = load_font("font8x16")
    small, small_stride = load_font("font6x8")
    for path in sys.argv[1:]:
        with open(path, "rb") as handle:
            fb = handle.read()[: WIDTH * PAGES]
        large_err, large_lines = decode(fb, wide, wide_stride)
        small_err, small_lines = decode(fb, small, small_stride)
        if small_err < large_err:
            font_name, error, lines = "6x8", small_err, small_lines
        else:
            font_name, error, lines = "8x16", large_err, large_lines
        print("=== %s  [%s, err %.3f] ===" % (os.path.basename(path), font_name, error))
        for line in lines:
            print("  %s" % line)
        print()


if __name__ == "__main__":
    main()
