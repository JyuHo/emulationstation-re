# Génère logo.svg (splash 1920x1080 style manga). Rendu PNG : node render.js logo
import math,random,sys
def gaming(dark,ink,paper,red):
  out=[]
  line = "#f6f1e7" if dark else "#141414"
  body = "#26262c" if dark else "#ffffff"
  gold="#ffc83d"
  sw='stroke="%s" stroke-width="10" stroke-linejoin="round" stroke-linecap="round"'%line
  # --- manette (gauche)
  out.append(f'''<g transform="translate(250 450) rotate(-14)">
   <path d="M -150 -62 Q -165 -88 -118 -88 H 118 Q 165 -88 150 -62 L 178 58 Q 190 112 142 110 Q 108 106 88 64 H -88 Q -108 106 -142 110 Q -190 112 -178 58 Z" fill="{body}" {sw}/>
   <path d="M -112 -36 h 24 v -24 h 24 v 24 h 24 v 24 h -24 v 24 h -24 v -24 h -24 z" fill="{line}" transform="translate(4 18)"/>
   <circle cx="92" cy="-40" r="16" fill="{red}" {sw.replace('10','6')}/>
   <circle cx="124" cy="-10" r="16" fill="{gold}" {sw.replace('10','6')}/>
   <circle cx="60" cy="-10" r="16" fill="{body}" {sw.replace('10','6')}/>
   <circle cx="92" cy="20" r="16" fill="{line}"/>
   <rect x="-34" y="-8" width="26" height="12" rx="6" fill="{line}"/><rect x="8" y="-8" width="26" height="12" rx="6" fill="{line}"/>
  </g>
  <g {sw.replace('10','8')} fill="none" transform="rotate(-14 250 450)">
   <path d="M 60 330 l -28 -22"/><path d="M 44 372 l -36 -6"/><path d="M 440 330 l 28 -22"/><path d="M 456 372 l 36 -6"/>
  </g>''')
  # --- coeurs de vie (haut gauche)
  heart=[".XX.XX.","XXXXXXX","XXXXXXX",".XXXXX.","..XXX..","...X..."]
  px=13
  def ph(ox,oy,fill):
    r=[]
    for j,row in enumerate(heart):
      for i,c in enumerate(row):
        if c=="X": r.append(f'<rect x="{ox+i*px-4}" y="{oy+j*px-4}" width="{px+8}" height="{px+8}" fill="{line}"/>')
    for j,row in enumerate(heart):
      for i,c in enumerate(row):
        if c=="X": r.append(f'<rect x="{ox+i*px}" y="{oy+j*px}" width="{px}" height="{px}" fill="{fill}"/>')
    return "".join(r)
  out.append(f'<g transform="rotate(-6 250 110)"><text x="70" y="148" font-family="Rubik" font-weight="700" font-style="italic" font-size="56" fill="{red}" stroke="{line}" stroke-width="8" paint-order="stroke" stroke-linejoin="round">HP</text>'+ph(160,96,red)+ph(270,96,red)+ph(380,96,body)+'</g>')
  # --- piece (droite)
  out.append(f'''<g transform="translate(1680 520) rotate(12)">
   <ellipse cx="0" cy="0" rx="62" ry="74" fill="{gold}" {sw}/>
   <ellipse cx="0" cy="0" rx="40" ry="52" fill="none" {sw.replace('10','6')}/>
   <rect x="-9" y="-30" width="18" height="60" rx="4" fill="{line}"/>
  </g>
  <g {sw.replace('10','7')} fill="none"><path d="M 1590 430 l -26 -20"/><path d="M 1574 500 l -34 -2"/><path d="M 1772 430 l 26 -20"/><path d="M 1786 500 l 34 -2"/></g>
  <text x="1770" y="660" font-family="Rubik" font-weight="700" font-style="italic" font-size="64" text-anchor="middle" fill="{gold}" stroke="{line}" stroke-width="10" paint-order="stroke" stroke-linejoin="round" transform="rotate(-8 1770 660)">1UP!</text>''')
  # --- cartouche (bas gauche)
  out.append(f'''<g transform="translate(250 870) rotate(-10)">
   <path d="M -80 -100 H 80 V -70 H 66 V -50 H 80 V 92 Q 80 100 72 100 H -72 Q -80 100 -80 92 Z" fill="{body}" {sw}/>
   <rect x="-56" y="-46" width="112" height="80" rx="6" fill="{red}" {sw.replace('10','6')}/>
   <text x="0" y="12" text-anchor="middle" font-family="Rubik" font-weight="700" font-style="italic" font-size="48" fill="#ffffff" stroke="#141414" stroke-width="6" paint-order="stroke">Re:</text>
   <g fill="{line}"><rect x="-56" y="54" width="14" height="30" rx="2"/><rect x="-32" y="54" width="14" height="30" rx="2"/><rect x="-8" y="54" width="14" height="30" rx="2"/><rect x="16" y="54" width="14" height="30" rx="2"/><rect x="40" y="54" width="14" height="30" rx="2"/></g>
  </g>''')
  # --- pixels qui volent autour de la bulle
  import random as R; R.seed(11)
  spots=[(520,140),(470,300),(1400,120),(1460,330),(560,640),(1380,640),(700,90),(1230,70)]
  for (x,y) in spots:
    sz=R.choice([18,24,30]); col=R.choice([red,gold,line])
    out.append(f'<rect x="{x}" y="{y}" width="{sz}" height="{sz}" fill="{col}" stroke="{line}" stroke-width="5" transform="rotate({R.randint(-25,25)} {x} {y})"/>')
  return "\n".join(out)

