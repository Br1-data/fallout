// Protocollo delle spedizioni: browser isolati, canale locale e orologio controllato.
// Eseguire con: node --test tools/net_expedition_test.js
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const { webcrypto } = require('node:crypto');

const source = fs.readFileSync(path.join(__dirname, '../web/net.js'), 'utf8');
const ROUND_MS = 720000;
const plain = value => JSON.parse(JSON.stringify(value));

class Network {
    constructor() {
        this.now = 1800000000000 + 1000;
        this.channels = [];
        this.pending = [];
        this.clients = [];
        this.sent = [];
    }
    peer(name, room = 'squadra', kind = 'locale') {
        const network = this, owned = [], timers = [], listeners = {};
        class Channel {
            constructor(topic) { this.topic = topic; this.closed = false; network.channels.push(this); owned.push(this); }
            postMessage(message) {
                network.sent.push(plain(message));
                network.channels.forEach(other => {
                    if (other !== this && !other.closed && other.topic === this.topic)
                        network.pending.push(() => { if (!other.closed && other.onmessage) other.onmessage({data: plain(message)}); });
                });
            }
            close() { this.closed = true; }
        }
        class Clock extends Date { static now() { return network.now; } }
        const sandbox = {
            crypto: webcrypto, TextEncoder, TextDecoder, Uint8Array, URL, URLSearchParams, Date: Clock,
            performance: {now: () => network.now}, BroadcastChannel: Channel, console,
            setTimeout: fn => { timers.push({fn, once: true}); return timers.length; },
            setInterval: fn => { timers.push({fn}); return timers.length; },
            clearTimeout() {}, clearInterval() {}, location: {href: 'http://localhost/index.html'},
            addEventListener: (type, fn) => { listeners[type] = fn; }
        };
        sandbox.window = sandbox;
        vm.runInNewContext(source, sandbox, {filename: 'web/net.js'});
        const api = sandbox.DustfallNet;
        api.init({kind, room});
        api.setProfile(JSON.stringify({n: name, l: 1, f: 0, o: 0, w: 'Ferrocittà'}));
        const client = {
            api, owned, timers, closed: false,
            view: () => plain(api.expedition()),
            close: () => {
                listeners.pagehide();
                owned.forEach(channel => channel.close());
                client.closed = true;
            }
        };
        this.clients.push(client);
        return client;
    }
    flush() {
        // Esegue una volta i timeout di presentazione; gli intervalli sono espliciti con tick().
        this.clients.filter(c => !c.closed).forEach(c => c.timers.filter(t => t.once && !t.ran).forEach(t => { t.ran = true; t.fn(); }));
        let count = 0;
        while (this.pending.length) {
            assert.ok(++count < 10000, 'il protocollo deve convergere senza cicli di messaggi');
            this.pending.shift()();
        }
    }
    tick(ms = 3000) {
        this.now += ms;
        this.clients.filter(c => !c.closed).forEach(c => c.timers.filter(t => !t.once).forEach(t => t.fn()));
        this.flush();
    }
    inject(client, message) {
        const channel = client.owned.findLast(c => !c.closed);
        channel.onmessage({data: plain(message)});
        this.flush();
    }
}

test('fotografie fresche, obiettivi condivisi, contributi personali e doppioni', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno');
    n.flush();
    const targets = a.view().targets.map(t => t.key);
    assert.equal(new Set(targets).size, 3);
    assert.deepEqual(targets, b.view().targets.map(t => t.key));
    assert.equal(a.api.expeditionPhoto('mk5'), false);
    assert.equal(a.api.expeditionPhoto(targets[0]), true);
    assert.equal(a.api.expeditionPhoto(targets[0]), false);
    n.flush();
    assert.equal(b.view().targets[0].done, true);
    assert.equal(b.view().targets[0].by, 'Ada');
    assert.equal(a.view().own, 1);
    assert.equal(b.view().own, 0);
    assert.equal(b.api.expeditionPhoto(targets[0]), true, 'si può contribuire anche a un obiettivo già trovato');
    b.api.expeditionPhoto(targets[1]);
    b.api.expeditionPhoto(targets[2]);
    n.flush();
    assert.equal(a.view().complete, true);
    assert.equal(b.view().complete, true);
    assert.deepEqual(a.view().contributors.map(c => c.count), [3, 1]);
    n.tick();
    assert.equal(a.view().contributors.length, 2);
    assert.equal(b.view().own, 3);
    const copy = a.api.expedition();
    copy.targets[0].key = 'falso'; copy.contributors[0].count = 999;
    assert.equal(a.view().targets[0].key, targets[0]);
    assert.equal(a.view().contributors[0].count, 3);
});

