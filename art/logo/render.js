// Rend logo.svg en PNG 1920x1080 avec Chromium (Playwright). Usage : node render.js
const {chromium} = require('playwright'); const fs = require('fs');
(async () => {
  const b = await chromium.launch(); const p = await b.newPage({viewport: {width: 1920, height: 1080}});
  await p.setContent(`<html><body style="margin:0">${fs.readFileSync(__dirname + '/logo.svg', 'utf8')}</body></html>`);
  await p.screenshot({path: __dirname + '/logo.png'});
  await b.close();
})();
