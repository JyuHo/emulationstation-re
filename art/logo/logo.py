# Génère logo.svg (splash de démarrage 1920x1080, néon bleu « ES-RE »). Rendu PNG : node render.js logo
import math
from letters import word
o=['<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="1080" viewBox="0 0 1920 1080">',
'''<defs>
<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#03142b"/><stop offset="0.55" stop-color="#062a52"/><stop offset="1" stop-color="#021022"/></linearGradient>
<linearGradient id="arc" x1="0" y1="1" x2="1" y2="0"><stop offset="0" stop-color="#00d8ff"/><stop offset="1" stop-color="#e6fbff"/></linearGradient>
<filter id="glow" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="9" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
<filter id="sh" x="-10%" y="-10%" width="120%" height="130%"><feDropShadow dx="0" dy="8" stdDeviation="10" flood-color="#000814" flood-opacity="0.7"/></filter>
</defs>
<rect width="1920" height="1080" fill="#02060d"/>''']
# bandes diagonales
for x,w,op in []:
    o.append(f'<polygon points="{x},1080 {x+w},1080 {x+w+1080},0 {x+1080},0" fill="#4fc3ff" opacity="{op}"/>')
# plaque à angles coupés
X0,Y0,X1,Y1,c=300,250,1620,690,60
plate=f'M {X0+c} {Y0} H {X1-c} L {X1} {Y0+c} V {Y1-c} L {X1-c} {Y1} H {X0+c} L {X0} {Y1-c} V {Y0+c} Z'
o.append(f'<path d="{plate}" fill="#03101f" stroke="#2fd0ff" stroke-width="10" stroke-linejoin="round" filter="url(#glow)"/>')
# arc « recommencer » autour de RE
cx,cy,r=1318,455,262
a0,a1=math.radians(40),math.radians(318)
p0=(cx+r*math.cos(a0),cy+r*math.sin(a0)); p1=(cx+r*math.cos(a1),cy+r*math.sin(a1))
o.append(f'<path d="M {p0[0]:.1f} {p0[1]:.1f} A {r} {r} 0 1 1 {p1[0]:.1f} {p1[1]:.1f}" fill="none" stroke="url(#arc)" stroke-width="34" stroke-linecap="round" filter="url(#glow)"/>')
# pointe de flèche au bout (angle a1 = 60° → tangente sens horaire)
t=(-math.sin(a1),math.cos(a1)); n=(math.cos(a1),math.sin(a1))
tip=(p1[0]+t[0]*86,p1[1]+t[1]*80); b1=(p1[0]+n[0]*52-t[0]*6,p1[1]+n[1]*52-t[1]*6); b2=(p1[0]-n[0]*52-t[0]*6,p1[1]-n[1]*52-t[1]*6)
o.append(f'<polygon points="{tip[0]:.1f},{tip[1]:.1f} {b1[0]:.1f},{b1[1]:.1f} {b2[0]:.1f},{b2[1]:.1f}" fill="#e6fbff" stroke="#e6fbff" stroke-width="8" stroke-linejoin="round" filter="url(#glow)"/>')
d,_=word('ES-RE',960,580,250,gap=26)
o.append(f'<path d="{d}" fill="#f4f8ff" fill-rule="evenodd" stroke="#f4f8ff" stroke-width="8" stroke-linejoin="round" filter="url(#sh)"/>')
o.append('<g fill="#cfe8ff"><rect x="300" y="822" width="230" height="4" rx="2" opacity="0.6"/><rect x="1390" y="822" width="230" height="4" rx="2" opacity="0.6"/></g>')
import os
from textpath import text_path
FONT=os.environ.get('ORBITRON','fonts/Orbitron-Black.ttf')
d1,w1=text_path(FONT,'EmulationStation-',0,0,64,skew=0.2)
d2,w2=text_path(FONT,'RE',0,0,64,skew=0.2)
x0=960-(w1+w2)/2
d1,_=text_path(FONT,'EmulationStation-',x0,850,64,skew=0.2,anchor='start')
d2,_=text_path(FONT,'RE',x0+w1,850,64,skew=0.2,anchor='start')
o.append(f'<path d="{d1}" fill="#f4f8ff"/><path d="{d2}" fill="#2fb8ff"/>')
o.append('</svg>')
open('logo.svg','w').write('\n'.join(o))
