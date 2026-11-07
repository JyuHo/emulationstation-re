# Lettrage maison « ES-RE » : capitales larges à angles coupés à 45°, dessinées en polygones.
W, H, T, C = 100, 120, 27, 22   # largeur, hauteur, épaisseur, chanfrein
M = H / 2
def E():
    return [[(C,0),(W,0),(W-T,T),(T,T),(T,M-T/2),(W-12,M-T/2),(W-12,M+T/2),(T,M+T/2),(T,H-T),(W-T,H-T),(W,H),(C,H),(0,H-C),(0,C)]]
def S():
    return [[(C,0),(W,0),(W-T,T),(T,T),(T,M-T/2),(W-C,M-T/2),(W,M-T/2+C),(W,H-C),(W-C,H),(0,H),(T,H-T),(W-T,H-T),(W-T,M+T/2),(C,M+T/2),(0,M+T/2-C),(0,C)]]
def R():
    bowl=[(C,0),(W-C,0),(W,C),(W,M+T/2-C),(W-C,M+T/2),(T,M+T/2),(T,H),(0,H),(0,C)]
    hole=[(T,T),(W-T,T),(W-T,M-T/2),(T,M-T/2)]
    leg=[(T+10,M+T/2),(T+10+T*1.25,M+T/2),(W+6,H),(W+6-T*1.25,H)]
    return [bowl,hole,leg]
def DASH(): return [[(4,M-T/2),(62,M-T/2),(62,M+T/2),(4,M+T/2)]]
def e():   # minuscule, hauteur d'x = 0.78 H, fûts un peu plus fins
    t=T*0.82; c=C*0.85; x=H*0.22; m=x+t+16
    outer=[(c,x),(W-c,x),(W,x+c),(W,m+t),(t,m+t),(t,H-t),(W-4,H-t),(W-4-t,H),(c,H),(0,H-c),(0,x+c)]
    hole=[(t,x+t),(W-t,x+t),(W-t,m),(t,m)]
    return [outer,hole]
def COLON():
    s=T*1.05
    sq=lambda y:[(6,y),(6+s,y),(6+s,y+s),(6,y+s)]
    return [sq(H*0.32),sq(H-s)]
GLYPHS={'E':(E,W),'S':(S,W),'R':(R,W+6),'-':(DASH,66),'e':(e,W),':':(COLON,6+T*1.05+6)}
def word(txt, x, y, size, gap=16, skew=0.0, anchor='middle'):
    """Renvoie (d, largeur) ; size = hauteur des capitales en px, y = ligne de base."""
    k=size/H
    tot=sum(GLYPHS[c][1] for c in txt)+gap*(len(txt)-1)
    ox = x - (tot*k/2 if anchor=='middle' else 0)
    d=[]; cx=0
    for c in txt:
        fn,w=GLYPHS[c]
        for poly in fn():
            pts=[(ox+(cx+px)*k + skew*(H-py)*k, y-H*k+py*k) for px,py in poly]
            d.append('M '+' L '.join(f'{a:.1f} {b:.1f}' for a,b in pts)+' Z')
        cx+=w+gap
    return ' '.join(d), tot*k