test('ingresso tardivo e uscita del primo fotografo conservano il lavoro della squadra', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno');
    n.flush();
    const targets = a.view().targets.map(t => t.key);
    a.api.expeditionPhoto(targets[0]); b.api.expeditionPhoto(targets[1]);
    n.flush(); a.close(); n.flush();
    const c = n.peer('Carla'); n.flush();
    assert.equal(c.view().targets.filter(t => t.done).length, 2);
    assert.equal(c.view().own, 0);
    assert.ok(c.view().contributors.some(p => p.name === 'Ada'));
    c.api.expeditionPhoto(targets[2]); n.flush();
    assert.equal(b.view().complete, true);
    assert.equal(c.view().complete, true);
});

test('heartbeat recupera una fotografia persa dal trasporto', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno');
    n.flush();
    a.api.expeditionPhoto(a.view().targets[0].key);
    n.pending.length = 0; // pacchetto QoS 0 perso
    assert.equal(b.view().targets.some(t => t.done), false);
    n.tick();
    assert.equal(b.view().targets.filter(t => t.done).length, 1);
});

test('cambio stanza e rete disattivata non importano progressi o eventi precedenti', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno');
    n.flush();
    a.api.expeditionPhoto(a.view().targets[0].key); n.flush();
    b.api.sendChat('messaggio della vecchia stanza'); n.flush();
    const stale = n.sent.findLast(m => m.t === 'ex' && m.d.c.length);
    a.api.setRoom('altra-stanza');
    assert.equal(a.api.poll(), '');
    assert.equal(a.view().own, 0);
    n.inject(a, {...stale, u: 'vecchio', s: 99999});
    assert.equal(a.view().contributors.length, 0);
    const c = n.peer('Carla', 'altra-stanza'); n.flush();
    assert.deepEqual(a.view().targets, c.view().targets);
    const off = n.peer('Offline', 'squadra', 'off'); n.flush();
    assert.equal(off.api.expeditionPhoto(off.view().targets[0].key), false);
    assert.equal(off.view().own, 0);
    assert.equal(off.view().complete, false);
});

test('nuovo round azzera i contributi e respinge fotografie del round scaduto', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno');
    n.flush();
    a.api.expeditionPhoto(a.view().targets[0].key); n.flush();
    const stale = n.sent.findLast(m => m.t === 'ex' && m.d.c.length), oldRound = a.view().round;
    n.tick(ROUND_MS);
    assert.equal(a.view().round, oldRound + 1);
    assert.equal(a.view().own, 0);
    assert.equal(a.view().complete, false);
    assert.equal(a.view().seconds, 719);
    n.inject(b, {...stale, s: 999999});
    assert.equal(b.view().contributors.length, 0);
    assert.deepEqual(a.view().targets, b.view().targets);
});

test('payload malformati, identità locali falsificate e dati fuori tempo sono respinti', () => {
    const n = new Network(), a = n.peer('Ada'); n.flush();
    const state = a.view(), key = state.targets[0].key;
    const good = {r: state.round, room: 'squadra', c: [{u: 'bruno', n: 'Bruno', p: [{k: key, t: n.now}]}]};
    const bad = [null, {}, {...good, r: state.round - 1}, {...good, room: 'altrove'},
        {...good, c: Array(65).fill(good.c[0])}, {...good, c: [{...good.c[0], u: a.api.selfId()}]},
        {...good, c: [{...good.c[0], u: '__proto__'}]},
        {...good, c: [{...good.c[0], p: [{k: 'mk5', t: n.now}]}]},
        {...good, c: [{...good.c[0], p: [{k: key, t: n.now + 6000}]}]},
        {...good, c: [{...good.c[0], p: [{k: key, t: state.round * ROUND_MS - 1}]}]},
        {...good, c: [{...good.c[0], p: [{k: key, t: 'ora'}]}]}];
    bad.forEach((data, i) => n.inject(a, {u: 'bruno', s: i + 1, t: 'ex', d: data}));
    assert.equal(a.view().own, 0);
    assert.equal(a.view().contributors.length, 0);
    n.inject(a, {u: 'bruno', s: 100, t: 'ex', d: good});
    assert.equal(a.view().targets[0].done, true);
    assert.equal(a.view().own, 0);
});

test('memoria limitata con un posto garantito per il contributo locale', () => {
    const n = new Network(), a = n.peer('Ada'); n.flush();
    const state = a.view(), key = state.targets[0].key;
    for (let i = 0; i < 100; i++) n.inject(a, {u: 'relay', s: i + 1, t: 'ex', d: {
        r: state.round, room: 'squadra', c: [{u: 'peer' + i, n: 'Viandante', p: [{k: key, t: n.now}]}]
    }});
    assert.equal(a.view().contributors.length, 63);
    assert.equal(a.api.expeditionPhoto(key), true);
    assert.equal(a.view().contributors.length, 64);
    assert.equal(a.view().own, 1);
});
