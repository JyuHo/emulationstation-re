# Logo et splash d'EmulationStation-RE

Assets de la marque, dans la palette du thème Mono : fond charbon, grille
fine, accent ambré et lettrage monochrome.

| Fichier généré | Source | Commande (depuis ce dossier) |
| --- | --- | --- |
| `resources/logo.png` (1920×1080, splash de démarrage) | `logo.py` → `logo.svg` | `python3 logo.py && rsvg-convert -o ../../resources/logo.png logo.svg` |
| `resources/splash.svg` (icône 720×720) | `splash.py` | `python3 splash.py` |
| `docs/es-re-banner.png` (2400×600, bannière du README) | `banner.py` → `banner.svg` | `python3 banner.py && rsvg-convert -o ../../docs/es-re-banner.png banner.svg` |

- Les lettres « ES-RE » (E, S, R, e, tiret, deux-points) sont dessinées à la main dans
  `letters.py` : ce sont des polygones, aucune police n'est nécessaire.
- Le sous-titre « EMULATIONSTATION-RE » est en Ubuntu Condensed, la police des titres
  du thème, déjà livrée dans `resources/ubuntu_condensed.ttf`. `textpath.py` la convertit
  en chemins avec fontTools (`python3 -m pip install fonttools`). `logo.svg` est versionné
  tout prêt, donc le rendu PNG ne demande ni police ni fontTools.
- `render.js` reste une alternative Puppeteer pour rendre `logo.svg` en PNG.
- `splash.svg` est lu par NanoSVG, qui ignore `<text>`, les filtres et les dégradés sur les
  traits. Il n'utilise donc que des chemins pleins, et le halo est simulé par des traits
  superposés semi-transparents.
- `banner.py` reprend l'icône de `resources/splash.svg` et le lettrage de `letters.py` pour la
  bannière du README ; lancer `splash.py` d'abord si l'icône change.

Palette : `#0A0C10` (fond), `#F2B84B` (ambre), `#F2F4F8` (blanc), `#9AA3AE` (texte secondaire).
