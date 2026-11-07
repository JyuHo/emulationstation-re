# Logo et splash d'EmulationStation-RE

Logo néon bleu « ES-RE », avec la flèche « recommencer ».

| Fichier généré | Source | Commande (depuis ce dossier) |
| --- | --- | --- |
| `resources/logo.png` (1920×1080, splash de démarrage) | `logo.py` → `logo.svg` | `python3 logo.py && node render.js && mv logo.png ../../resources/logo.png` |
| `resources/splash.svg` (icône 720×720) | `splash.py` | `python3 splash.py` |

- Les lettres « ES-RE » (E, S, R, e, tiret, deux-points) sont dessinées à la main dans
  `letters.py` : ce sont des polygones, aucune police n'est nécessaire.
- Le sous-titre « EmulationStation-RE » est en Orbitron (police libre, licence OFL), converti
  en chemins par `textpath.py`. Pour régénérer `logo.svg`, il faut `fonttools` et le fichier
  Orbitron Black de Google Fonts, dans `fonts/Orbitron-Black.ttf` ou à l'emplacement donné
  par la variable `ORBITRON`. `logo.svg` est versionné tout prêt, donc `render.js` n'a besoin
  d'aucune police.
- `splash.svg` est lu par NanoSVG, qui ignore `<text>`, les filtres et les dégradés sur les
  traits. Il n'utilise donc que des chemins pleins, et le halo néon y est simulé par des traits
  superposés semi-transparents.
