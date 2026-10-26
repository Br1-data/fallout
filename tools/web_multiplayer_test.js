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
        async function makePage(name, room, gameArgs, touch = false) {
            const context = await browser.newContext({ viewport: touch ? { width: 960, height: 540 } : { width: 1280, height: 720 }, hasTouch: touch });
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
        const aId = await a.evaluate(() => DustfallNet.selfId());
        await a.evaluate(() => DustfallNet.sendState([0,12.5,2,30,0.2,0,3,0,6,1,0,1]));
        await wait(b, id => {
            const peer = DustfallNet.debug().peers[id];
            return peer && peer.profile && peer.profile.n === 'Ada' && peer.state && peer.state[1] === 12.5 && peer.state[8] === 6;
        }, aId);
        console.log('[OK] Due contesti indipendenti condividono profilo, posizione e registratore tramite MQTT cifrato.');

        await a.evaluate(() => { DustfallNet.sendChat('Avanziamo alla Base Delta-9!'); DustfallNet.sendEmote(1); });
        await wait(b, () => {
            const events = DustfallNet.poll();
            window.testEvents = (window.testEvents || '') + events;
            return window.testEvents.includes('Avanziamo alla Base Delta-9!') && window.testEvents.includes('E\t');
        });
        console.log('[OK] Chat e saluto attraversano il broker.');

        const c = await makePage('Carla', room);
        await wait(c, id => {
            const peer = DustfallNet.debug().peers[id];
            return peer && peer.state && peer.state[8] === 6 && peer.state[9] === 1;
        }, aId);
        await a.evaluate(() => DustfallNet.bye());
        await a.context().close(); // l'uscita reale interrompe anche gli heartbeat del browser
        await wait(b, () => DustfallNet.peerCount() === 1);
        assert.equal(await c.evaluate(() => DustfallNet.peerCount()), 1);
        console.log('[OK] Ingresso tardivo con stato corrente e uscita di un giocatore.');

        const d = await makePage('Dina', room + '-alt');
        assert.equal(await d.evaluate(() => DustfallNet.peerCount()), 0);
        await c.evaluate(name => DustfallNet.setRoom(name), room + '-alt');
        await wait(c, () => DustfallNet.peerCount() === 1);
        await wait(d, () => DustfallNet.peerCount() === 1);
        await b.evaluate(() => DustfallNet.poll());
        await d.evaluate(() => DustfallNet.sendChat('Solo nella nuova stanza'));
        await wait(c, () => DustfallNet.poll().includes('Solo nella nuova stanza'));
        assert.equal(await b.evaluate(() => DustfallNet.poll().includes('Solo nella nuova stanza')), false);
        console.log('[OK] Le stanze isolano presenze e messaggi.');

        // Input reale -> fotografia del reattore -> ponte WASM -> altro browser.
        const observer = await makePage('Osservatore', room + '-photo');
        const game = await makePage('Gameplay', room + '-photo',
            '--openworld --zone 1 --pos -22 15 --yaw 180 --pitch 15 --give all --name Collaudatore');
        await wait(game, () => document.documentElement.classList.contains('started') &&
            DustfallNet.debug().self && DustfallNet.debug().self[11] === 1 && DustfallNet.debug().self[8] === 1);
        await game.locator('#canvas').focus();
        await settleInput(game);
        const photo = await game.evaluate(() => ({ id: DustfallNet.selfId(), flash: DustfallNet.debug().self[9], model: DustfallNet.debug().self[8] }));
        await game.mouse.move(640, 360);
        await game.mouse.down();
        try {
            await wait(game, before => DustfallNet.debug().self[9] > before, photo.flash);
        } catch (error) {
            console.error('Stato del giocatore:', await game.evaluate(() => DustfallNet.debug().self));
            throw error;
        } finally {
            await game.mouse.up();
        }
        await wait(observer, ({ id, flash, model }) => {
            const peer = DustfallNet.debug().peers[id];
            return peer && peer.state && peer.state[8] === model && peer.state[9] > flash;
        }, photo);
        console.log('[OK] Una fotografia scattata nel gioco si vede anche nell’altro browser tramite MQTT.');

        async function selectTool(key, tool) {
            await game.keyboard.down(key);
            try { await wait(game, expected => DustfallNet.debug().self[8] === expected, tool); }
            finally { await game.keyboard.up(key); }
            await wait(observer, ({ id, tool }) => {
                const peer = DustfallNet.debug().peers[id];
                return peer && peer.state && peer.state[8] === tool;
            }, { id: photo.id, tool });
        }
        await selectTool('Digit5', 4);
        await selectTool('Digit7', 6);
        console.log('[OK] Scanner e registratore equipaggiati si aggiornano anche per un compagno fermo.');

        async function quicksave() {
            await game.evaluate(() => { try { Module.FS.unlink('/dustfall/saves-atlas/slot6.sav'); } catch (_) {} });
            await game.keyboard.down('F5');
            await wait(game, () => Module.FS.analyzePath('/dustfall/saves-atlas/slot6.sav').exists);
            await game.keyboard.up('F5');
            return game.evaluate(() => Module.FS.readFile('/dustfall/saves-atlas/slot6.sav', { encoding: 'utf8' }));
        }
        await settleInput(game);
        const save = await quicksave();
        const lines = save.trim().split('\n');
        assert.equal(lines[0], 'DUSTFALL_SAVE 4');
        assert.equal(Number(lines[10].trim().split(/\s+/)[0]), 6, 'il registratore resta equipaggiato nel salvataggio');
        const photos = lines[14].trim().split(/\s+/).slice(1);
        if (!photos.includes('reactor')) {
            console.error('Fotografie nel diario:', photos, 'posizione:', lines[11]);
            if (process.env.DUSTFALL_TEST_ARTIFACTS) {
                fs.mkdirSync(process.env.DUSTFALL_TEST_ARTIFACTS, { recursive: true });
                await game.screenshot({ path: path.join(process.env.DUSTFALL_TEST_ARTIFACTS, 'photo-failure.png') });
            }
        }
        assert.ok(photos.includes('reactor'), 'la fotografia realmente scattata entra nel diario persistente');
        console.log('[OK] Il browser salva il personaggio nel formato della campagna esplorativa.');
        if (process.env.DUSTFALL_TEST_ARTIFACTS) {
            fs.mkdirSync(process.env.DUSTFALL_TEST_ARTIFACTS, { recursive: true });
            await game.screenshot({ path: path.join(process.env.DUSTFALL_TEST_ARTIFACTS, 'multiplayer.png') });
            await game.context().close();
            const mobile = await makePage('Touch', room + '-mobile',
                '--openworld --zone 0 --pos 0 76 --yaw 180 --pitch 0 --give all --name Esploratrice', true);
            await wait(mobile, () => document.documentElement.classList.contains('started') && DustfallNet.debug().self && DustfallNet.debug().self[8] === 1);
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
