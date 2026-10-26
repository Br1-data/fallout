// Protocollo multigiocatore: presenza, stato degli avatar, chat e isolamento delle stanze.
// Eseguire con: node --test tools/net_expedition_test.js
const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const { webcrypto } = require('node:crypto');

const source = fs.readFileSync(path.join(__dirname, '../web/net.js'), 'utf8');
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


const state = (model = 6, flash = 0, x = 1) => [0, x, 2, 3, 0, 0, 0, 0, model, flash, 0, 1];

test('due browser scoprono profili e strumenti da esplorazione', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno');
    n.flush();
    assert.equal(a.api.peerCount(), 1);
    assert.equal(b.api.peerCount(), 1);
    assert.equal(a.api.status(), 2);
    a.api.sendState(state(6, 1, 12.345)); n.flush();
    const peer = plain(b.api.debug().peers)[a.api.selfId()];
    assert.equal(peer.profile.n, 'Ada');
    assert.equal(peer.state[1], 12.35);
    assert.equal(peer.state[8], 6, 'il registratore non viene troncato al vecchio massimo 3');
    assert.equal(peer.state[9], 1);
    assert.ok(b.api.poll().includes('S\t' + a.api.selfId()));
    a.api.sendState(state(4, 1, 12.345)); n.flush();
    assert.equal(plain(b.api.debug().peers)[a.api.selfId()].state[8], 4, 'lo scanner si vede subito anche da fermi');
});

test('stato aggiornato una volta, duplicati e pacchetti vecchi non lo sovrascrivono', () => {
    const n = new Network(), a = n.peer('Ada'); n.flush(); a.api.poll();
    n.inject(a, {u:'bruno',s:100,t:'st',d:state(5, 2, 20)});
    n.inject(a, {u:'bruno',s:99,t:'st',d:state(1, 0, -20)});
    n.inject(a, {u:'bruno',s:100,t:'st',d:state(2, 0, -30)});
    const peer = plain(a.api.debug().peers).bruno;
    assert.equal(peer.state[1], 20);
    assert.equal(peer.state[8], 5);
    assert.equal(peer.state[9], 2);
    assert.equal(a.api.poll().split('\n').filter(l => l.startsWith('S\t')).length, 1);
    assert.equal(a.api.poll(), '');
});

test('stato remoto resta entro i limiti e gli identificativi non alterano il prototipo', () => {
    const n = new Network(), a = n.peer('Ada'); n.flush();
    n.inject(a, {u:'__proto__',s:1,t:'st',d:state()});
    assert.equal(a.api.peerCount(), 0);
    n.inject(a, {u:'constructor',s:1,t:'st',d:[9,1e8,-1e8,1e8,90,-90,80,7,99,1e9,-4,9]});
    const peer = plain(a.api.debug().peers).constructor;
    assert.deepEqual(peer.state, [2,2000,-500,2000,10,-2,30,1,6,1000000,0,1]);
    n.inject(a, {u:'constructor',s:2,t:'st',d:[]});
    assert.deepEqual(plain(a.api.debug().peers).constructor.state, peer.state);
});

test('chat e saluti si ricevono una volta con pulizia del testo e anti-spam', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno'); n.flush(); a.api.poll();
    b.api.sendChat('  Ciao\t\n  Ada!  '); b.api.sendEmote(1); n.flush();
    let events = a.api.poll();
    assert.ok(events.includes('C\t' + b.api.selfId() + '\tCiao Ada!'));
    assert.ok(events.includes('E\t' + b.api.selfId() + '\t1'));
    for (let i=0;i<8;i++) n.inject(a, {u:'spam',s:i+1,t:'ch',d:'Messaggio '+i});
    events = a.api.poll();
    assert.equal(events.split('\n').filter(l => l.startsWith('C\tspam\t')).length, 6);
});

test('nuovo arrivato riceve lo stato corrente e uscita rimuove la presenza', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno'); n.flush();
    a.api.sendState(state(4, 4)); n.flush();
    const c = n.peer('Carla'); n.flush();
    assert.equal(plain(c.api.debug().peers)[a.api.selfId()].state[9], 4);
    a.close(); n.flush();
    assert.equal(c.api.peerCount(), 1);
    assert.equal(b.api.peerCount(), 1);
    assert.equal(plain(c.api.debug().peers)[a.api.selfId()], undefined);
});

test('cambiare stanza cancella eventi e vecchi compagni, rete off non trasmette', () => {
    const n = new Network(), a = n.peer('Ada'), b = n.peer('Bruno'); n.flush();
    b.api.sendChat('Vecchia stanza'); n.flush();
    a.api.setRoom('altra');
    assert.equal(a.api.poll(), '');
    assert.equal(a.api.peerCount(), 0);
    const c = n.peer('Carla', 'altra'); n.flush();
    assert.equal(a.api.peerCount(), 1);
    c.api.sendChat('Nuova stanza'); n.flush();
    assert.ok(a.api.poll().includes('Nuova stanza'));
    assert.equal(b.api.poll().includes('Nuova stanza'), false);
    const off = n.peer('Offline', 'squadra', 'off'); n.flush();
    assert.equal(off.api.status(), 0);
    assert.equal(off.api.peerCount(), 0);
    const sent=n.sent.length;
    off.api.sendState(state()); off.api.sendChat('silenzio'); off.api.sendEmote(1); n.flush();
    assert.equal(n.sent.length, sent);
});

test('presenza silenziosa scade mentre un nuovo heartbeat la mantiene viva', () => {
    const n = new Network(), a = n.peer('Ada'); n.flush();
    n.inject(a, {u:'bruno',s:1,t:'hb',d:{n:'Bruno',l:1,o:0,w:'Covo'}});
    assert.equal(a.api.peerCount(), 1);
    n.tick(13000);
    assert.equal(a.api.peerCount(), 0);
    assert.ok(a.api.poll().includes('L\tbruno'));
});
