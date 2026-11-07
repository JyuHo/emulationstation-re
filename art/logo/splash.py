# Génère resources/splash.svg : icône néon « ES / RE » 720x720.
# NanoSVG (le moteur SVG d'ES) ignore <text> et les filtres : tout est en chemins,
# et le halo néon est simulé par des traits superposés de plus en plus fins.
import math, sys
from letters import word
OUT = sys.argv[1] if len(sys.argv) > 1 else '../../resources/splash.svg'
CYAN, PALE, WHITE = '#2fd0ff', '#e6fbff', '#f4f8ff'
def glow(d, color, width, extra=''):
    out = []
    for w, op in [(width + 34, 0.08), (width + 22, 0.12), (width + 12, 0.2), (width + 5, 0.4)]:
        out.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-opacity="{op}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"{extra}/>')
    out.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"{extra}/>')
    return out
o = ['<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 720 720">']
X0, Y0, X1, Y1, c = 40, 40, 680, 680, 90
plate = f'M {X0+c} {Y0} H {X1-c} L {X1} {Y0+c} V {Y1-c} L {X1-c} {Y1} H {X0+c} L {X0} {Y1-c} V {Y0+c} Z'
o.append(f'<path d="{plate}" fill="#03101f"/>')
o += glow(plate, CYAN, 12)
# anneau « recommencer »
cx, cy, r = 360, 365, 255
a0, a1 = math.radians(40), math.radians(318)
p0 = (cx + r*math.cos(a0), cy + r*math.sin(a0)); p1 = (cx + r*math.cos(a1), cy + r*math.sin(a1))
arc = f'M {p0[0]:.1f} {p0[1]:.1f} A {r} {r} 0 1 1 {p1[0]:.1f} {p1[1]:.1f}'
o += glow(arc, CYAN, 30)
t = (-math.sin(a1), math.cos(a1)); n = (math.cos(a1), math.sin(a1))
tip = (p1[0]+t[0]*80, p1[1]+t[1]*80)
b1 = (p1[0]+n[0]*48, p1[1]+n[1]*48); b2 = (p1[0]-n[0]*48, p1[1]-n[1]*48)
o.append(f'<path d="M {tip[0]:.1f} {tip[1]:.1f} L {b1[0]:.1f} {b1[1]:.1f} L {b2[0]:.1f} {b2[1]:.1f} Z" fill="{PALE}" stroke="{PALE}" stroke-width="8" stroke-linejoin="round"/>')
# lettres empilées
for txt, base in [('ES', 335), ('RE', 535)]:
    d, _ = word(txt, 360, base, 160, gap=24)
    o.append(f'<path transform="translate(0 8)" d="{d}" fill="#000814" fill-opacity="0.6" stroke="#000814" stroke-opacity="0.6" stroke-width="10" stroke-linejoin="round" fill-rule="evenodd"/>')
    o.append(f'<path d="{d}" fill="{WHITE}" fill-rule="evenodd" stroke="{WHITE}" stroke-width="7" stroke-linejoin="round"/>')
o.append('</svg>')
open(OUT, 'w').write('\n'.join(o))
