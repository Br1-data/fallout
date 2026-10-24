// Test automatico della versione web: serve il sito compilato, lo apre in Chromium senza finestra
// ed esegue "Dustfall --selftest" dentro il browser (tutta la trama, salvataggi compresi).
// Uso: node tools/web_selftest.js <cartella del sito>      (richiede il pacchetto npm "playwright")
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const root = path.resolve(process.argv[2] || 'site');
const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.png': 'image/png',
                '.ttf': 'font/ttf', '.webmanifest': 'application/manifest+json' };

const server = http.createServer((req, res) => {
    const file = path.join(root, decodeURIComponent(new URL(req.url, 'http://x').pathname));
    if (!file.startsWith(root) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'Content-Type': types[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
});

server.listen(0, async () => {
    const url = `http://localhost:${server.address().port}/index.html?rete=off&args=--selftest`;
    const browser = await chromium.launch({ args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
    const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
    page.on('console', m => { const t = m.text(); if (!t.startsWith('INFO')) console.log(t); });
    page.on('pageerror', e => console.log('ERRORE nella pagina:', e.message));
    let rc = 1;
    try {
        await page.goto(url);
        await page.waitForFunction(() => window.dfTestResult !== undefined, null, { timeout: 600000 });
        rc = await page.evaluate(() => window.dfTestResult);
    } catch (e) {
        console.log('Test non completato:', e.message);
    }
    await browser.close();
    server.close();
    process.exit(rc);
});
