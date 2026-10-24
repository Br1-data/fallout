"""Genera le icone della versione web (web/icon-*.png): lo stesso disegno dell'icona del gioco."""
import math
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), "..", "web")


def icon(size, pad=0.0):
    s = size * 4  # disegno in alta risoluzione, poi riduzione (bordi morbidi)
    img = Image.new("RGBA", (s, s), (8, 7, 5, 255) if pad else (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = s / 2
    r = s / 2 * (1 - pad)

    def disc(rad, col):
        d.ellipse([c - rad, c - rad, c + rad, c + rad], fill=col)

    disc(r, (40, 34, 24, 255))
    disc(r * 0.88, (225, 180, 40, 255))
    disc(r * 0.76, (40, 70, 120, 255))
    w = max(2, int(s * 0.022))
    for k in range(3):
        a = k * math.pi / 3
        pts = []
        for i in range(0, 361, 3):
            t = math.radians(i)
            x, y = math.cos(t) * r * 0.62, math.sin(t) * r * 0.22
            pts.append((c + x * math.cos(a) - y * math.sin(a), c + x * math.sin(a) + y * math.cos(a)))
        d.line(pts, fill=(240, 230, 200, 255), width=w)
    disc(r * 0.15, (255, 90, 60, 255))
    return img.resize((size, size), Image.LANCZOS)


icon(192).save(os.path.join(OUT, "icon-192.png"))
icon(512).save(os.path.join(OUT, "icon-512.png"))
icon(180, pad=0.06).save(os.path.join(OUT, "apple-touch-icon.png"))  # iOS non usa la trasparenza
print("icone scritte in", os.path.abspath(OUT))
