// DUSTFALL - rete multigiocatore nel browser.
//
// Non c'è nessun server di gioco: i giocatori della stessa stanza si collegano direttamente tra loro
// con WebRTC. Per trovarsi usano relay pubblici (Nostr, oppure tracker BitTorrent) tramite Trystero,
// quindi il sito può stare su un hosting statico come GitHub Pages.
// Con ?rete=locale le schede dello stesso browser si parlano con BroadcastChannel (utile per i test).
//
// Il gioco (C++) chiama DustfallNet.* e ogni fotogramma legge gli eventi con poll().
(function () {
    'use strict';

    var APP_ID = 'dustfall-terre-bruciate-v1';
    var PROTO = 1;
    var STATE_MS = 80;          // ~12 aggiornamenti al secondo
    var PEER_TIMEOUT_MS = 15000;
    var MAX_NAME = 18, MAX_CHAT = 140, MAX_WHERE = 40;

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

    // -----------------------------------------------------------------------
    //  Trasporti
    // -----------------------------------------------------------------------
    function trysteroTransport(kind, roomName, relays, handlers) {
        var lib = window.DustfallP2P && window.DustfallP2P[kind];
        if (!lib) throw new Error('libreria P2P non disponibile');
        var relayConfig = {warnOnRelayFailure: false};
        if (relays.length) relayConfig.urls = relays; // relay propri (?relay=wss://...)
        var room = lib.joinRoom({appId: APP_ID, relayConfig: relayConfig}, roomName, {
            onJoinError: function (d) { console.warn('[rete] connessione non riuscita con', d && d.peerId, d && d.error); }
        });
        var actions = {};
        ['st', 'pr', 'ch', 'em', 'by'].forEach(function (t) {
            var a = room.makeAction(t);
            a.onMessage = function (data, meta) { handlers.message(t, data, meta && meta.peerId); };
            actions[t] = a;
        });
        room.onPeerJoin = function (id) { handlers.join(id); };
        room.onPeerLeave = function (id) { handlers.leave(id); };
        return {
            selfId: lib.selfId,
            send: function (t, data, target) {
                var p = actions[t].send(data, target ? {target: target} : undefined);
                if (p && p.catch) p.catch(function () {});
            },
            relaysUp: function () {
                try {
                    var socks = lib.getRelaySockets ? lib.getRelaySockets() : {};
                    for (var k in socks) if (socks[k] && socks[k].readyState === 1) return true;
                } catch (e) {}
                return false;
            },
            ping: function (id) { return room.ping(id); },
            leave: function () { try { room.leave(); } catch (e) {} }
        };
    }

    function localTransport(roomName, handlers) {
        var selfId = Math.random().toString(36).slice(2, 12);
        var ch = new BroadcastChannel('dustfall-' + APP_ID + '-' + roomName);
        var known = {};
        function post(t, data, target) { ch.postMessage({f: selfId, to: target || null, t: t, d: data}); }
        ch.onmessage = function (ev) {
            var m = ev.data || {};
            if (!m.f || m.f === selfId || (m.to && m.to !== selfId)) return;
            if (m.t === 'hi') {
                if (!known[m.f]) { known[m.f] = true; handlers.join(m.f); post('hi', 0, m.f); }
                return;
            }
            if (m.t === 'by') { delete known[m.f]; handlers.leave(m.f); return; }
            if (!known[m.f]) { known[m.f] = true; handlers.join(m.f); }
            handlers.message(m.t, m.d, m.f);
        };
        post('hi', 0);
        return {
            selfId: selfId,
            send: post,
            relaysUp: function () { return true; },
            ping: function () { return Promise.resolve(1); },
            leave: function () { post('by', 0); ch.close(); }
        };
    }

    // -----------------------------------------------------------------------
    //  Stato della rete
    // -----------------------------------------------------------------------
    var net = {
        kind: 'nostr',
        relays: [],
        roomName: 'pubblica',
        tr: null,
        error: '',
        peers: {},          // id -> {state, stateNew, profile, seen, chat: [tempi], rtt}
        queue: [],          // righe di eventi per il gioco
        profileJson: '',
        lastStateSent: 0,
        lastState: null,
        startedAt: 0
    };

    function push(line) {
        net.queue.push(line);
        if (net.queue.length > 400) net.queue.splice(0, net.queue.length - 400);
    }

    function peer(id) {
        var p = net.peers[id];
        if (!p) {
            p = net.peers[id] = {state: null, stateNew: false, profile: null, seen: Date.now(), chat: [], rtt: -1};
            push('J\t' + id);
            // presentati al nuovo arrivato
            if (net.profileJson && net.tr) net.tr.send('pr', JSON.parse(net.profileJson), id);
            if (net.lastState && net.tr) net.tr.send('st', net.lastState, id);
        }
        p.seen = Date.now();
        return p;
    }

    function dropPeer(id) {
        if (!net.peers[id]) return;
        delete net.peers[id];
        push('L\t' + id);
    }

    var handlers = {
        join: function (id) { if (id) peer(id); },
        leave: function (id) { dropPeer(id); },
        message: function (t, d, id) {
            if (!id) return;
            if (t === 'by') { dropPeer(id); return; }
            var p = peer(id);
            if (t === 'st') {
                if (!Array.isArray(d) || d.length < 12) return;
                var s = [num(d[0], 0, 2) | 0, num(d[1], -2000, 2000), num(d[2], -500, 500), num(d[3], -2000, 2000),
                         num(d[4], -10, 10), num(d[5], -2, 2), num(d[6], 0, 30), num(d[7], 0, 1) | 0, num(d[8], 0, 3) | 0,
                         num(d[9], 0, 1e6) | 0, num(d[10], 0, 1e6) | 0, num(d[11], 0, 1) | 0];
                p.state = s;
                p.stateNew = true;
            } else if (t === 'pr') {
                if (!d || typeof d !== 'object') return;
                var pr = {n: clean(d.n, MAX_NAME) || 'Viandante', l: num(d.l, 1, 99) | 0, f: d.f ? 1 : 0, o: num(d.o, 0, 31) | 0,
                          w: clean(d.w, MAX_WHERE), v: num(d.v, 0, 1000) | 0};
                p.profile = pr;
                push('P\t' + id + '\t' + pr.n + '\t' + pr.l + '\t' + pr.f + '\t' + pr.o + '\t' + pr.w);
            } else if (t === 'ch') {
                var text = clean(d, MAX_CHAT);
                if (!text) return;
                var now = Date.now();
                p.chat = p.chat.filter(function (x) { return now - x < 10000; });
                if (p.chat.length >= 6) return; // anti-spam: massimo 6 messaggi ogni 10 secondi
                p.chat.push(now);
                push('C\t' + id + '\t' + text);
            } else if (t === 'em') {
                push('E\t' + id + '\t' + (num(d, 0, 9) | 0));
            }
        }
    };

    function connect() {
        if (net.tr) { try { net.tr.send('by', 0); } catch (e) {} net.tr.leave(); }
        net.tr = null;
        net.error = '';
        for (var id in net.peers) dropPeer(id);
        net.startedAt = Date.now();
        if (net.kind === 'off') return;
        try {
            net.tr = net.kind === 'locale' ? localTransport(net.roomName, handlers) : trysteroTransport(net.kind, net.roomName, net.relays, handlers);
        } catch (e) {
            net.error = String(e && e.message || e);
            console.warn('[rete]', net.error);
        }
    }

    // pulizia dei giocatori silenziosi e misura della latenza
    setInterval(function () {
        var now = Date.now();
        for (var id in net.peers) {
            var p = net.peers[id];
            if (now - p.seen > PEER_TIMEOUT_MS) { dropPeer(id); continue; }
            if (net.tr && net.tr.ping) {
                (function (pid, pp) {
                    var r = net.tr.ping(pid);
                    if (r && r.then) r.then(function (ms) { pp.rtt = Math.round(ms); }).catch(function () {});
                })(id, p);
            }
        }
    }, 3000);

    // -----------------------------------------------------------------------
    //  API per il gioco
    // -----------------------------------------------------------------------
    var api = {
        init: function (opts) {
            opts = opts || {};
            var k = String(opts.kind || 'nostr').toLowerCase();
            net.kind = (k === 'torrent' || k === 'locale' || k === 'off') ? k : 'nostr';
            net.relays = String(opts.relay || '').split(',').map(function (s) { return s.trim(); }).filter(function (s) { return /^wss?:\/\//.test(s); });
            if (typeof RTCPeerConnection === 'undefined' && net.kind !== 'locale') net.kind = 'off';
            net.roomName = cleanRoom(opts.room);
            connect();
        },
        setRoom: function (name) {
            name = cleanRoom(name);
            if (name === net.roomName && net.tr) return;
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
        selfId: function () { return net.tr ? net.tr.selfId : ''; },
        // 0 disattivata, 1 connessione in corso, 2 in rete
        status: function () {
            if (!net.tr) return 0;
            return net.tr.relaysUp() || Object.keys(net.peers).length ? 2 : 1;
        },
        peerCount: function () { return Object.keys(net.peers).length; },
        rtt: function (id) { var p = net.peers[id]; return p ? p.rtt : -1; },
        inviteLink: function () {
            var u = new URL(location.href);
            u.hash = net.roomName === 'pubblica' ? '' : net.roomName;
            var q = new URLSearchParams();
            if (net.kind !== 'nostr') q.set('rete', net.kind);
            if (net.relays.length) q.set('relay', net.relays.join(','));
            u.search = q.toString();
            return u.toString();
        },
        setProfile: function (json) {
            if (json === net.profileJson) return;
            net.profileJson = json;
            if (net.tr) net.tr.send('pr', JSON.parse(json));
        },
        sendState: function (s) {
            var now = Date.now();
            var changed = !net.lastState || s[9] !== net.lastState[9] || s[10] !== net.lastState[10] || s[11] !== net.lastState[11] || s[0] !== net.lastState[0];
            var interval = s[11] ? STATE_MS : 1000;
            if (!changed && now - net.lastStateSent < interval) return;
            for (var i = 1; i < 7; i++) s[i] = Math.round(s[i] * 100) / 100;
            net.lastState = s;
            net.lastStateSent = now;
            if (net.tr && Object.keys(net.peers).length) net.tr.send('st', s);
        },
        sendChat: function (text) {
            text = clean(text, MAX_CHAT);
            if (text && net.tr) net.tr.send('ch', text);
        },
        sendEmote: function (e) { if (net.tr) net.tr.send('em', e | 0); },
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
        bye: function () { if (net.tr) { try { net.tr.send('by', 0); } catch (e) {} } },
        debug: function () { return {kind: net.kind, room: net.roomName, status: api.status(), self: net.lastState, peers: JSON.parse(JSON.stringify(net.peers)), error: net.error}; }
    };

    window.addEventListener('pagehide', function () { api.bye(); });
    window.DustfallNet = api;
})();
