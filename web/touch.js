// DUSTFALL - comandi touch per telefoni e tablet, e finestra per scrivere testo.
//
// I comandi sono un livello HTML sopra il gioco: generano gli stessi eventi di tastiera e mouse
// che il gioco riceverebbe da un computer, mentre il trascinamento del dito a destra muove la
// visuale (si somma al movimento del mouse letto da platform.cpp).
// Il gioco comunica lo stato corrente con Module.dfTouchMode(modo).
(function () {
    'use strict';

    var M = window.Module;
    var q = new URLSearchParams(location.search);
    var touch = q.get('touch') === '1' ||
        (q.get('touch') !== '0' && window.matchMedia && matchMedia('(pointer: coarse)').matches);
    M.dfTouch = touch;
    // sui telefoni si disegna alla densità reale dello schermo (testo nitido), al massimo 2x
    M.dfRenderScale = touch ? Math.min(window.devicePixelRatio || 1, 2) : 1;
    M.dfLook = M.dfLook || {x: 0, y: 0};
    if (touch) document.documentElement.classList.add('touch');

    // -----------------------------------------------------------------------
    //  Finestra per scrivere (chat, nome del personaggio, stanza)
    // -----------------------------------------------------------------------
    M.dfTextResults = {};
    var box = document.createElement('div');
    box.id = 'textbox';
    box.innerHTML = '<form><label for="tb-input" id="tb-title"></label><input id="tb-input" autocomplete="off" autocapitalize="sentences" enterkeyhint="send">' +
        '<div class="tb-row"><button type="button" id="tb-cancel">Annulla</button><button type="submit" id="tb-ok">OK</button></div></form>';
    document.body.appendChild(box);
    var tbForm = box.querySelector('form'), tbInput = box.querySelector('input'), tbTitle = box.querySelector('#tb-title');
    var tbKind = null;
    function closeText(value) {
        if (tbKind !== null) M.dfTextResults[tbKind] = value;
        tbKind = null;
        box.classList.remove('open');
        tbInput.blur();
    }
    M.dfAskText = function (kind, title, init, maxLen) {
        tbKind = kind;
        tbTitle.textContent = title;
        tbInput.maxLength = maxLen || 140;
        tbInput.value = init || '';
        box.classList.add('open');
        setTimeout(function () { tbInput.focus(); tbInput.select(); }, 30);
    };
    M.dfTextOpen = function () { return tbKind !== null; };
    tbForm.addEventListener('submit', function (e) { e.preventDefault(); closeText(tbInput.value.trim()); });
    box.querySelector('#tb-cancel').addEventListener('click', function () { closeText(''); });
    tbInput.addEventListener('keydown', function (e) { if (e.key === 'Escape') { e.preventDefault(); closeText(''); } });

    if (!touch) {
        M.dfTouchMode = function () {};
        return;
    }

    // -----------------------------------------------------------------------
    //  Eventi sintetici per il gioco (GLFW legge keyCode, che va impostato a mano)
    // -----------------------------------------------------------------------
    var KEYS = {
        '1': [49, 'Digit1', '1'], '2': [50, 'Digit2', '2'], '3': [51, 'Digit3', '3'],
        '4': [52, 'Digit4', '4'], '5': [53, 'Digit5', '5'], '6': [54, 'Digit6', '6'], '7': [55, 'Digit7', '7'],
        w: [87, 'KeyW', 'w'], a: [65, 'KeyA', 'a'], s: [83, 'KeyS', 's'], d: [68, 'KeyD', 'd'],
        e: [69, 'KeyE', 'e'], q: [81, 'KeyQ', 'q'], c: [67, 'KeyC', 'c'], b: [66, 'KeyB', 'b'],
        g: [71, 'KeyG', 'g'], h: [72, 'KeyH', 'h'], f: [70, 'KeyF', 'f'], v: [86, 'KeyV', 'v'],
        l: [76, 'KeyL', 'l'], z: [90, 'KeyZ', 'z'], p: [80, 'KeyP', 'p'], x: [88, 'KeyX', 'x'],
        r: [82, 'KeyR', 'r'], space: [32, 'Space', ' '], enter: [13, 'Enter', 'Enter'],
        esc: [27, 'Escape', 'Escape'], tab: [9, 'Tab', 'Tab'], shift: [16, 'ShiftLeft', 'Shift'],
        up: [38, 'ArrowUp', 'ArrowUp'], down: [40, 'ArrowDown', 'ArrowDown'],
        left: [37, 'ArrowLeft', 'ArrowLeft'], right: [39, 'ArrowRight', 'ArrowRight']
    };
    function sendKey(name, down) {
        var k = KEYS[name];
        var ev = new KeyboardEvent(down ? 'keydown' : 'keyup', {key: k[2], code: k[1], bubbles: true, cancelable: true});
        Object.defineProperty(ev, 'keyCode', {get: function () { return k[0]; }});
        Object.defineProperty(ev, 'which', {get: function () { return k[0]; }});
        window.dispatchEvent(ev);
    }
    // ogni tasto resta premuto almeno qualche fotogramma, e più sorgenti possono tenerlo premuto
    var held = {}, downAt = {};
    var MIN_HOLD = 70;
    function press(name) {
        held[name] = (held[name] || 0) + 1;
        if (held[name] === 1) { downAt[name] = performance.now(); sendKey(name, true); }
    }
    function release(name) {
        if (!held[name]) return;
        held[name]--;
        if (held[name] > 0) return;
        var wait = MIN_HOLD - (performance.now() - downAt[name]);
        if (wait > 0) setTimeout(function () { if (!held[name]) sendKey(name, false); }, wait);
        else sendKey(name, false);
    }
    function releaseAll() {
        for (var k in held) if (held[k]) { held[k] = 1; release(k); }
        for (var b in mouseHeld) if (mouseHeld[b]) mouseRelease(+b);
    }

    function sendMouse(button, down) {
        var c = M.canvas, r = c.getBoundingClientRect();
        c.dispatchEvent(new MouseEvent(down ? 'mousedown' : 'mouseup', {
            button: button, buttons: down ? (button === 0 ? 1 : 2) : 0,
            clientX: r.left + r.width / 2, clientY: r.top + r.height / 2, bubbles: true, cancelable: true
        }));
    }
    var mouseHeld = {};
    function mousePress(b) { if (!mouseHeld[b]) { mouseHeld[b] = performance.now(); sendMouse(b, true); } }
    function mouseRelease(b) {
        if (!mouseHeld[b]) return;
        var wait = MIN_HOLD - (performance.now() - mouseHeld[b]);
        mouseHeld[b] = 0;
        if (wait > 0) setTimeout(function () { if (!mouseHeld[b]) sendMouse(b, false); }, wait);
        else sendMouse(b, false);
    }
    function wheel(dir) {
        M.canvas.dispatchEvent(new WheelEvent('wheel', {deltaY: dir > 0 ? -100 : 100, deltaMode: 0, bubbles: true, cancelable: true}));
    }

    // -----------------------------------------------------------------------
    //  Interfaccia
    // -----------------------------------------------------------------------
    var root = document.createElement('div');
    root.id = 'touch';
    // b: [etichetta, classi, azione]  azione: 'k:tasto' | 'm:pulsante' | 'w:verso' | 'a:speciale'
    var BUTTONS = [
        ['USA', 'big use play', 'm:0'], ['E', 'mid act play', 'k:e'], ['SALTA', 'mid jump play', 'k:space'],
        ['ACCOV.', 'mid crouch play', 'k:c'], ['ZOOM', 'mid zoom play', 'm:2'],
        ['&#9776;', 'sm r0 c0 play scan', 'k:esc'], ['PIP-BOY', 'sm r1 c0 play', 'k:tab'], ['OSSERVA', 'sm r2 c0 play', 'k:q'],
        ['CHAT', 'sm r3 c0 play', 'a:chat'], ['ATTREZZO', 'sm r0 c1 play', 'w:1'], ['ESCA', 'sm r1 c1 play', 'k:b'],
        ['BARATTOLO', 'sm r2 c1 play', 'k:g'], ['REGISTR.', 'sm r3 c1 play', 'k:7'], ['KIT', 'sm r0 c2 play', 'k:h'],
        ['SCANNER', 'sm r1 c2 play', 'k:5'], ['UTENSILE', 'sm r2 c2 play', 'k:6'], ['LUCE', 'sm r3 c2 play', 'k:l'],
        ['GIOCATORI', 'sm r4 c0 play', 'k:p'],
        ['CHIUDI', 'mid close scan', 'k:q'],
        ['GIRA', 'big use pick', 'k:space'], ['ESCI', 'mid close pick', 'k:esc'],
        ['&#9650;', 'pad pu menu pip', 'k:up'], ['&#9660;', 'pad pd menu pip', 'k:down'],
        ['&#9664;', 'pad pl menu pip', 'k:left'], ['&#9654;', 'pad pr menu pip', 'k:right'],
        ['OK', 'pad ok menu pip', 'k:enter'], ['INDIETRO', 'pad back menu', 'k:esc'], ['CHIUDI', 'pad back pip', 'k:tab'],
        ['SCHEDA &#9664;', 'pad tl pip', 'k:q'], ['SCHEDA &#9654;', 'pad tr pip', 'k:e']
    ];
    var html = '<div id="t-look"></div><div id="t-move"></div><div id="t-stick"><div></div></div>';
    BUTTONS.forEach(function (b, i) { html += '<button type="button" class="tb ' + b[1] + '" data-i="' + i + '">' + b[0] + '</button>'; });
    root.innerHTML = html;
    document.body.appendChild(root);

    function act(spec, down) {
        var t = spec[0], v = spec.slice(2);
        if (t === 'k') (down ? press : release)(v);
        else if (t === 'm') (down ? mousePress : mouseRelease)(+v);
        else if (t === 'w' && down) wheel(+v);
        else if (t === 'a' && down && v === 'chat') M.dfAskText('chat', 'Messaggio per gli altri giocatori', '', 140);
    }
    Array.prototype.forEach.call(root.querySelectorAll('button.tb'), function (el) {
        var spec = BUTTONS[+el.dataset.i][2];
        var active = null;
        el.addEventListener('pointerdown', function (e) {
            e.preventDefault();
            if (active !== null) return;
            active = e.pointerId;
            try { el.setPointerCapture(e.pointerId); } catch (err) {}
            el.classList.add('on');
            act(spec, true);
        });
        function up(e) {
            if (active === null || (e && e.pointerId !== active)) return;
            active = null;
            el.classList.remove('on');
            act(spec, false);
        }
        el.addEventListener('pointerup', up);
        el.addEventListener('pointercancel', up);
        el.addEventListener('lostpointercapture', up);
        el.addEventListener('contextmenu', function (e) { e.preventDefault(); });
    });

    // --- joystick (compare dove si appoggia il pollice) e visuale (dito a destra).
    // Si usano gli eventi touch, seguendo ogni dito con il suo identificatore.
    var moveZone = root.querySelector('#t-move'), stick = root.querySelector('#t-stick'), knob = stick.firstChild;
    var lookZone = root.querySelector('#t-look');
    var moveId = null, ox = 0, oy = 0, dirKeys = {};
    var looks = {};
    var R = 56, LOOK_GAIN = 1.7;
    function setDir(want) {
        ['w', 'a', 's', 'd', 'shift'].forEach(function (k) {
            if (want[k] && !dirKeys[k]) { dirKeys[k] = true; press(k); }
            else if (!want[k] && dirKeys[k]) { dirKeys[k] = false; release(k); }
        });
    }
    function stickMove(x, y) {
        var dx = x - ox, dy = y - oy, len = Math.sqrt(dx * dx + dy * dy);
        var k = len > R ? R / len : 1;
        knob.style.transform = 'translate(calc(-50% + ' + dx * k + 'px), calc(-50% + ' + dy * k + 'px))';
        if (len < R * 0.22) { setDir({}); return; }
        // direzione a 8 vie; spingendo oltre il bordo in avanti si corre
        var ux = dx / len, uy = dy / len;
        setDir({w: uy < -0.38, s: uy > 0.38, a: ux < -0.38, d: ux > 0.38, shift: len > R * 1.35 && uy < -0.5});
    }
    function stickEnd() {
        moveId = null;
        stick.classList.remove('on');
        setDir({});
    }
    moveZone.addEventListener('touchstart', function (e) {
        e.preventDefault();
        var t = e.changedTouches[0];
        if (moveId !== null || !t) return;
        moveId = t.identifier;
        ox = t.clientX; oy = t.clientY;
        stick.style.left = ox + 'px';
        stick.style.top = oy + 'px';
        knob.style.transform = 'translate(-50%, -50%)';
        stick.classList.add('on');
    }, {passive: false});
    lookZone.addEventListener('touchstart', function (e) {
        e.preventDefault();
        for (var i = 0; i < e.changedTouches.length; i++) {
            var t = e.changedTouches[i];
            looks[t.identifier] = {x: t.clientX, y: t.clientY};
        }
    }, {passive: false});
    // i movimenti arrivano all'elemento dove il dito è stato appoggiato: basta ascoltarli sul livello
    root.addEventListener('touchmove', function (e) {
        e.preventDefault();
        for (var i = 0; i < e.changedTouches.length; i++) {
            var t = e.changedTouches[i];
            if (t.identifier === moveId) stickMove(t.clientX, t.clientY);
            var p = looks[t.identifier];
            if (p) {
                M.dfLook.x += (t.clientX - p.x) * LOOK_GAIN;
                M.dfLook.y += (t.clientY - p.y) * LOOK_GAIN;
                p.x = t.clientX; p.y = t.clientY;
            }
        }
    }, {passive: false});
    function touchEnd(e) {
        for (var i = 0; i < e.changedTouches.length; i++) {
            var t = e.changedTouches[i];
            if (t.identifier === moveId) stickEnd();
            delete looks[t.identifier];
        }
    }
    root.addEventListener('touchend', touchEnd);
    root.addEventListener('touchcancel', touchEnd);

    // --- modi: 0 nessuno, 1 menu, 2 gioco, 3 scanner, 4 scasso, 5 Pip-Boy
    var MODES = ['none', 'menu', 'play', 'scan', 'pick', 'pip'];
    var mode = -1;
    M.dfTouchMode = function (m) {
        if (m === mode) return;
        mode = m;
        releaseAll();
        stickEnd();
        looks = {};
        root.className = 'mode-' + (MODES[m] || 'none');
    };
    document.addEventListener('visibilitychange', function () { if (document.hidden) { releaseAll(); stickEnd(); looks = {}; } });
})();
