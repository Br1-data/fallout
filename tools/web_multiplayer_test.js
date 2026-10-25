// Test multigiocatore reale: contesti Chromium separati e broker MQTT WebSocket locale.
// Uso: NODE_PATH=<dipendenze>/node_modules node tools/web_multiplayer_test.js build-web/site
// Dipendenze: playwright@1.56.1 aedes@0.51.3 ws@8.18.3.
// CHROMIUM_EXECUTABLE permette di scegliere un Chromium gia' installato.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { chromium } = require('playwright');
const aedes = require('aedes');
const { WebSocketServer, createWebSocketStream } = require('ws');

const root = path.resolve(process.argv[2] || 'build-web/site');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
    '.png': 'image/png', '.ttf': 'font/ttf', '.webmanifest': 'application/manifest+json' };
const fixture = `<!doctype html><meta charset="utf-8"><script src="/net.js"></script><script>
const q = new URLSearchParams(location.search);
DustfallNet.init({kind:'mqtt',mqtt:q.get('mqtt'),room:q.get('room')});
DustfallNet.setProfile(JSON.stringify({n:q.get('name'),l:1,f:0,o:0,w:'Test MQTT'}));
</script>`;

function listen(server) {
    return new Promise((resolve, reject) => {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', () => resolve(server.address().port));
    });
}

