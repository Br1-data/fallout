// DUSTFALL - rete multigiocatore nel browser.
//
// Non c'è nessun server di gioco, quindi il sito può stare su un hosting statico come GitHub Pages.
// I giocatori della stessa stanza si scambiano piccoli messaggi su due strade insieme:
//  - broker MQTT pubblici (WebSocket sicuri): funzionano su qualsiasi rete, anche dal telefono in 4G/5G;
//    i messaggi sono cifrati con una chiave ricavata dal nome della stanza;
//  - WebRTC diretto tra i browser (Trystero, che si trova tramite i relay Nostr): più rapido, quando la
//    rete lo permette. Se tutti gli altri sono raggiungibili direttamente, la posizione viaggia solo così.
// Ogni messaggio porta l'identificativo del mittente e un numero di sequenza: i doppioni si scartano.
// Con ?rete=locale le schede dello stesso browser si parlano con BroadcastChannel (utile per i test).
//
// Il gioco (C++) chiama DustfallNet.* e ogni fotogramma legge gli eventi con poll().
(function () {
    'use strict';

    var APP_ID = 'dustfall-terre-bruciate-v1';
    var MQTT_BROKERS = ['wss://broker.emqx.io:8084/mqtt', 'wss://broker.hivemq.com:8884/mqtt'];
    var STATE_MS = 100;         // posizione: 10 volte al secondo mentre ci si muove
    var HEARTBEAT_MS = 3000;    // presenza e profilo
    var PEER_TIMEOUT_MS = 12000;
    var DIRECT_FRESH_MS = 5000; // un giocatore è "diretto" se lo abbiamo sentito via WebRTC da poco
    var MAX_NAME = 18, MAX_CHAT = 140, MAX_WHERE = 40;
    var EXPEDITION_MS = 12 * 60 * 1000;
    var EXPEDITION_MAX_PLAYERS = 64;
    var EXPEDITION_POOL = ['vault_door', 'water_tower', 'atom_sign', 'silo', 'radio_tower',
                           'crater', 'hospital', 'skyscraper', 'base_gate'];

    function clean(s, n) {
        return String(s == null ? '' : s).replace(/[\u0000-\u001f\u007f]/g, ' ').replace(/\s+/g, ' ').trim().slice(0, n);
    }
    function num(v, lo, hi) {
        v = Number(v);
        if (!isFinite(v)) return 0;
        return Math.min(hi, Math.max(lo, v));
    }
    function cleanRoom(s) {
        s = String(s || '').toLowerCase().replace(/[^a-z0-9àèéìòù_-]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 24);
        return s || 'pubblica';
    }
    function randomId(n) {
        var c = 'abcdefghijklmnopqrstuvwxyz0123456789', b = new Uint8Array(n), s = '';
        crypto.getRandomValues(b);
        for (var i = 0; i < n; i++) s += c[b[i] % c.length];
        return s;
    }
    var enc = new TextEncoder(), dec = new TextDecoder();

    // -----------------------------------------------------------------------
    //  Cifratura dei messaggi sui broker pubblici (AES-GCM, chiave dal nome della stanza)
    // -----------------------------------------------------------------------
    var subtle = window.crypto && crypto.subtle;
    function sha256(text) { return subtle.digest('SHA-256', enc.encode(text)); }
    function hex(buf) { return Array.prototype.map.call(new Uint8Array(buf), function (x) { return ('0' + x.toString(16)).slice(-2); }).join(''); }
    function roomCrypto(room) {
        if (!subtle) return Promise.resolve({topic: 'dustfall/v1/' + room, key: null});
        return Promise.all([sha256('dustfall-argomento|' + room), sha256('dustfall-chiave|' + room)]).then(function (h) {
            return subtle.importKey('raw', new Uint8Array(h[1]).slice(0, 16), 'AES-GCM', false, ['encrypt', 'decrypt']).then(function (key) {
                return {topic: 'dustfall/v1/' + hex(h[0]).slice(0, 32), key: key};
            });
        });
    }
    function seal(rc, obj) {
        var plain = enc.encode(JSON.stringify(obj));
        if (!rc.key) return Promise.resolve(plain);
        var iv = crypto.getRandomValues(new Uint8Array(12));
        return subtle.encrypt({name: 'AES-GCM', iv: iv}, rc.key, plain).then(function (ct) {
            var out = new Uint8Array(12 + ct.byteLength);
            out.set(iv, 0);
            out.set(new Uint8Array(ct), 12);
            return out;
        });
    }
    function open(rc, bytes) {
        if (!rc.key) return Promise.resolve(JSON.parse(dec.decode(bytes)));
        if (bytes.length < 29) return Promise.reject(new Error('corto'));
        return subtle.decrypt({name: 'AES-GCM', iv: bytes.subarray(0, 12)}, rc.key, bytes.subarray(12)).then(function (pt) {
            return JSON.parse(dec.decode(pt));
        });
    }

    // -----------------------------------------------------------------------
    //  Client MQTT 3.1.1 minimo su WebSocket (QoS 0: connessione, iscrizione, pubblicazione, ping)
    // -----------------------------------------------------------------------
    function mqttString(s) {
        var b = enc.encode(s), out = new Uint8Array(2 + b.length);
        out[0] = b.length >> 8; out[1] = b.length & 255;
        out.set(b, 2);
        return out;
    }
    function mqttPacket(type, parts) {
        var len = 0;
        parts.forEach(function (p) { len += p.length; });
        var head = [type], n = len;
        do { var d = n % 128; n = Math.floor(n / 128); head.push(n > 0 ? d | 128 : d); } while (n > 0);
        var out = new Uint8Array(head.length + len), o = head.length;
        out.set(head, 0);
        parts.forEach(function (p) { out.set(p, o); o += p.length; });
        return out;
    }

    function mqttTransport(url, rc, handlers) {
        var ws = null, up = false, closed = false, buf = new Uint8Array(0), retry = 1000, pingTimer = null, retryTimer = null;
        function connect() {
            if (closed) return;
            try { ws = new WebSocket(url, ['mqtt']); } catch (e) { scheduleRetry(); return; }
            ws.binaryType = 'arraybuffer';
            ws.onopen = function () {
                var flags = new Uint8Array([0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x04, 0x02, 0x00, 0x1e]); // "MQTT", v3.1.1, sessione pulita, keepalive 30 s
                ws.send(mqttPacket(0x10, [flags, mqttString('df' + randomId(20))]));
            };
            ws.onmessage = function (ev) {
                var data = new Uint8Array(ev.data), joined = new Uint8Array(buf.length + data.length);
                joined.set(buf, 0);
                joined.set(data, buf.length);
                buf = joined;
                parse();
            };
            ws.onclose = function () {
                up = false;
                clearInterval(pingTimer);
                scheduleRetry();
            };
            ws.onerror = function () {};
        }
        function scheduleRetry() {
            if (closed || retryTimer) return;
            retryTimer = setTimeout(function () { retryTimer = null; connect(); }, retry);
            retry = Math.min(retry * 2, 30000);
        }
        function parse() {
            while (buf.length >= 2) {
                var mult = 1, len = 0, i = 1, d;
                do {
                    if (i >= buf.length) return; // lunghezza incompleta
                    d = buf[i++];
                    len += (d & 127) * mult;
                    mult *= 128;
                } while (d & 128);
                if (buf.length < i + len) return; // pacchetto incompleto
                var first = buf[0], type = first >> 4, body = buf.slice(i, i + len);
                buf = buf.slice(i + len);
                if (type === 2) { // CONNACK
                    if (body[1] !== 0) { ws.close(); return; }
                    // iscrizione all'argomento della stanza
                    ws.send(mqttPacket(0x82, [new Uint8Array([0, 1]), mqttString(rc.topic), new Uint8Array([0])]));
                    up = true;
                    retry = 1000;
                    clearInterval(pingTimer);
                    pingTimer = setInterval(function () { if (ws.readyState === 1) ws.send(new Uint8Array([0xc0, 0])); }, 20000);
                    handlers.up();
                } else if (type === 3 && body.length >= 2) { // PUBLISH
                    var off = 2 + ((body[0] << 8) | body[1]);
                    if ((first >> 1) & 3) off += 2; // identificativo del pacchetto (QoS 1/2)
                    if (body.length > off) handlers.data(body.subarray(off), 'mqtt');
                }
            }
        }
        connect();
        return {
            name: 'mqtt',
            send: function (bytes) {
                if (up && ws && ws.readyState === 1) ws.send(mqttPacket(0x30, [mqttString(rc.topic), bytes]));
            },
            up: function () { return up; },
            leave: function () {
                closed = true;
                clearInterval(pingTimer);
                clearTimeout(retryTimer);
                if (ws) {
                    try { if (ws.readyState === 1) ws.send(new Uint8Array([0xe0, 0])); ws.close(); } catch (e) {}
                }
            }
        };
    }

    // -----------------------------------------------------------------------
    //  WebRTC diretto (Trystero) e BroadcastChannel per i test
    // -----------------------------------------------------------------------
    function trysteroTransport(kind, roomName, relays, handlers) {
        var lib = window.DustfallP2P && window.DustfallP2P[kind];
        if (!lib || typeof RTCPeerConnection === 'undefined') return null;
        var relayConfig = {warnOnRelayFailure: false};
        if (relays.length) relayConfig.urls = relays; // relay propri (?relay=wss://...)
        var room = lib.joinRoom({appId: APP_ID, relayConfig: relayConfig}, roomName, {
            onJoinError: function (d) { console.info('[rete] collegamento diretto non riuscito, si usa il broker', d && d.error); }
        });
        var action = room.makeAction('m');
        action.onMessage = function (data) { if (data && typeof data === 'object') handlers.obj(data, 'diretto'); };
        room.onPeerJoin = function () { handlers.newPeer(); };
        return {
            name: 'diretto',
            direct: true,
            sendObj: function (obj) {
                if (Object.keys(room.getPeers()).length === 0) return;
                var p = action.send(obj);
                if (p && p.catch) p.catch(function () {});
            },
            up: function () {
                try {
                    var socks = lib.getRelaySockets ? lib.getRelaySockets() : {};
                    for (var k in socks) if (socks[k] && socks[k].readyState === 1) return true;
                } catch (e) {}
                return false;
            },
            leave: function () { try { room.leave(); } catch (e) {} }
        };
    }

    function localTransport(roomName, handlers) {
        var ch = new BroadcastChannel('dustfall-' + APP_ID + '-' + roomName);
        ch.onmessage = function (ev) { if (ev.data && typeof ev.data === 'object') handlers.obj(ev.data, 'locale'); };
        setTimeout(function () { handlers.newPeer(); }, 50);
        return {
            name: 'locale',
            direct: true,
            sendObj: function (obj) { ch.postMessage(obj); },
            up: function () { return true; },
            leave: function () { ch.close(); }
        };
    }

    // -----------------------------------------------------------------------
    //  Stato della rete
    // -----------------------------------------------------------------------
    var net = {
        kind: 'auto',       // auto (broker + diretto), mqtt, nostr, torrent, locale, off
        relays: [],
        brokers: MQTT_BROKERS.slice(),
        roomName: 'pubblica',
        uid: randomId(12),
        seq: 0,
        transports: [],
        rc: null,
        gen: 0,             // cambia a ogni nuova connessione (scarta i messaggi in ritardo)
        peers: {},          // uid -> {state, stateSeq, stateNew, profile, seen, seenSeqs, chat, rtt, directAt, via}
        queue: [],          // righe di eventi per il gioco
        profile: null,
        profileJson: '',
        lastState: null,
        lastStateSent: 0,
        lastHeartbeat: 0
    };

    // Spedizioni fotografiche: stessi obiettivi per tutta la stanza, senza un caposquadra
    // che debba restare connesso. Ogni dodici minuti il segnale radio cambia itinerario.
    // Le fotografie sono un insieme monotono: i doppioni e l'ordine di arrivo non contano.
    var expedition = {round: -1, targets: [], players: Object.create(null)};

    function expeditionCurrent() {
        var round = Math.floor(Date.now() / EXPEDITION_MS);
        if (expedition.round === round) return expedition;
        var seed = 2166136261, text = net.roomName + '|' + round;
        for (var i = 0; i < text.length; i++) seed = Math.imul(seed ^ text.charCodeAt(i), 16777619) >>> 0;
        var pool = EXPEDITION_POOL.slice();
        for (var j = pool.length - 1; j > 0; j--) {
            seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
            var k = seed % (j + 1), swap = pool[j];
            pool[j] = pool[k]; pool[k] = swap;
        }
        expedition = {round: round, targets: pool.slice(0, 3), players: Object.create(null)};
        return expedition;
    }

    function expeditionSnapshot() {
        var e = expeditionCurrent();
        return {r: e.round, room: net.roomName, c: Object.keys(e.players).sort().map(function (id) {
            var p = e.players[id];
            return {u: id, n: p.name, p: Object.keys(p.photos).sort().map(function (key) { return {k: key, t: p.photos[key]}; })};
        })};
    }

    function expeditionBroadcast() {
        if (net.kind !== 'off') send('ex', expeditionSnapshot());
    }

    function expeditionReceive(data) {
        var e = expeditionCurrent(), now = Date.now();
        if (!data || typeof data !== 'object' || data.r !== e.round || data.room !== net.roomName ||
            !Array.isArray(data.c) || data.c.length > EXPEDITION_MAX_PLAYERS) return;
        data.c.forEach(function (row) {
            // Nessun altro browser può attribuire una fotografia al giocatore locale.
            if (!row || typeof row !== 'object' || typeof row.u !== 'string' ||
                !/^[a-z0-9]{1,32}$/.test(row.u) || row.u === net.uid ||
                typeof row.n !== 'string' || row.n.length > 128 || !Array.isArray(row.p) ||
                !row.p.length || row.p.length > 3) return;
            var photos = Object.create(null);
            for (var i = 0; i < row.p.length; i++) {
                var photo = row.p[i];
                if (!photo || e.targets.indexOf(photo.k) < 0 || typeof photo.t !== 'number' ||
                    !Number.isSafeInteger(photo.t) || photo.t < e.round * EXPEDITION_MS ||
                    photo.t >= (e.round + 1) * EXPEDITION_MS || photo.t > now + 5000) return;
                photos[photo.k] = Math.min(photos[photo.k] || photo.t, photo.t);
            }
            var p = e.players[row.u], name = clean(row.n, MAX_NAME) || 'Viandante';
            // Un posto resta riservato al giocatore locale anche nelle stanze molto affollate.
            if (!p && Object.keys(e.players).length >= EXPEDITION_MAX_PLAYERS - (e.players[net.uid] ? 0 : 1)) return;
            if (!p) p = e.players[row.u] = {name: name, photos: Object.create(null)};
            else if (name < p.name) p.name = name;
            Object.keys(photos).forEach(function (key) { p.photos[key] = Math.min(p.photos[key] || photos[key], photos[key]); });
        });
    }

    function expeditionView() {
        var e = expeditionCurrent(), active = net.kind !== 'off', ids = active ? Object.keys(e.players).sort() : [];
        var targets = e.targets.map(function (key) {
            var first = null;
            ids.forEach(function (id) {
                var p = e.players[id];
                if (p.photos[key] !== undefined && (!first || p.photos[key] < first.time ||
                    (p.photos[key] === first.time && id < first.id))) first = {id: id, time: p.photos[key], name: p.name};
            });
            return {key: key, done: !!first, by: first ? first.name : ''};
        });
        var own = active && e.players[net.uid] ? Object.keys(e.players[net.uid].photos).length : 0;
        return {
            round: e.round, seconds: Math.max(0, Math.ceil(((e.round + 1) * EXPEDITION_MS - Date.now()) / 1000)),
            complete: active && targets.every(function (t) { return t.done; }), own: own, targets: targets,
            contributors: ids.map(function (id) { return {id: id, name: e.players[id].name, count: Object.keys(e.players[id].photos).length}; })
                .sort(function (a, b) { return b.count - a.count || a.id.localeCompare(b.id); })
        };
    }

    function push(line) {
        net.queue.push(line);
        if (net.queue.length > 400) net.queue.splice(0, net.queue.length - 400);
    }

    function dropPeer(uid) {
        if (!net.peers[uid]) return;
        delete net.peers[uid];
        push('L\t' + uid);
    }

    // invia su tutte le strade disponibili (onlyDirect: solo WebRTC/locale)
    function send(type, data, onlyDirect) {
        var obj = {u: net.uid, s: ++net.seq, t: type, d: data};
        var gen = net.gen;
        net.transports.forEach(function (tr) { if (tr.direct) tr.sendObj(obj); });
        if (onlyDirect) return;
        var brokers = net.transports.filter(function (tr) { return !tr.direct && tr.up(); });
        if (!brokers.length || !net.rc) return;
        seal(net.rc, obj).then(function (bytes) {
            if (gen !== net.gen) return;
            brokers.forEach(function (tr) { tr.send(bytes); });
        }).catch(function () {});
    }

    function allDirect() {
        var now = Date.now(), ids = Object.keys(net.peers);
        if (!ids.length) return false;
        for (var i = 0; i < ids.length; i++) if (now - net.peers[ids[i]].directAt > DIRECT_FRESH_MS) return false;
        return true;
    }

    function heartbeat() {
        net.lastHeartbeat = Date.now();
        if (net.profile) send('hb', net.profile);
        expeditionBroadcast(); // recupera perdite QoS 0 e aggiorna chi entra a spedizione iniziata
    }

    function receive(m, via) {
        if (!m || typeof m !== 'object' || typeof m.u !== 'string' || m.u === net.uid || !/^[a-z0-9]{1,32}$/.test(m.u)) return;
        var uid = m.u;
        var seq = num(m.s, 0, 1e12);
        var p = net.peers[uid];
        var now = Date.now();
        if (m.t === 'by') { dropPeer(uid); return; }
        if (!p) {
            p = net.peers[uid] = {state: null, stateSeq: -1, stateNew: false, profile: null, seen: now, seenSeqs: [], chat: [], rtt: -1, directAt: 0, via: via};
            push('J\t' + uid);
            heartbeat(); // presentati subito al nuovo arrivato
            if (net.lastState) send('st', net.lastState);
        }
        p.seen = now;
        if (via !== 'mqtt') p.directAt = now;
        p.via = now - p.directAt < DIRECT_FRESH_MS ? 'diretto' : via;
        // doppioni (lo stesso messaggio arriva da più strade)
        if (p.seenSeqs.indexOf(seq) >= 0) return;
        p.seenSeqs.push(seq);
        if (p.seenSeqs.length > 64) p.seenSeqs.shift();
        var d = m.d;
        if (m.t === 'st') {
            if (seq <= p.stateSeq || !Array.isArray(d) || d.length < 12) return;
            p.stateSeq = seq;
            p.state = [num(d[0], 0, 2) | 0, num(d[1], -2000, 2000), num(d[2], -500, 500), num(d[3], -2000, 2000),
                       num(d[4], -10, 10), num(d[5], -2, 2), num(d[6], 0, 30), num(d[7], 0, 1) | 0, num(d[8], 0, 4) | 0,
                       num(d[9], 0, 1e6) | 0, num(d[10], 0, 1e6) | 0, num(d[11], 0, 1) | 0];
            p.stateNew = true;
        } else if (m.t === 'hb') {
            if (!d || typeof d !== 'object') return;
            var pr = {n: clean(d.n, MAX_NAME) || 'Viandante', l: num(d.l, 1, 99) | 0, f: d.f ? 1 : 0, o: num(d.o, 0, 31) | 0, w: clean(d.w, MAX_WHERE)};
            var line = 'P\t' + uid + '\t' + pr.n + '\t' + pr.l + '\t' + pr.f + '\t' + pr.o + '\t' + pr.w;
            if (!p.profile || p.profileLine !== line) push(line);
            p.profile = pr;
            p.profileLine = line;
        } else if (m.t === 'ch') {
            var text = clean(d, MAX_CHAT);
            if (!text) return;
            p.chat = p.chat.filter(function (x) { return now - x < 10000; });
            if (p.chat.length >= 6) return; // anti-spam: massimo 6 messaggi ogni 10 secondi
            p.chat.push(now);
            push('C\t' + uid + '\t' + text);
        } else if (m.t === 'em') {
            push('E\t' + uid + '\t' + (num(d, 0, 9) | 0));
        } else if (m.t === 'ex') {
            expeditionReceive(d);
        } else if (m.t === 'pi') {
            if (d && typeof d === 'object') send('po', {to: uid, n: num(d.n, 0, 1e9) | 0}, false);
        } else if (m.t === 'po') {
            if (d && d.to === net.uid && pings[d.n]) p.rtt = Math.round(performance.now() - pings[d.n]);
        }
    }

    var pings = {}, pingN = 0;

    function connect() {
        net.gen++;
        net.transports.forEach(function (tr) { tr.leave(); });
        net.transports = [];
        for (var id in net.peers) dropPeer(id);
        net.queue.length = 0;
        expedition = {round: -1, targets: [], players: Object.create(null)};
        net.rc = null;
        if (net.kind === 'off') return;
        var gen = net.gen, room = net.roomName;
        var handlers = {
            obj: function (m, via) { if (gen === net.gen) receive(m, via); },
            data: function (bytes, via) {
                if (gen !== net.gen || !net.rc) return;
                open(net.rc, bytes).then(function (m) { if (gen === net.gen) receive(m, via); }).catch(function () {});
            },
            up: function () { if (gen === net.gen) heartbeat(); },
            newPeer: function () { if (gen === net.gen) heartbeat(); }
        };
        if (net.kind === 'locale') {
            net.transports.push(localTransport(room, handlers));
            return;
        }
        if (net.kind === 'auto' || net.kind === 'nostr' || net.kind === 'torrent') {
            var tr = trysteroTransport(net.kind === 'torrent' ? 'torrent' : 'nostr', room, net.relays, handlers);
            if (tr) net.transports.push(tr);
        }
        if (net.kind === 'auto' || net.kind === 'mqtt') {
            roomCrypto(room).then(function (rc) {
                if (gen !== net.gen) return;
                net.rc = rc;
                net.brokers.forEach(function (url) { net.transports.push(mqttTransport(url, rc, handlers)); });
            }).catch(function (e) { console.warn('[rete] broker non disponibili', e); });
        }
    }

    // presenza, latenza e pulizia dei giocatori silenziosi
    setInterval(function () {
        var now = Date.now();
        if (now - net.lastHeartbeat >= HEARTBEAT_MS) heartbeat();
        for (var id in net.peers) if (now - net.peers[id].seen > PEER_TIMEOUT_MS) dropPeer(id);
        if (Object.keys(net.peers).length && net.transports.length) {
            pingN = (pingN + 1) % 1000;
            pings[pingN] = performance.now();
            delete pings[(pingN + 990) % 1000];
            send('pi', {n: pingN});
        }
    }, 1000);

    // -----------------------------------------------------------------------
    //  API per il gioco
    // -----------------------------------------------------------------------
    var api = {
        init: function (opts) {
            opts = opts || {};
            var k = String(opts.kind || 'auto').toLowerCase();
            net.kind = ['auto', 'mqtt', 'nostr', 'torrent', 'locale', 'off'].indexOf(k) >= 0 ? k : 'auto';
            var urls = function (s) { return String(s || '').split(',').map(function (x) { return x.trim(); }).filter(function (x) { return /^wss?:\/\//.test(x); }); };
            net.relays = urls(opts.relay);
            if (urls(opts.mqtt).length) net.brokers = urls(opts.mqtt);
            net.roomName = cleanRoom(opts.room);
            connect();
        },
        setRoom: function (name) {
            name = cleanRoom(name);
            if (name === net.roomName && net.transports.length) return;
            net.roomName = name;
            if (window.history && history.replaceState) {
                var u = new URL(location.href);
                u.hash = name === 'pubblica' ? '' : name;
                history.replaceState(null, '', u.toString());
            }
            connect();
        },
        room: function () { return net.roomName; },
        kind: function () { return net.kind; },
        selfId: function () { return net.uid; },
        // 0 disattivata, 1 connessione in corso, 2 in rete
        status: function () {
            if (!net.transports.length && net.kind === 'off') return 0;
            for (var i = 0; i < net.transports.length; i++) if (net.transports[i].up()) return 2;
            return Object.keys(net.peers).length ? 2 : 1;
        },
        peerCount: function () { return Object.keys(net.peers).length; },
        rtt: function (id) { var p = net.peers[id]; return p ? p.rtt : -1; },
        via: function (id) { var p = net.peers[id]; return p ? p.via : ''; },
        inviteLink: function () {
            var u = new URL(location.href);
            u.hash = net.roomName === 'pubblica' ? '' : net.roomName;
            var q = new URLSearchParams();
            if (net.kind !== 'auto') q.set('rete', net.kind);
            if (net.relays.length) q.set('relay', net.relays.join(','));
            if (net.brokers.join(',') !== MQTT_BROKERS.join(',')) q.set('mqtt', net.brokers.join(','));
            u.search = q.toString();
            return u.toString();
        },
        setProfile: function (json) {
            if (json === net.profileJson) return;
            net.profileJson = json;
            try { net.profile = JSON.parse(json); } catch (e) { return; }
            heartbeat();
        },
        sendState: function (s) {
            var now = Date.now(), last = net.lastState;
            var changed = !last || s[9] !== last[9] || s[10] !== last[10] || s[11] !== last[11] || s[0] !== last[0] || s[7] !== last[7];
            var moving = s[11] && (s[6] > 0.05 || !last || Math.abs(s[1] - last[1]) + Math.abs(s[3] - last[3]) > 0.02 || Math.abs(s[4] - last[4]) > 0.01);
            var interval = moving ? STATE_MS : 1000;
            if (!changed && now - net.lastStateSent < interval) return;
            for (var i = 1; i < 7; i++) s[i] = Math.round(s[i] * 100) / 100;
            net.lastState = s;
            net.lastStateSent = now;
            if (!Object.keys(net.peers).length) return;
            // se tutti sono raggiungibili direttamente, la posizione non passa dai broker
            send('st', s, !changed && allDirect());
        },
        sendChat: function (text) {
            text = clean(text, MAX_CHAT);
            if (text) send('ch', text);
        },
        sendEmote: function (e) { send('em', e | 0); },
        expedition: expeditionView,
        expeditionPhoto: function (key) {
            var e = expeditionCurrent();
            if (net.kind === 'off' || e.targets.indexOf(key) < 0) return false;
            var p = e.players[net.uid];
            if (!p) p = e.players[net.uid] = {name: 'Viandante', photos: Object.create(null)};
            if (p.photos[key] !== undefined) return false;
            p.name = clean(net.profile && net.profile.n, MAX_NAME) || 'Viandante';
            p.photos[key] = Date.now();
            expeditionBroadcast();
            return true;
        },
        // eventi dall'ultima lettura, una riga ciascuno
        poll: function () {
            for (var id in net.peers) {
                var p = net.peers[id];
                if (p.stateNew) { p.stateNew = false; push('S\t' + id + '\t' + p.state.join(' ')); }
            }
            if (!net.queue.length) return '';
            var out = net.queue.join('\n');
            net.queue.length = 0;
            return out;
        },
        bye: function () { try { send('by', 0); } catch (e) {} },
        debug: function () {
            return {
                kind: net.kind, room: net.roomName, uid: net.uid, status: api.status(), self: net.lastState,
                transports: net.transports.map(function (t) { return t.name + (t.up() ? ':su' : ':giu'); }),
                peers: JSON.parse(JSON.stringify(net.peers))
            };
        }
    };

    window.addEventListener('pagehide', function () { api.bye(); });
    window.DustfallNet = api;
})();
