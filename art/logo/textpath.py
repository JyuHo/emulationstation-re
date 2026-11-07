from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
def text_path(fontfile, s, x, y, size, skew=0.0, spacing=0, anchor='middle'):
    f=TTFont(fontfile); gs=f.getGlyphSet(); cmap=f.getBestCmap(); upm=f['head'].unitsPerEm
    k=size/upm; ws=[gs[cmap[ord(c)]].width*k+spacing for c in s]; tot=sum(ws)-spacing
    cx=x-(tot/2 if anchor=='middle' else 0); pen=SVGPathPen(gs); spans=[]
    for c,w in zip(s,ws):
        gs[cmap[ord(c)]].draw(TransformPen(pen,(k,0,skew*k,-k,cx,y))); spans.append(cx); cx+=w
    return pen.getCommands(), tot