async function run() {
    assert.ok(fs.existsSync(path.join(root, 'dustfall.wasm')), 'Compilare prima la versione web.');
    assert.ok(fs.existsSync(path.join(root, 'net.js')), 'net.js manca dal sito compilato.');
    const broker = aedes();
    const server = http.createServer((req, res) => {
        const pathname = decodeURIComponent(new URL(req.url, 'http://test').pathname);
        if (pathname === '/net-test.html') {
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(fixture);
            return;
        }
        const file = path.resolve(root, '.' + pathname);
        if (!file.startsWith(root + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
            res.writeHead(404); res.end(); return;
        }
        res.writeHead(200, { 'Content-Type': mime[path.extname(file)] || 'application/octet-stream' });
        fs.createReadStream(file).pipe(res);
    });
    const sockets = new WebSocketServer({ server, path: '/mqtt' });
    sockets.on('connection', socket => {
        const stream = createWebSocketStream(socket);
        stream.on('error', () => {}); // le chiusure improvvise simulano l'uscita di un giocatore
        broker.handle(stream);
    });
    broker.on('clientError', () => {});
    const errors = [];
    let browser;
    try {
        const port = await listen(server);
        const origin = `http://127.0.0.1:${port}`;
        const mqtt = `ws://127.0.0.1:${port}/mqtt`;
        const executable = process.env.CHROMIUM_EXECUTABLE || (fs.existsSync('/usr/bin/chromium') ? '/usr/bin/chromium' : undefined);
        browser = await chromium.launch({ executablePath: executable,
            args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
        // L'orologio continua a scorrere, ma parte a meta' turno per evitare un cambio durante il test.
        const clockOffset = Math.floor(Date.now() / 720000) * 720000 + 360000 - Date.now();
        async function makePage(name, room, gameArgs, touch = false) {
            const context = await browser.newContext({ viewport: touch ? { width: 960, height: 540 } : { width: 1280, height: 720 }, hasTouch: touch });
            await context.addInitScript(offset => {
                const now = Date.now.bind(Date);
                Date.now = () => now() + offset;
            }, clockOffset);
            const page = await context.newPage();
            page.on('pageerror', error => errors.push(`${name}: ${error.message}`));
            const url = new URL(gameArgs ? '/index.html' : '/net-test.html', origin);
            url.searchParams.set('mqtt', mqtt);
            if (gameArgs) {
                url.searchParams.set('rete', 'mqtt');
                url.searchParams.set('args', gameArgs);
                url.searchParams.set('autoplay', '1');
                if (touch) url.searchParams.set('touch', '1');
                url.hash = room;
            } else {
                url.searchParams.set('name', name);
                url.searchParams.set('room', room);
            }
            await page.goto(url.toString());
            await page.waitForFunction(() => window.DustfallNet && DustfallNet.status() === 2, null, { timeout: 30000 });
            return page;
        }
        async function wait(page, predicate, arg) {
            await page.waitForFunction(predicate, arg, { timeout: 20000 });
        }
        async function settleInput(page) {
            await page.evaluate(() => Promise.all([
                new Promise(resolve => setTimeout(resolve, 800)),
                new Promise(resolve => {
                    let frames = 0;
                    function next() { if (++frames >= 20) resolve(); else requestAnimationFrame(next); }
                    requestAnimationFrame(next);
                })
            ]));
        }
        const room = 'test-' + Math.random().toString(36).slice(2, 10);
        const a = await makePage('Ada', room);
        const b = await makePage('Bruno', room);
        await wait(a, () => DustfallNet.peerCount() === 1);
        await wait(b, () => DustfallNet.peerCount() === 1);
        const keys = await a.evaluate(() => DustfallNet.expedition().targets.map(t => t.key));
        assert.deepEqual(await b.evaluate(() => DustfallNet.expedition().targets.map(t => t.key)), keys);
        assert.equal(new Set(keys).size, 3);
        console.log('[OK] Due contesti indipendenti si incontrano tramite MQTT cifrato.');

        assert.equal(await a.evaluate(key => DustfallNet.expeditionPhoto(key), keys[0]), true);
        await wait(b, () => DustfallNet.expedition().targets.filter(t => t.done).length === 1);
        assert.equal(await b.evaluate(() => DustfallNet.expedition().own), 0);
        assert.equal(await b.evaluate(key => DustfallNet.expeditionPhoto(key), keys[1]), true);
        await wait(a, () => DustfallNet.expedition().targets.filter(t => t.done).length === 2);
        assert.equal(await b.evaluate(key => DustfallNet.expeditionPhoto(key), keys[1]), false);
        assert.equal(await b.evaluate(() => DustfallNet.expedition().own), 1);
        assert.equal(await a.evaluate(() => DustfallNet.laserRelay(0)), true);
        await wait(b, () => DustfallNet.laserRelays() === 1);
        assert.equal(await b.evaluate(() => DustfallNet.laserRelay(0)), false);
        console.log('[OK] Foto e ripetitori condivisi; nessun contributo duplicato o personale inventato.');

        const c = await makePage('Carla', room);
        await wait(c, () => DustfallNet.expedition().targets.filter(t => t.done).length === 2 && DustfallNet.laserRelays() === 1);
        assert.equal(await c.evaluate(() => DustfallNet.expedition().own), 0);
        await a.evaluate(() => DustfallNet.bye());
        await a.context().close();
        assert.equal(await c.evaluate(key => DustfallNet.expeditionPhoto(key), keys[2]), true);
        await wait(b, () => DustfallNet.expedition().complete);
        await wait(c, () => DustfallNet.expedition().complete);
        assert.ok(await c.evaluate(() => DustfallNet.expedition().contributors.some(p => p.name === 'Ada' && p.count === 1)));
        assert.equal(await b.evaluate(() => DustfallNet.laserRelay(2)), true);
        await wait(c, () => DustfallNet.laserRelays() === 5);
        console.log('[OK] Ingresso tardivo, uscita del primo giocatore e completamento senza host.');

        const d = await makePage('Dina', room + '-alt');
        assert.equal(await d.evaluate(() => DustfallNet.peerCount()), 0);
        assert.equal(await d.evaluate(() => DustfallNet.expedition().targets.filter(t => t.done).length), 0);
        assert.equal(await d.evaluate(() => DustfallNet.laserRelays()), 0);
        await c.evaluate(name => DustfallNet.setRoom(name), room + '-alt');
        await wait(c, () => DustfallNet.peerCount() === 1);
        await wait(d, () => DustfallNet.peerCount() === 1);
        assert.equal(await c.evaluate(() => DustfallNet.expedition().own), 0);
        assert.equal(await c.evaluate(() => DustfallNet.expedition().complete), false);
        assert.equal(await c.evaluate(() => DustfallNet.laserRelays()), 0);
        assert.equal(await d.evaluate(() => DustfallNet.laserRelay(1)), true);
        await wait(c, () => DustfallNet.laserRelays() === 2);
        assert.equal(await b.evaluate(() => DustfallNet.laserRelays()), 5);
        console.log('[OK] Cambio stanza e isolamento dei progressi di foto e missione laser.');

        // Questa parte attraversa tastiera -> gameplay C++ -> ponte WASM -> MQTT, senza chiamare laserRelay.
        const observer = await makePage('Osservatore', room + '-shot');
        const game = await makePage('Gameplay', room + '-shot',
            '--openworld --zone 0 --pos 0 76 --yaw 180 --pitch 0 --give all --name Collaudatore');
        await wait(game, () => document.documentElement.classList.contains('started') &&
            DustfallNet.debug().self && DustfallNet.debug().self[11] === 1);
        await game.locator('#canvas').focus();
        // Mantieni il tasto fino a un fotogramma: il rendering software puo' essere lento.
        await game.keyboard.down('5');
        await wait(game, () => DustfallNet.debug().self[8] === 4);
        await game.keyboard.up('5');
        // Attendi anche la fine dell'animazione di equipaggiamento prima del clic.
        await settleInput(game);
        assert.equal(await game.evaluate(() => DustfallNet.laserRelays()), 0);
        await game.mouse.move(640, 360);
        await game.mouse.down();
        try {
            await wait(game, () => (DustfallNet.laserRelays() & 1) !== 0);
        } catch (error) {
            console.error('Stato del giocatore:', await game.evaluate(() => DustfallNet.debug().self));
            if (process.env.DUSTFALL_TEST_ARTIFACTS) {
                fs.mkdirSync(process.env.DUSTFALL_TEST_ARTIFACTS, { recursive: true });
                await game.screenshot({ path: path.join(process.env.DUSTFALL_TEST_ARTIFACTS, 'laser-failure.png') });
            }
            throw error;
        }
        await game.mouse.up();
        await wait(observer, () => DustfallNet.laserRelays() === 1);
        assert.equal(await game.evaluate(() => DustfallNet.laserRelays()), 1);
        if (process.env.DUSTFALL_TEST_ARTIFACTS) {
            fs.mkdirSync(process.env.DUSTFALL_TEST_ARTIFACTS, { recursive: true });
            await game.screenshot({ path: path.join(process.env.DUSTFALL_TEST_ARTIFACTS, 'multiplayer.png') });
        }
        console.log('[OK] Un colpo reale del blaster nel gioco attiva il ripetitore e attraversa il ponte WASM.');
        async function quicksave() {
            await game.evaluate(() => { try { Module.FS.unlink('/dustfall/saves/slot6.sav'); } catch (_) {} });
            await game.keyboard.down('F5');
            await wait(game, () => Module.FS.analyzePath('/dustfall/saves/slot6.sav').exists);
            await game.keyboard.up('F5');
            return game.evaluate(() => Module.FS.readFile('/dustfall/saves/slot6.sav', { encoding: 'utf8' }));
        }
        function rewardState(save) {
            const lines = save.trim().split('\n');
            assert.equal(lines[0], 'DUSTFALL_SAVE 2');
            const inventory = lines[13].trim().split(/\s+/).map(Number);
            let caps = 0;
            for (let i = 1; i < inventory.length; i += 2) if (inventory[i] === 0) caps = inventory[i + 1];
            const flags = Object.fromEntries(lines.slice(16, 16 + Number(lines[15])).map(line => line.trim().split(/\s+/)));
            return { xp: Number(lines[8].trim().split(/\s+/)[1]), caps, round: Number(flags.expedition_reward_round || 0) };
        }
        const before = rewardState(await quicksave());
        await game.evaluate(() => DustfallNet.expedition().targets.forEach(t => DustfallNet.expeditionPhoto(t.key)));
        await settleInput(game);
        const paid = rewardState(await quicksave());
        assert.equal(paid.xp - before.xp, 120);
        assert.equal(paid.caps - before.caps, 60);
        assert.equal(paid.round, await game.evaluate(() => DustfallNet.expedition().round));
        await settleInput(game);
        assert.deepEqual(rewardState(await quicksave()), paid);
        console.log('[OK] Il gameplay assegna 120 PE e 60 tappi una sola volta e salva il turno riscattato.');
        if (process.env.DUSTFALL_TEST_ARTIFACTS) {
            await game.context().close();
            const mobile = await makePage('Touch', room + '-mobile',
                '--openworld --zone 0 --pos 0 76 --yaw 180 --pitch 0 --give all --name Esploratrice', true);
            await wait(mobile, () => document.documentElement.classList.contains('started') && DustfallNet.debug().self);
            await mobile.keyboard.down('5');
            await wait(mobile, () => DustfallNet.debug().self[8] === 4);
            await mobile.keyboard.up('5');
            await settleInput(mobile);
            await mobile.screenshot({ path: path.join(process.env.DUSTFALL_TEST_ARTIFACTS, 'mobile.png') });
        }
        assert.deepEqual(errors, [], 'Errori JavaScript nel browser');
        console.log('=== MULTIGIOCATORE SUPERATO ===');
    } finally {
        if (browser) await browser.close();
        for (const socket of sockets.clients) socket.terminate();
        await new Promise(resolve => sockets.close(resolve));
        await new Promise(resolve => broker.close(resolve));
        await new Promise(resolve => server.close(resolve));
    }
}

run().catch(error => { console.error(error); process.exitCode = 1; });