def build(dark):
  random.seed(7)
  paper = "#0d0d10" if dark else "#f6f1e7"
  ink   = "#f6f1e7" if dark else "#141414"
  red="#e8253f"
  cx,cy=960,430
  o=[f'<svg xmlns="http://www.w3.org/2000/svg" width="1920" height="1080" viewBox="0 0 1920 1080">',f'<rect width="1920" height="1080" fill="{paper}"/>']
  # halftone in corners
  dots=[]
  step=22
  for y in range(0,1081,step):
    for x in range(0,1921,step):
      xx=x+(step/2 if (y//step)%2 else 0)
      d=min(math.hypot(xx,y),math.hypot(1920-xx,1080-y))
      r=max(0,(1-d/700))*step*0.5
      if r>0.6: dots.append(f'<circle cx="{xx}" cy="{y}" r="{r:.1f}"/>')
  o.append(f'<g fill="{ink}" opacity="0.22">'+''.join(dots)+'</g>')
  # speed lines
  lines=[]
  a=0
  while a<360:
    w=random.uniform(0.25,1.1); rin=random.uniform(380,520); rout=1500
    a1=math.radians(a); a2=math.radians(a+w)
    tip=(cx+rin*math.cos((a1+a2)/2), cy+rin*math.sin((a1+a2)/2))
    p1=(cx+rout*math.cos(a1),cy+rout*math.sin(a1)); p2=(cx+rout*math.cos(a2),cy+rout*math.sin(a2))
    lines.append(f'<polygon points="{tip[0]:.1f},{tip[1]:.1f} {p1[0]:.1f},{p1[1]:.1f} {p2[0]:.1f},{p2[1]:.1f}"/>')
    a+=w+random.uniform(0.6,2.6)
  o.append(f'<g fill="{ink}" opacity="0.85">'+''.join(lines)+'</g>')
  # burst
  pts=[];n=34
  for i in range(n*2):
    ang=math.pi*2*i/(n*2)
    r=(random.uniform(300,350) if i%2==0 else random.uniform(230,255))
    pts.append(f'{cx+r*1.35*math.cos(ang):.1f},{cy+r*math.sin(ang):.1f}')
  burst_fill = red if dark else "#ffffff"
  o.append(f'<polygon points="{" ".join(pts)}" fill="{burst_fill}" stroke="{ink}" stroke-width="12" stroke-linejoin="round"/>')
  # Re: with offset shadow
  re_fill = "#ffffff" if dark else red
  sh = "#141414"
  o.append(f'''<g font-family="Rubik" font-weight="700" font-style="italic" text-anchor="middle" font-size="330">
  <text x="{cx+16}" y="{cy+125}" fill="{sh}" stroke="{sh}" stroke-width="30" stroke-linejoin="round">Re:</text>
  <text x="{cx}" y="{cy+110}" fill="{re_fill}" stroke="#141414" stroke-width="22" stroke-linejoin="round" paint-order="stroke">Re:</text>
</g>''')
  # banner
  o.append(f'''<g transform="translate(960 752) skewX(-14)">
  <rect x="-560" y="-68" width="1120" height="124" fill="{red if not dark else '#f6f1e7'}" transform="translate(12 12)"/>
  <rect x="-560" y="-68" width="1120" height="124" fill="#141414" stroke="{'#f6f1e7' if dark else '#141414'}" stroke-width="6"/>
  <text x="0" y="24" text-anchor="middle" font-family="Rubik" font-weight="700" font-size="82" letter-spacing="8" fill="#f6f1e7">EMULATIONSTATION</text>
</g>''')
  # tagline
  o.append(f'<text x="960" y="900" text-anchor="middle" font-family="DroidJP" font-size="44" letter-spacing="20" fill="{ink}">もう一度、遊ぼう！</text>')
  o.append(gaming(dark,ink,paper,red))
  # SFX
  o.append(f'''<g font-family="DroidJP" font-weight="700" text-anchor="middle" stroke="#141414" stroke-width="14" stroke-linejoin="round" paint-order="stroke">
  <text x="1540" y="250" font-size="120" fill="{red}" transform="rotate(14 1540 250)">ピコッ!</text>
  <text x="1640" y="1010" font-size="80" fill="{'#f6f1e7' if dark else '#ffffff'}" transform="rotate(-10 1640 1010)">ドン!</text>
</g>''')
  o.append('</svg>')
  return '\n'.join(o)
open('logo.svg','w').write(build(False))  # version papier retenue
open('logo-nuit.svg','w').write(build(True))  # variante sombre
