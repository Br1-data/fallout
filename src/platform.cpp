// Differenze tra la versione desktop e quella web (browser, WebAssembly).
#include "platform.h"
#include "game.h"

#if DUSTFALL_WEB
#include <emscripten/emscripten.h>

// Il movimento del mouse viene accumulato in JavaScript: con il puntatore bloccato il browser
// invia molti eventi per fotogramma e raylib terrebbe solo l'ultimo.
EM_JS(void, js_platform_init, (), {
    if (Module.dfLookInit) return;
    Module.dfLookInit = true;
    Module.dfLook = Module.dfLook || {x: 0, y: 0};
    document.addEventListener('mousemove', function(e) {
        if (!document.pointerLockElement) return;
        if (Math.abs(e.movementX) > 500 || Math.abs(e.movementY) > 500) return; // scatti spuri di alcuni browser
        Module.dfLook.x += e.movementX;
        Module.dfLook.y += e.movementY;
    });
});

EM_JS(void, js_take_look, (float* out), {
    var l = Module.dfLook || {x: 0, y: 0};
    HEAPF32[out >> 2] = l.x;
    HEAPF32[(out >> 2) + 1] = l.y;
    l.x = 0;
    l.y = 0;
});

EM_JS(int, js_pointer_locked, (), { return document.pointerLockElement ? 1 : 0; });
EM_JS(int, js_is_fullscreen, (), { return document.fullscreenElement ? 1 : 0; });
EM_JS(void, js_set_fullscreen, (int on), {
    if (on) Module.dfWantFullscreen = true; // richiede un gesto dell'utente: viene eseguito al prossimo clic o tasto
    else if (document.fullscreenElement) document.exitFullscreen();
});
EM_JS(void, js_sync_storage, (), { if (Module.dfSyncStorage) Module.dfSyncStorage(); });
EM_JS(void, js_status, (const char* t), { if (Module.dfStatus) Module.dfStatus(UTF8ToString(t)); });
EM_JS(void, js_ready, (), { if (Module.dfReady) Module.dfReady(); });
EM_JS(int, js_is_touch, (), { return Module.dfTouch ? 1 : 0; });
EM_JS(void, js_touch_mode, (int m), { if (Module.dfTouchMode) Module.dfTouchMode(m); });
EM_JS(void, js_ask_text, (const char* kind, const char* title, const char* init, int maxLen), {
    if (Module.dfAskText) Module.dfAskText(UTF8ToString(kind), UTF8ToString(title), UTF8ToString(init), maxLen);
});
EM_JS(char*, js_take_text, (const char* kind), {
    var r = Module.dfTextResults, k = UTF8ToString(kind);
    if (!r || !(k in r)) return 0;
    var s = r[k];
    delete r[k];
    var n = lengthBytesUTF8(s) + 1, p = _malloc(n);
    stringToUTF8(s, p, n);
    return p;
});
// dimensione della tela in pixel reali: sui telefoni si disegna alla densità dello schermo
EM_JS(void, js_canvas_target, (int* out), {
    var k = Module.dfRenderScale || 1;
    HEAP32[out >> 2] = Math.max(1, Math.round(window.innerWidth * k));
    HEAP32[(out >> 2) + 1] = Math.max(1, Math.round(window.innerHeight * k));
});

static Vector2 sLook{0, 0};

void PlatformInit() { js_platform_init(); }

void PlatformFrame() {
    float d[2];
    js_take_look(d);
    sLook = Vector2{d[0], d[1]};
    int sz[2];
    js_canvas_target(sz);
    if (sz[0] != GetScreenWidth() || sz[1] != GetScreenHeight()) SetWindowSize(sz[0], sz[1]);
}

std::string DataDir() { return "/dustfall/"; }
void PlatformSyncStorage() { js_sync_storage(); }
Vector2 LookDelta() { return sLook; }

bool PlatformPointerLocked() { return js_pointer_locked() != 0; }
void PlatformSetFullscreen(bool on) { js_set_fullscreen(on ? 1 : 0); }
bool PlatformIsFullscreen() { return js_is_fullscreen() != 0; }
void PlatformStatus(const char* text) { js_status(text); }
void PlatformReady() { js_ready(); }
const char* GlslHeader() { return "#version 300 es\nprecision highp float;\nprecision highp int;\n"; }

bool PlatformTouch() { return js_is_touch() != 0; }

void PlatformSetTouchMode(int mode) {
    static int last = -1;
    if (mode == last) return;
    last = mode;
    js_touch_mode(mode);
}

void PlatformAskText(const char* kind, const char* title, const std::string& initial, int maxLen) { js_ask_text(kind, title, initial.c_str(), maxLen); }

bool PlatformTakeText(const char* kind, std::string& out) {
    char* p = js_take_text(kind);
    if (!p) return false;
    out = p;
    free(p);
    return true;
}

#else

void PlatformInit() {}
void PlatformFrame() {}
std::string DataDir() { return GetApplicationDirectory(); }
void PlatformSyncStorage() {}
Vector2 LookDelta() { return GetMouseDelta(); }
bool PlatformPointerLocked() { return IsCursorHidden(); }

void PlatformSetFullscreen(bool on) {
    bool isFull = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (on != isFull) ToggleBorderlessWindowed();
}

bool PlatformIsFullscreen() { return IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE); }
void PlatformStatus(const char*) {}
void PlatformReady() {}
const char* GlslHeader() { return "#version 330\n"; }
bool PlatformTouch() { return false; }
void PlatformSetTouchMode(int) {}
void PlatformAskText(const char*, const char*, const std::string&, int) {}
bool PlatformTakeText(const char*, std::string&) { return false; }

#endif
