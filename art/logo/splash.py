# Génère resources/splash.svg : emblème manga "Re:" en chemins uniquement (NanoSVG ne gère pas <text>)
import math, random, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
font = TTFont(sys.argv[1] if len(sys.argv) > 1 else '../../resources/Rubik-Regular.ttf')
gs = font.getGlyphSet(); cmap = font.getBestCmap(); upm = font['head'].unitsPerEm
def text_path(s, x, y, size, skew=-0.21, spacing=0, anchor='middle'):
    k = size / upm
    widths = [gs[cmap[ord(c)]].width * k + spacing for c in s]
    total = sum(widths) - spacing
    cx = x - (total / 2 if anchor == 'middle' else 0)
    pen = SVGPathPen(gs)
    for c, w in zip(s, widths):
        # x' = cx + k*gx - skew*k*gy ; y' = y - k*gy
        tp = TransformPen(pen, (k, 0, -skew * k, -k, cx, y))
        gs[cmap[ord(c)]].draw(tp)
        cx += w
    return pen.getCommands()
INK, RED, WHITE, GOLD = '#141414', '#e8253f', '#ffffff', '#ffc83d'
cx, cy = 360, 300
random.seed(7)
pts = []
n = 26
for i in range(n * 2):
    a = math.pi * 2 * i / (n * 2)
    r = random.uniform(205, 235) if i % 2 == 0 else random.uniform(150, 168)
    pts.append(f'{cx + r * 1.32 * math.cos(a):.1f},{cy + r * math.sin(a):.1f}')
re_d = text_path('Re:', cx, cy + 80, 230)
ban_d = text_path('EMULATIONSTATION', 360, 592, 50, spacing=3)
o = ['<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 720 720">']
o.append(f'<polygon points="{" ".join(pts)}" fill="{WHITE}" stroke="{INK}" stroke-width="10" stroke-linejoin="round"/>')
# ombre décalée, contour, remplissage (léger contour rouge pour épaissir la graisse)
o.append(f'<path transform="translate(12 12)" d="{re_d}" fill="{INK}" stroke="{INK}" stroke-width="26" stroke-linejoin="round"/>')
o.append(f'<path d="{re_d}" fill="{INK}" stroke="{INK}" stroke-width="22" stroke-linejoin="round"/>')
o.append(f'<path d="{re_d}" fill="{RED}" stroke="{RED}" stroke-width="5" stroke-linejoin="round"/>')
# bandeau penché
def para(x, y, w, h, s):  # parallélogramme incliné vers la droite
    d = h * s
    return f'M {x + d:.1f} {y} H {x + w + d:.1f} L {x + w:.1f} {y + h} H {x} Z'
o.append(f'<path d="{para(52, 540, 620, 76, 0.25)}" fill="{RED}" transform="translate(9 9)"/>')
o.append(f'<path d="{para(52, 540, 620, 76, 0.25)}" fill="{INK}"/>')
o.append(f'<path d="{ban_d}" fill="#f6f1e7" stroke="#f6f1e7" stroke-width="2.5" stroke-linejoin="round"/>')
# pixels
for (x, y, s, c, r) in [(70, 120, 22, RED, -15), (640, 90, 26, GOLD, 18), (95, 470, 18, GOLD, 10), (628, 455, 20, RED, -20), (180, 40, 16, INK, 12), (540, 30, 18, INK, -8)]:
    o.append(f'<rect x="{x}" y="{y}" width="{s}" height="{s}" fill="{c}" stroke="{INK}" stroke-width="5" transform="rotate({r} {x} {y})"/>')
o.append('</svg>')
open('../../resources/splash.svg', 'w').write('\n'.join(o))
