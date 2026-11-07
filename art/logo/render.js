// Rend un SVG de ce dossier en PNG 1920x1080 avec Chromium (Playwright).
// Usage : node render.js logo  -> logo.png
const {chromium} = require('playwright'); const fs = require('fs');
const font = p => fs.readFileSync(__dirname + '/../../resources/' + p).toString('base64');
const rubik = font('Rubik-Regular.ttf'), jp = font('DroidSansFallbackFull.ttf');
(async () => {
  const b = await chromium.launch(); const p = await b.newPage({viewport: {width: 1920, height: 1080}});
  for (const n of process.argv.slice(2)) {
    const svg = fs.readFileSync(`${__dirname}/${n}.svg`, 'utf8');
    await p.setContent(`<html><head><style>@font-face{font-family:Rubik;src:url(data:font/ttf;base64,${rubik})}@font-face{font-family:DroidJP;src:url(data:font/ttf;base64,${jp})}html,body{margin:0}</style></head><body>${svg}</body></html>`);
    await p.evaluate(() => document.fonts.ready); await p.waitForTimeout(300);
    await p.screenshot({path: `${__dirname}/${n}.png`});
  }
  await b.close();
})();
