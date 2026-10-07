#!/usr/bin/env python3
"""Draws PadBridge PS5's icon: assets/icon0.png (the media-row launcher icon,
embedded in the ELF as src/icon_data.c by make) and assets/logo.png (README).

An original design: a dark rounded tile, a gamepad silhouette hanging from a
neon suspension-bridge arc. Needs cairosvg and Pillow
(python3 -m venv v && v/bin/pip install cairosvg pillow && v/bin/python tools/make-icon.py),
then check with tools/check-icon.py and run make to refresh src/icon_data.c.
"""
import io
import os

import cairosvg
from PIL import Image, ImageChops, ImageFilter

S = 512
CYAN, BLUE, DEEP = "#00E5FF", "#2F7BFF", "#7A5CFF"
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")

DEFS = f"""
<defs>
  <linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#0E1A2E"/><stop offset="1" stop-color="#060A14"/>
  </linearGradient>
  <radialGradient id="halo" cx="0.5" cy="0.30" r="0.62">
    <stop offset="0" stop-color="{BLUE}" stop-opacity="0.30"/>
    <stop offset="1" stop-color="{BLUE}" stop-opacity="0"/>
  </radialGradient>
  <linearGradient id="neon" gradientUnits="userSpaceOnUse" x1="40" y1="0" x2="472" y2="0">
    <stop offset="0" stop-color="{CYAN}"/><stop offset="0.55" stop-color="{BLUE}"/>
    <stop offset="1" stop-color="{DEEP}"/>
  </linearGradient>
  <linearGradient id="body" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#16263F"/><stop offset="1" stop-color="#0A1222"/>
  </linearGradient>
</defs>"""

PAD = ("M176 286 H336 C378 286 402 306 412 344 L430 410 C438 444 408 462 384 444 "
       "L352 414 C344 406 336 402 324 402 H188 C176 402 168 406 160 414 L128 444 "
       "C104 462 74 444 82 410 L100 344 C110 306 134 286 176 286 Z")


def arc_y(x):
    """y of the arc M64 300 Q256 70 448 300 at x (x is linear in t)."""
    t = (x - 64) / 384
    return 300 * (1 - t) ** 2 + 2 * 70 * t * (1 - t) + 300 * t * t


def neon_parts(w):
    cables = "".join(
        f'<line x1="{x}" y1="{arc_y(x):.1f}" x2="{x}" y2="286" stroke="url(#neon)" '
        f'stroke-width="{w * 0.45:.1f}" stroke-linecap="round" opacity="0.75"/>'
        for x in (176, 216, 256, 296, 336))
    return (
        f'<path d="M64 300 Q256 70 448 300" fill="none" stroke="url(#neon)" '
        f'stroke-width="{w * 1.3:.1f}" stroke-linecap="round"/>' + cables +
        f'<path d="{PAD}" fill="none" stroke="url(#neon)" stroke-width="{w:.1f}" stroke-linejoin="round"/>'
        f'<circle cx="64" cy="300" r="13" fill="{CYAN}"/><circle cx="448" cy="300" r="13" fill="{DEEP}"/>'
        # d-pad and face buttons
        f'<path d="M168 334 h16 v-16 h12 v16 h16 v12 h-16 v16 h-12 v-16 h-16 Z" fill="{CYAN}"/>'
        f'<circle cx="330" cy="326" r="8" fill="{BLUE}"/><circle cx="330" cy="358" r="8" fill="{BLUE}"/>'
        f'<circle cx="314" cy="342" r="8" fill="{BLUE}"/><circle cx="346" cy="342" r="8" fill="{DEEP}"/>')


def svg(inner, with_tile, art=True):
    if art:   # the drawing sits a little above the tile's centre
        inner = f'<g transform="translate(0 -22)">{inner}</g>'
    tile = ('<rect x="24" y="24" width="464" height="464" rx="108" fill="url(#tile)"/>'
            '<rect x="24" y="24" width="464" height="464" rx="108" fill="url(#halo)"/>') if with_tile else ""
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{S}" height="{S}" viewBox="0 0 {S} {S}">{DEFS}{tile}{inner}</svg>'


def render(doc):
    return Image.open(io.BytesIO(cairosvg.svg2png(bytestring=doc.encode(), output_width=S, output_height=S))).convert("RGBA")


def main():
    base = render(svg(
        f'<path d="{PAD}" fill="url(#body)"/>'
        '<circle cx="222" cy="378" r="17" fill="#060A14" stroke="#2A4466" stroke-width="4"/>'
        '<circle cx="290" cy="378" r="17" fill="#060A14" stroke="#2A4466" stroke-width="4"/>', True))
    glow = render(svg(neon_parts(14), False)).filter(ImageFilter.GaussianBlur(10))
    lines = render(svg(neon_parts(6), False))
    rim = render(svg('<rect x="27" y="27" width="458" height="458" rx="105" fill="none" '
                     'stroke="url(#neon)" stroke-width="5" opacity="0.9"/>', False, False))
    img = base
    for layer in (glow, glow, lines, rim):
        img = Image.alpha_composite(img, layer)
    mask = render(svg('<rect x="24" y="24" width="464" height="464" rx="108" fill="#fff"/>', False, False)).getchannel("A")
    img.putalpha(ImageChops.multiply(img.getchannel("A"), mask))
    # Fully transparent pixels stored black: a renderer that drops alpha shows black.
    black = Image.new("RGBA", img.size, (0, 0, 0, 0))
    img = Image.composite(img, black, img.getchannel("A").point(lambda a: 255 if a else 0))
    for name in ("icon0.png", "logo.png"):
        img.save(os.path.join(ROOT, name), optimize=True)
    print("wrote assets/icon0.png and assets/logo.png")


if __name__ == "__main__":
    main()
