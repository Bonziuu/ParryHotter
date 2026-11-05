#!/usr/bin/env python3
"""Generate 100% original 32-bit TGA sprites for the Parry Hotter Gothic 1 HUD.

No Pillow, ImageMagick or game assets required. Standard-library only.
Can be run by the one-click installer on the user's machine.
"""
from __future__ import annotations

import math
from pathlib import Path

N = 64
COLORS = (
    (91, 206, 255),   # Flipendo: sky-blue whorl
    (204, 157, 255),  # Levicorpus: violet inverted hook
    (255, 185, 76),   # Rictusempra: golden laughing zig-zag
    (255, 110, 112),  # Expelliarmus: red crossing wands
    (255, 122, 45),   # Bumbardo: fire burst
    (85, 247, 142),   # Avada: emerald lightning
)
NAMES = (
    "FLIPENDO", "LEVICORPUS", "RICTUSEMPRA", "EXPELLIARMUS", "BUMBARDO", "AVADA"
)


def blank():
    return [(0, 0, 0, 0) for _ in range(N * N)]


def color_pixel(img, x, y, rgb, a=255):
    if not (0 <= x < N and 0 <= y < N):
        return
    idx = y * N + x
    previous = img[idx]
    alpha = a / 255.0
    inv = 1.0 - alpha
    img[idx] = (
        int(rgb[0] * alpha + previous[0] * inv),
        int(rgb[1] * alpha + previous[1] * inv),
        int(rgb[2] * alpha + previous[2] * inv),
        min(255, a + int(previous[3] * inv))
    )


def segment(img, a, b, rgb, width=2, alpha=255):
    x1, y1 = a
    x2, y2 = b
    xmin, xmax = math.floor(min(x1, x2) - width - 1), math.ceil(max(x1, x2) + width + 1)
    ymin, ymax = math.floor(min(y1, y2) - width - 1), math.ceil(max(y1, y2) + width + 1)
    dx, dy = x2 - x1, y2 - y1
    total = dx * dx + dy * dy
    for y in range(max(0, ymin), min(N, ymax + 1)):
        for x in range(max(0, xmin), min(N, xmax + 1)):
            if total < 1:
                u = 0.0
            else:
                u = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / total))
            dist = math.hypot(x - (x1 + u * dx), y - (y1 + u * dy))
            if dist <= width:
                color_pixel(img, x, y, rgb, int(alpha * min(1.0, width - dist + 0.5)))


def draw_path(img, pts, rgb, width, alpha=255):
    for a, b in zip(pts[:-1], pts[1:]):
        segment(img, a, b, rgb, width, alpha)


def glyph_path(i):
    if i == 0:  # push wave with arrow - distinct from Rictusempra spiral
        return [(10, 22), (16, 17), (24, 22), (31, 17), (39, 22),
                (18, 34), (26, 29), (34, 34), (43, 29),
                (32, 42), (48, 42), (42, 36), (48, 42), (42, 48)]
    if i == 1:  # hanging inverted person
        return [(32, 15), (32, 44), (25, 50), (17, 44), (17, 34), (24, 30),
                (32, 34), (40, 30), (47, 34), (47, 44)]
    if i == 2:  # HP2-inspired fiery spiral like the user's screenshot
        return [(17, 46), (11, 35), (14, 24), (23, 16), (36, 14),
                (46, 21), (51, 33), (47, 45), (37, 49), (27, 45),
                (22, 35), (27, 27), (36, 26), (42, 32), (41, 40),
                (33, 43), (28, 38), (29, 33), (35, 31)]
    if i == 3:  # crossed disarming wands
        return [(15, 17), (48, 49), (40, 41), (48, 49), (50, 38), (48, 49),
                (17, 17), (15, 17), (26, 19)]
    if i == 4:  # blast star burst
        return [(32, 10), (36, 25), (51, 17), (41, 32), (55, 39), (38, 38),
                (32, 55), (26, 38), (11, 46), (22, 31), (12, 21), (28, 25), (32, 10)]
    return [(42, 10), (23, 30), (33, 30), (21, 51), (46, 25), (34, 25), (42, 10)]


def glyph(img, i, target=False):
    rgb = COLORS[i]
    path = glyph_path(i)
    if target:
        draw_path(img, path, rgb, 6.2, 60)
        draw_path(img, path, rgb, 3.4, 150)
        draw_path(img, path, (255, 255, 230), 1.35, 255)
    else:
        draw_path(img, path, rgb, 4.0, 55)
        draw_path(img, path, rgb, 2.0, 250)


def box_border(img):
    # dark translucent beveled square
    for y in range(2, N - 2):
        for x in range(2, N - 2):
            color_pixel(img, x, y, (7, 11, 23), 165)
    for k in range(3):
        for t in range(3, N - 3):
            rgb = (105, 99, 82) if k == 0 else (59, 59, 71)
            for x, y in ((t, 3+k), (t, N-4-k), (3+k, t), (N-4-k, t)):
                color_pixel(img, x, y, rgb, 230)


def selected_border(img):
    for k in range(4):
        for t in range(2, N-2):
            a = 255 - k*48
            for x,y in ((t, 2+k), (t,N-3-k), (2+k,t), (N-3-k,t)):
                color_pixel(img, x, y, (255,226,135), a)


def crosshair():
    canvas = blank()
    for y in range(28, 37):
        for x in range(28, 37):
            dist = math.hypot(x - 32, y - 32)
            if dist <= 3.6:
                color_pixel(canvas, x, y, (5, 9, 17), 160)
            if dist <= 1.35:
                color_pixel(canvas, x, y, (231, 236, 226), 235)
    return canvas


def write_tga(path, img):
    # Uncompressed BGRA with top-left origin.
    header = bytes([0, 0, 2, 0,0,0,0,0, 0,0,0,0,
                    N & 255, (N >> 8) & 255, N & 255, (N >> 8) & 255,
                    32, 0x28])
    with path.open("wb") as out:
        out.write(header)
        for r, g, b, a in img:
            out.write(bytes([b, g, r, a]))


def generate(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    files = []
    for i, name in enumerate(NAMES):
        canvas = blank()
        box_border(canvas)
        glyph(canvas, i)
        filename = dest / ("PH_" + name + ".TGA")
        write_tga(filename, canvas)
        files.append(filename)
        target = blank()
        glyph(target, i, target=True)
        filename = dest / ("PH_TARGET_" + name + ".TGA")
        write_tga(filename, target)
        files.append(filename)
    active = blank()
    selected_border(active)
    write_tga(dest / "PH_SELECTED.TGA", active)
    files.append(dest / "PH_SELECTED.TGA")
    write_tga(dest / "PH_RETICLE.TGA", crosshair())
    files.append(dest / "PH_RETICLE.TGA")
    return files


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Generate Parry Hotter HUD textures")
    parser.add_argument(
        "--out", type=Path,
        default=Path(r"C:\Programy\Steam\steamapps\common\Gothic\_work\Data\Textures"),
    )
    args = parser.parse_args()
    files = generate(args.out)
    print(f"Generated {len(files)} original Gothic TGA textures in {args.out}")


if __name__ == "__main__":
    main()
