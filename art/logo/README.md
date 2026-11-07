# Logo et splash d'EmulationStation-RE

Sources du logo « Re: » au style manga (version papier).

| Fichier généré | Source | Commande |
| --- | --- | --- |
| `resources/logo.png` (1920×1080, splash de démarrage) | `logo.py` → `logo.svg` | `python3 logo.py && node render.js logo && cp logo.png ../../resources/logo.png` |
| `resources/splash.svg` (emblème 720×720) | `splash.py` | `python3 splash.py` (nécessite `fonttools`) |

- `logo.svg` utilise les polices Rubik et DroidSansFallbackFull de `resources/`, sous les noms
  `Rubik` et `DroidJP`. `render.js` les embarque avant le rendu ; ouvert ailleurs, le SVG
  s'affichera avec une police de repli.
- `splash.svg` est lu par NanoSVG, qui ignore `<text>`, les filtres et les motifs : `splash.py`
  convertit donc les lettres en chemins et n'utilise que des formes simples.
- `logo.py` produit aussi `logo-nuit.svg`, une variante sur fond noir non utilisée pour l'instant.
