# Génère banner.svg : bannière 2400x600 pour le README, à partir de l'icône
# splash (resources/splash.svg) et de la palette du thème Mono.
# Rendu : rsvg-convert -o ../../docs/es-re-banner.png banner.svg
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE / '../..'
sys.path.insert(0, str(HERE))
from letters import word
from textpath import text_path

BG, AMBER, WHITE, DIM = '#0A0C10', '#F2B84B', '#F2F4F8', '#9AA3AE'
FONT = (ROOT / 'resources/ubuntu_condensed.ttf').resolve()
SPLASH = (ROOT / 'resources/splash.svg').resolve()
OUT = HERE / 'banner.svg'
W, H = 2400, 600


def build():
    if not SPLASH.is_file():
        sys.exit(f'{SPLASH} is missing: run splash.py first')

    splash = SPLASH.read_text(encoding='utf-8')
    icon = splash[splash.index('>') + 1:splash.rindex('</svg>')]

    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">']
    o.append(f'''<defs>
<radialGradient id="glow" cx="48%" cy="50%" r="58%"><stop offset="0%" stop-color="{AMBER}" stop-opacity="0.15"/><stop offset="55%" stop-color="{AMBER}" stop-opacity="0.04"/><stop offset="100%" stop-color="{AMBER}" stop-opacity="0"/></radialGradient>
<radialGradient id="vig" cx="50%" cy="50%" r="72%"><stop offset="55%" stop-color="#000000" stop-opacity="0"/><stop offset="100%" stop-color="#000000" stop-opacity="0.75"/></radialGradient>
<linearGradient id="rule" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stop-color="{AMBER}" stop-opacity="0.7"/><stop offset="100%" stop-color="{AMBER}" stop-opacity="0"/></linearGradient>
</defs>''')
    o.append(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
    o.append('<g stroke="#ffffff" stroke-opacity="0.03" stroke-width="1">')
    for x in range(0, W + 1, 80):
        o.append(f'<line x1="{x}" y1="0" x2="{x}" y2="{H}"/>')
    for y in range(0, H + 1, 80):
        o.append(f'<line x1="0" y1="{y}" x2="{W}" y2="{y}"/>')
    o.append('</g>')
    o.append('<rect width="{0}" height="{1}" fill="url(#glow)"/>'.format(W, H))
    o.append(f'<circle cx="1250" cy="300" r="380" fill="none" stroke="#ffffff" stroke-opacity="0.04" stroke-width="1.5"/>')
    o.append(f'<circle cx="1250" cy="300" r="455" fill="none" stroke="{AMBER}" stroke-opacity="0.06" stroke-width="1"/>')
    o.append(f'<rect width="{W}" height="{H}" fill="url(#vig)"/>')

    # Icône splash à gauche, à l'échelle 400/720.
    o.append(f'<g transform="translate(100 100) scale(0.5556)">{icon}</g>')
    o.append(f'<rect x="580" y="140" width="2" height="320" fill="url(#rule)" opacity="0.7"/>')

    # Lettrage ES-RE : ES blanc, tiret et RE ambre comme l'icône.
    size, base, gap, x0 = 250, 345, 28, 650
    d_es, w_es = word('ES', 0, base, size, gap=gap, anchor='start')
    d_da, w_da = word('-', 0, base, size, anchor='start')
    d_re, w_re = word('RE', 0, base, size, gap=gap, anchor='start')
    x_da = x0 + w_es + gap
    x_re = x_da + w_da + gap
    o.append(f'<path d="{d_es}" fill="{WHITE}" fill-rule="evenodd" stroke="{WHITE}" stroke-width="3" stroke-linejoin="round" transform="translate({x0:.1f} 0)"/>')
    o.append(f'<path d="{d_da}" fill="{AMBER}" transform="translate({x_da:.1f} 0)"/>')
    o.append(f'<path d="{d_re}" fill="{AMBER}" fill-rule="evenodd" stroke="{AMBER}" stroke-width="3" stroke-linejoin="round" transform="translate({x_re:.1f} 0)"/>')

    # Sous-titre.
    ss, sp, dash_w, gap2 = 42, 9, 17, 13
    d1, w1 = text_path(str(FONT), 'EMULATIONSTATION', 0, 0, ss, spacing=sp, anchor='start')
    d2, w2 = text_path(str(FONT), 'RE', 0, 0, ss, spacing=sp, anchor='start')
    sx = x0
    o.append(f'<path d="{d1}" fill="{DIM}" transform="translate({sx:.1f} 470)"/>')
    o.append(f'<rect x="{sx+w1+gap2:.1f}" y="450" width="{dash_w}" height="2.5" fill="{AMBER}"/>')
    o.append(f'<path d="{d2}" fill="{AMBER}" transform="translate({sx+w1+gap2+dash_w+gap2:.1f} 470)"/>')

    # Libellés.
    d, _ = text_path(str(FONT), 'MONO / ES-RE', 0, 0, 18, spacing=6, anchor='start')
    o.append(f'<path d="{d}" fill="{AMBER}" transform="translate({x0} 540)"/>')
    d, tw = text_path(str(FONT), 'SYSTEM FRONTEND', 0, 0, 16, spacing=5, anchor='start')
    o.append(f'<path d="{d}" fill="{DIM}" transform="translate({2300-tw:.1f} 540)"/>')
    o.append('</svg>')
    return '\n'.join(o)


OUT.write_text(build(), encoding='utf-8')
print(f'wrote {OUT}')
