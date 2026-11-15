# Génère resources/splash.svg : icône 720x720, palette du thème Mono.
# NanoSVG (le moteur SVG d'ES) ignore <text> et les filtres : tout est en
# chemins, et le halo est simulé par des traits superposés.
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from letters import word

BG, AMBER, WHITE = '#0A0C10', '#F2B84B', '#F2F4F8'
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / '../../resources/splash.svg'


def cut_rect(x0, y0, x1, y1, c):
    return f'M {x0+c} {y0} H {x1-c} L {x1} {y0+c} V {y1-c} L {x1-c} {y1} H {x0+c} L {x0} {y1-c} V {y0+c} Z'


def grid(step, opacity, x0, y0, x1, y1):
    p = [f'<g stroke="#ffffff" stroke-opacity="{opacity}" stroke-width="1">']
    x = x0
    while x <= x1:
        p.append(f'<line x1="{x}" y1="{y0}" x2="{x}" y2="{y1}"/>')
        x += step
    y = y0
    while y <= y1:
        p.append(f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}"/>')
        y += step
    p.append('</g>')
    return '\n'.join(p)


def build():
    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 720 720">']
    plate = cut_rect(40, 40, 680, 680, 90)

    # Halo ambré, sans filtre.
    for width, opacity in [(22, 0.05), (14, 0.09), (8, 0.16)]:
        o.append(f'<path d="{plate}" fill="none" stroke="{AMBER}" stroke-opacity="{opacity}" stroke-width="{width}" stroke-linejoin="round"/>')
    o.append(f'<path d="{plate}" fill="{BG}" stroke="{AMBER}" stroke-width="3" stroke-linejoin="round"/>')
    inner = cut_rect(52, 52, 668, 668, 80)
    o.append(f'<path d="{inner}" fill="none" stroke="#ffffff" stroke-opacity="0.10" stroke-width="1"/>')
    o.append(grid(40, 0.03, 110, 110, 610, 610))
    o.append(f'<circle cx="360" cy="365" r="286" fill="none" stroke="#ffffff" stroke-opacity="0.05" stroke-width="1"/>')

    # Arc « recommencer ».
    cx, cy, r = 360, 365, 250
    a0, a1 = math.radians(40), math.radians(318)
    p0 = (cx + r*math.cos(a0), cy + r*math.sin(a0))
    p1 = (cx + r*math.cos(a1), cy + r*math.sin(a1))
    o.append(f'<path d="M {p0[0]:.1f} {p0[1]:.1f} A {r} {r} 0 1 1 {p1[0]:.1f} {p1[1]:.1f}" fill="none" stroke="{AMBER}" stroke-opacity="0.8" stroke-width="6" stroke-linecap="round"/>')
    t = (-math.sin(a1), math.cos(a1))
    n = (math.cos(a1), math.sin(a1))
    tip = (p1[0] + t[0]*30, p1[1] + t[1]*30)
    b1 = (p1[0] + n[0]*17 - t[0]*2, p1[1] + n[1]*17 - t[1]*2)
    b2 = (p1[0] - n[0]*17 - t[0]*2, p1[1] - n[1]*17 - t[1]*2)
    o.append(f'<path d="M {tip[0]:.1f} {tip[1]:.1f} L {b1[0]:.1f} {b1[1]:.1f} L {b2[0]:.1f} {b2[1]:.1f} Z" fill="{AMBER}"/>')

    # ES blanc, RE ambre : le monogramme reste lisible en petite taille.
    for text, base, color in [('ES', 330, WHITE), ('RE', 535, AMBER)]:
        d, _ = word(text, 360, base, 160, gap=24)
        o.append(f'<path d="{d}" fill="{color}" fill-rule="evenodd" stroke="{color}" stroke-width="3" stroke-linejoin="round"/>')

    o.append('</svg>')
    return '\n'.join(o)


OUT.write_text(build(), encoding='utf-8')
print(f'wrote {OUT}')
