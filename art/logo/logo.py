# Génère logo.svg : splash de démarrage 1920x1080, palette du thème Mono.
# Rendu PNG : rsvg-convert -o ../../resources/logo.png logo.svg
# (ou node render.js avec Puppeteer).
import math
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from letters import word
from textpath import text_path

BG, PANEL = '#0A0C10', '#0D1117'
AMBER, WHITE, DIM = '#F2B84B', '#F2F4F8', '#9AA3AE'
FONT = Path(os.environ.get('UBUNTU_CONDENSED', HERE / '../../resources/ubuntu_condensed.ttf')).resolve()
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / 'logo.svg'


def cut_rect(x0, y0, x1, y1, c):
    return f'M {x0+c} {y0} H {x1-c} L {x1} {y0+c} V {y1-c} L {x1-c} {y1} H {x0+c} L {x0} {y1-c} V {y0+c} Z'


def grid(w, h, step, opacity):
    p = [f'<g stroke="#ffffff" stroke-opacity="{opacity}" stroke-width="1">']
    x = 0
    while x <= w:
        p.append(f'<line x1="{x}" y1="0" x2="{x}" y2="{h}"/>')
        x += step
    y = 0
    while y <= h:
        p.append(f'<line x1="0" y1="{y}" x2="{w}" y2="{y}"/>')
        y += step
    p.append('</g>')
    return '\n'.join(p)


def build():
    w, h = 1920, 1080
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">']
    o.append(f'''<defs>
<radialGradient id="glow" cx="50%" cy="43%" r="52%"><stop offset="0%" stop-color="{AMBER}" stop-opacity="0.16"/><stop offset="45%" stop-color="{AMBER}" stop-opacity="0.05"/><stop offset="100%" stop-color="{AMBER}" stop-opacity="0"/></radialGradient>
<radialGradient id="vig" cx="50%" cy="45%" r="72%"><stop offset="50%" stop-color="#000000" stop-opacity="0"/><stop offset="100%" stop-color="#000000" stop-opacity="0.82"/></radialGradient>
<linearGradient id="rule" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="{AMBER}" stop-opacity="0"/><stop offset="50%" stop-color="{AMBER}" stop-opacity="0.75"/><stop offset="100%" stop-color="{AMBER}" stop-opacity="0"/></linearGradient>
</defs>''')
    o.append(f'<rect width="{w}" height="{h}" fill="{BG}"/>')
    o.append(grid(w, h, 80, 0.03))
    o.append(f'<rect width="{w}" height="{h}" fill="url(#glow)"/>')
    o.append(f'<circle cx="960" cy="480" r="420" fill="none" stroke="#ffffff" stroke-opacity="0.045" stroke-width="1.5"/>')
    o.append(f'<circle cx="960" cy="480" r="505" fill="none" stroke="{AMBER}" stroke-opacity="0.07" stroke-width="1"/>')
    o.append(f'<rect width="{w}" height="{h}" fill="url(#vig)"/>')

    # Panneau à angles coupés, comme les cartes du thème.
    frame = cut_rect(280, 240, 1640, 840, 56)
    o.append(f'<path d="{frame}" fill="{PANEL}" fill-opacity="0.55" stroke="#ffffff" stroke-opacity="0.14" stroke-width="1.5"/>')
    o.append(f'<rect x="316" y="256" width="58" height="3" rx="1.5" fill="{AMBER}" fill-opacity="0.9"/>')

    # Libellés supérieurs.
    d, _ = text_path(str(FONT), 'MONO', 0, 0, 19, spacing=6, anchor='start')
    o.append(f'<path d="{d}" fill="{AMBER}" transform="translate(316 198)"/>')
    o.append(f'<rect x="296" y="184" width="7" height="7" fill="{AMBER}"/>')
    d, tw = text_path(str(FONT), 'DEFAULT THEME', 0, 0, 16, spacing=5, anchor='start')
    o.append(f'<path d="{d}" fill="{DIM}" transform="translate({1604-tw:.1f} 198)"/>')

    # Arc « recommencer » derrière RE.
    cx, cy, r = 1282, 480, 220
    a0, a1 = math.radians(40), math.radians(318)
    p0 = (cx + r*math.cos(a0), cy + r*math.sin(a0))
    p1 = (cx + r*math.cos(a1), cy + r*math.sin(a1))
    o.append(f'<path d="M {p0[0]:.1f} {p0[1]:.1f} A {r} {r} 0 1 1 {p1[0]:.1f} {p1[1]:.1f}" fill="none" stroke="{AMBER}" stroke-opacity="0.75" stroke-width="6" stroke-linecap="round"/>')
    t = (-math.sin(a1), math.cos(a1))
    n = (math.cos(a1), math.sin(a1))
    tip = (p1[0] + t[0]*26, p1[1] + t[1]*26)
    b1 = (p1[0] + n[0]*15 - t[0]*2, p1[1] + n[1]*15 - t[1]*2)
    b2 = (p1[0] - n[0]*15 - t[0]*2, p1[1] - n[1]*15 - t[1]*2)
    o.append(f'<path d="M {tip[0]:.1f} {tip[1]:.1f} L {b1[0]:.1f} {b1[1]:.1f} L {b2[0]:.1f} {b2[1]:.1f} Z" fill="{AMBER}"/>')

    # Lettrage ES-RE.
    size, base, gap = 240, 600, 28
    d_es, w_es = word('ES', 0, base, size, gap=gap, anchor='start')
    d_da, w_da = word('-', 0, base, size, anchor='start')
    d_re, w_re = word('RE', 0, base, size, gap=gap, anchor='start')
    total = w_es + gap + w_da + gap + w_re
    x0 = 960 - total/2
    x_es = x0
    x_da = x0 + w_es + gap
    x_re = x_da + w_da + gap
    o.append(f'<path d="{d_es}" fill="{WHITE}" fill-rule="evenodd" stroke="{WHITE}" stroke-width="3" stroke-linejoin="round" transform="translate({x_es:.1f} 0)"/>')
    o.append(f'<path d="{d_da}" fill="{WHITE}" stroke="{WHITE}" stroke-width="2" transform="translate({x_da:.1f} 0)"/>')
    o.append(f'<path d="{d_re}" fill="{WHITE}" fill-rule="evenodd" stroke="{WHITE}" stroke-width="3" stroke-linejoin="round" transform="translate({x_re:.1f} 0)"/>')

    # Filet ambré et sous-titre.
    o.append(f'<rect x="510" y="688" width="900" height="2" fill="url(#rule)"/>')
    ss, sp, dash_w, gap2 = 44, 9, 18, 14
    d1, w1 = text_path(str(FONT), 'EMULATIONSTATION', 0, 0, ss, spacing=sp, anchor='start')
    d2, w2 = text_path(str(FONT), 'RE', 0, 0, ss, spacing=sp, anchor='start')
    tot = w1 + gap2 + dash_w + gap2 + w2
    sx = 960 - tot/2
    o.append(f'<path d="{d1}" fill="{DIM}" transform="translate({sx:.1f} 762)"/>')
    o.append(f'<rect x="{sx+w1+gap2:.1f}" y="740" width="{dash_w}" height="2.5" fill="{AMBER}"/>')
    o.append(f'<path d="{d2}" fill="{AMBER}" transform="translate({sx+w1+gap2+dash_w+gap2:.1f} 762)"/>')

    o.append('</svg>')
    return '\n'.join(o)


OUT.write_text(build(), encoding='utf-8')
print(f'wrote {OUT}')
