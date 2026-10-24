// Multigiocatore: gli altri viandanti nelle Terre Bruciate.
// La rete vera e propria è in web/net.js (broker MQTT e WebRTC); qui ci sono gli avatar, la chat,
// i nomi sopra le teste, il menu della stanza e i segnalini sulla mappa.
#include "net.h"
#include "platform.h"

static std::vector<RemotePlayer> sRemotes;

struct ChatLine { std::string text; Color col; float t; };
static std::vector<ChatLine> sChat;
static std::string sChatInput;
static int sStatus = 0;
static bool sAvail = false;
static bool sShowList = false;
static int sWaveN = 0;
static float sLocalWave = 0;

static const Color CHAT_OTHER = {255, 225, 150, 255};
static const Color CHAT_SYS = {150, 200, 160, 255};

#if DUSTFALL_WEB
#include <emscripten/emscripten.h>

EM_JS(int, js_net_init, (), {
    if (typeof DustfallNet === 'undefined') return 0;
    if (!Module.dfNetStarted) {
        var q = new URLSearchParams(location.search);
        var room = "";
        try { room = decodeURIComponent(location.hash.slice(1)); } catch (e) {}
        DustfallNet.init({kind: q.get("rete") || "auto", relay: q.get("relay") || "", mqtt: q.get("mqtt") || "", room: room});
        Module.dfNetStarted = true;
    }
    return 1;
});
EM_JS(void, js_net_profile, (const char* json), { DustfallNet.setProfile(UTF8ToString(json)); });
EM_JS(void, js_net_state, (const float* s), {
    var a = [];
    for (var i = 0; i < 12; i++) a.push(HEAPF32[(s >> 2) + i]);
    DustfallNet.sendState(a);
});
EM_JS(char*, js_net_poll, (), {
    var s = DustfallNet.poll();
    if (!s) return 0;
    var n = lengthBytesUTF8(s) + 1;
    var p = _malloc(n);
    stringToUTF8(s, p, n);
    return p;
});
EM_JS(char*, js_net_room, (), {
    var s = DustfallNet.room();
    var n = lengthBytesUTF8(s) + 1;
    var p = _malloc(n);
    stringToUTF8(s, p, n);
    return p;
});
EM_JS(void, js_net_chat, (const char* t), { DustfallNet.sendChat(UTF8ToString(t)); });
EM_JS(void, js_net_emote, (int e), { DustfallNet.sendEmote(e); });
EM_JS(int, js_net_status, (), { return DustfallNet.status(); });
EM_JS(int, js_net_rtt, (const char* id), { return DustfallNet.rtt(UTF8ToString(id)); });
EM_JS(int, js_net_direct, (const char* id), { return DustfallNet.via(UTF8ToString(id)) === 'diretto' ? 1 : 0; });
EM_JS(void, js_net_set_room, (const char* r), { DustfallNet.setRoom(UTF8ToString(r)); });
EM_JS(void, js_net_copy_invite, (), {
    var l = DustfallNet.inviteLink();
    var copy = function() { if (navigator.clipboard) navigator.clipboard.writeText(l).catch(function() {}); };
    // sul telefono si apre il menu Condividi (WhatsApp, Telegram...)
    if (Module.dfTouch && navigator.share) navigator.share({title: 'Dustfall', text: 'Giochiamo insieme a Dustfall!', url: l}).catch(copy);
    else copy();
});
EM_JS(char*, js_net_invite, (), {
    var s = DustfallNet.inviteLink();
    var n = lengthBytesUTF8(s) + 1;
    var p = _malloc(n);
    stringToUTF8(s, p, n);
    return p;
});

static std::string TakeJsString(char* p) {
    if (!p) return "";
    std::string s(p);
    free(p);
    return s;
}
#endif

bool NetAvailable() { return sAvail; }
int NetStatus() { return sStatus; }
const std::vector<RemotePlayer>& NetPlayers() { return sRemotes; }
float NetLocalWave() { return sLocalWave; }

static std::string NetRoom() {
#if DUSTFALL_WEB
    if (sAvail) return TakeJsString(js_net_room());
#endif
    return "";
}

// collegamento diretto (WebRTC) o tramite broker
static const char* NetVia(const std::string& id) {
#if DUSTFALL_WEB
    if (sAvail) return js_net_direct(id.c_str()) ? "diretto" : "via broker";
#endif
    return "";
}

static std::string NetInvite() {
#if DUSTFALL_WEB
    if (sAvail) return TakeJsString(js_net_invite());
#endif
    return "";
}

static int NetRtt(const std::string& id) {
#if DUSTFALL_WEB
    if (sAvail) return js_net_rtt(id.c_str());
#endif
    return -1;
}

static void AddChat(const std::string& text, Color col) {
    sChat.push_back({text, col, 0});
    if (sChat.size() > 40) sChat.erase(sChat.begin());
}

static bool LocalInGame() { return G.state != GS_MENU && G.state != GS_INTRO && G.state != GS_CREATE; }

#if DUSTFALL_WEB
static int sFlashN = 0;
static float sProfileT = 0;
static bool sPrevFlash = false;

static RemotePlayer* FindRemote(const std::string& id) {
    for (auto& r : sRemotes)
        if (r.id == id) return &r;
    return nullptr;
}

static RemotePlayer& GetRemote(const std::string& id) {
    if (RemotePlayer* r = FindRemote(id)) return *r;
    RemotePlayer r;
    r.id = id;
    sRemotes.push_back(r);
    return sRemotes.back();
}

// aspetto: tuta o abiti dal giocatore, carnagione e capelli ricavati dal nome
static void UpdateLook(RemotePlayer& r) {
    unsigned h = 2166136261u;
    for (char c : r.name) h = (h ^ (unsigned char)c) * 16777619u;
    static const Color skins[] = {Col(232, 190, 160), Col(215, 170, 140), Col(190, 140, 105), Col(150, 105, 75), Col(110, 75, 55)};
    static const Color hairs[] = {Col(40, 30, 25), Col(70, 50, 35), Col(120, 80, 40), Col(200, 170, 110), Col(150, 60, 35), Col(160, 160, 155)};
    r.look = Look();
    r.look.outfit = r.outfit;
    r.look.female = r.female;
    r.look.skin = skins[h % 5];
    r.look.hair = hairs[(h / 5) % 6];
    r.look.hairStyle = r.female ? 2 : ((h / 30) % 4 == 0 ? 3 : 1);
}

static std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t a = 0;
    while (true) {
        size_t b = s.find(sep, a);
        out.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

static std::string JsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c < 32) o += ' ';
        else o += (char)c;
    }
    return o + "\"";
}

static std::string PlayerWhere() {
    Player& P = G.player;
    if (P.zone != ZONE_WASTE) return W.zones[P.zone].name;
    float d;
    int li = NearestLocation(P.pos, &d);
    return li >= 0 && d < 120 ? W.locations[li].name : std::string("Terre Bruciate");
}

static void HandleEvent(const std::string& line) {
    std::vector<std::string> f = Split(line, '\t');
    if (f.size() < 2) return;
    const std::string& type = f[0];
    const std::string& id = f[1];
    if (type == "J") {
        GetRemote(id);
    } else if (type == "L") {
        RemotePlayer* r = FindRemote(id);
        if (r) {
            if (r->announced) AddChat(r->name + " ha lasciato le Terre Bruciate.", CHAT_SYS);
            sRemotes.erase(sRemotes.begin() + (r - &sRemotes[0]));
        }
    } else if (type == "P" && f.size() >= 7) {
        RemotePlayer& r = GetRemote(id);
        r.name = f[2];
        r.level = atoi(f[3].c_str());
        r.female = atoi(f[4].c_str()) != 0;
        r.outfit = Clamp(atoi(f[5].c_str()), 0, OF_DARK);
        r.where = f[6];
        UpdateLook(r);
        if (!r.announced) AddChat(r.name + " è nelle Terre Bruciate (" + r.where + ").", CHAT_SYS);
        r.announced = true;
    } else if (type == "S" && f.size() >= 3) {
        float v[12] = {};
        if (sscanf(f[2].c_str(), "%f %f %f %f %f %f %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11]) != 12) return;
        for (float x : v)
            if (!std::isfinite(x)) return;
        RemotePlayer& r = GetRemote(id);
        int zone = Clamp((int)v[0], 0, ZONE_COUNT - 1);
        Vector3 p = V3(Clamp(v[1], -WORLD_HALF, WORLD_HALF), Clamp(v[2], -100.0f, 200.0f), Clamp(v[3], -WORLD_HALF, WORLD_HALF));
        bool snap = !r.hasState || zone != r.zone || Vector3Distance(p, r.pos) > 12;
        r.zone = zone;
        r.netPos = p;
        r.netYaw = v[4];
        r.speed = Clamp(v[6], 0.0f, 20.0f);
        r.crouch = v[7] > 0.5f;
        r.tool = Clamp((int)v[8], 0, TOOL_COUNT - 1);
        int flashN = (int)v[9], waveN = (int)v[10];
        r.inGame = v[11] > 0.5f;
        bool near = r.inGame && zone == G.player.zone && Vector3Distance(p, G.player.pos) < 60 && LocalInGame();
        if (r.flashN >= 0 && flashN != r.flashN) {
            r.flashT = 0.25f;
            if (near) PlaySfx3D(SFX_SHUTTER, p + V3(0, 1.6f, 0), 0.7f);
        }
        if (r.waveN >= 0 && waveN != r.waveN) r.waveT = 2.4f;
        r.flashN = flashN;
        r.waveN = waveN;
        if (snap) {
            r.pos = p;
            r.yaw = r.netYaw;
            r.crouchT = r.crouch ? 1.0f : 0.0f;
        }
        r.hasState = true;
    } else if (type == "C" && f.size() >= 3) {
        RemotePlayer& r = GetRemote(id);
        AddChat(r.name + ": " + f[2], CHAT_OTHER);
        r.say = f[2];
        r.sayT = 7;
        if (LocalInGame()) PlaySfx(SFX_UI_MOVE, 0.4f, 1.3f);
    } else if (type == "E" && f.size() >= 3) {
        RemotePlayer& r = GetRemote(id);
        r.waveT = 2.4f;
        if (r.inGame && r.zone == G.player.zone && Vector3Distance(r.pos, G.player.pos) < 40)
            AddChat(r.name + " ti saluta.", CHAT_SYS);
    }
}

#endif

void NetInit() {
#if DUSTFALL_WEB
    sAvail = js_net_init() != 0;
#endif
}

void NetShutdown() {}

void NetUpdate(float dt) {
    for (auto& c : sChat) c.t += dt;
    if (sLocalWave > 0) sLocalWave -= dt;
    if (!sAvail) return;
#if DUSTFALL_WEB
    Player& P = G.player;
    bool inGame = LocalInGame();
    // profilo (nome, livello, aspetto, luogo) ogni secondo: la pagina lo invia solo se cambia
    sProfileT -= dt;
    if (sProfileT <= 0) {
        sProfileT = 1;
        std::string json = "{\"n\":" + JsonStr(P.name) + ",\"l\":" + std::to_string(P.level) + ",\"f\":" + (P.female ? "1" : "0") +
                           ",\"o\":" + std::to_string(ArmorOutfit()) + ",\"w\":" + JsonStr(inGame ? PlayerWhere() : std::string("Menu principale")) + ",\"v\":1}";
        js_net_profile(json.c_str());
    }
    bool flash = P.flash > 0;
    if (flash && !sPrevFlash) sFlashN++;
    sPrevFlash = flash;
    float st[12] = {(float)P.zone, P.pos.x, P.pos.y, P.pos.z, P.yaw, P.pitch, Vector3Length(V3(P.vel.x, 0, P.vel.z)), P.crouch ? 1.0f : 0.0f,
                    (float)P.tool, (float)sFlashN, (float)sWaveN, inGame ? 1.0f : 0.0f};
    js_net_state(st);
    std::string typed;
    if (PlatformTakeText("chat", typed) && !typed.empty()) {
        js_net_chat(typed.c_str());
        AddChat(G.player.name + ": " + typed, PIP);
    }
    std::string events = TakeJsString(js_net_poll());
    if (!events.empty())
        for (auto& line : Split(events, '\n')) HandleEvent(line);
    sStatus = js_net_status();
#endif
    // interpolazione verso l'ultima posizione ricevuta
    float k = 1.0f - expf(-dt * 12.0f);
    for (auto& r : sRemotes) {
        if (!r.hasState) continue;
        r.pos = Vector3Lerp(r.pos, r.netPos, k);
        r.yaw += AngleWrap(r.netYaw - r.yaw) * k;
        r.crouchT += ((r.crouch ? 1.0f : 0.0f) - r.crouchT) * Clamp01(dt * 8);
        r.anim += r.speed * dt * 2.1f;
        if (r.flashT > 0) r.flashT -= dt;
        if (r.waveT > 0) r.waveT -= dt;
        if (r.sayT > 0) r.sayT -= dt;
    }
}

static bool Visible(const RemotePlayer& r) {
    return r.hasState && r.inGame && r.zone == G.player.zone;
}

void NetDraw3D(Camera3D cam) {
    if (!LocalInGame()) return;
    for (auto& r : sRemotes) {
        if (!Visible(r) || Vector3Distance(r.pos, cam.position) > 260) continue;
        float hold = r.tool == TOOL_CAMERA ? 1.0f : (r.tool != TOOL_HANDS ? 0.4f : 0.0f);
        float action = 0;
        if (r.waveT > 0) {
            hold = 0;
            action = 0.2f + 0.15f * sinf(G.time * 12.0f);
        }
        DrawHumanoid(r.pos, r.yaw, r.look, r.anim, Clamp01(r.speed / 4.5f), hold, action, 0, r.tool, r.flashT > 0 ? 1.0f : 0.0f, 1.0f, false, r.crouchT);
    }
    for (auto& r : sRemotes)
        if (Visible(r) && Vector3Distance(r.pos, cam.position) < 50) DrawCylinder(r.pos + V3(0, 0.03f, 0), 0.42f, 0.42f, 0.01f, 10, Color{0, 0, 0, 70});
}

bool NetPlayKeys() {
    if (!sAvail) return false;
    if (IsKeyPressed(KEY_T) || IsKeyPressed(KEY_ENTER)) {
        sChatInput.clear();
        while (GetCharPressed() > 0) {
        }
        SetGameState(GS_CHAT);
        return true;
    }
    if (IsKeyPressed(KEY_Z)) {
        sWaveN++;
        sLocalWave = 2.4f;
#if DUSTFALL_WEB
        js_net_emote(1);
#endif
    }
    if (IsKeyPressed(KEY_P)) sShowList = !sShowList;
    return false;
}

// ---------------------------------------------------------------------------
//  Chat
// ---------------------------------------------------------------------------
static void TypeInto(std::string& s, size_t maxBytes) {
    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && s.size() + 4 < maxBytes) {
            if (ch < 0x80) s += (char)ch;
            else if (ch < 0x800) { s += (char)(0xC0 | (ch >> 6)); s += (char)(0x80 | (ch & 0x3F)); }
            else if (ch < 0x10000) { s += (char)(0xE0 | (ch >> 12)); s += (char)(0x80 | ((ch >> 6) & 0x3F)); s += (char)(0x80 | (ch & 0x3F)); }
        }
        ch = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !s.empty()) {
        while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
        if (!s.empty()) s.pop_back();
    }
}

void ChatUpdate() {
    TypeInto(sChatInput, 140);
    if (IsKeyPressed(KEY_ESCAPE)) { SetGameState(GS_PLAY); return; }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        size_t a = sChatInput.find_first_not_of(' ');
        if (a != std::string::npos) {
            std::string msg = sChatInput.substr(a);
#if DUSTFALL_WEB
            js_net_chat(msg.c_str());
#endif
            AddChat(G.player.name + ": " + msg, PIP);
            PlaySfx(SFX_UI_OK, 0.5f);
        }
        sChatInput.clear();
        SetGameState(GS_PLAY);
    }
}

// ---------------------------------------------------------------------------
//  HUD: nomi, chat, stato della rete, elenco dei giocatori
// ---------------------------------------------------------------------------
void NetDrawHud() {
    if (!sAvail) return;
    float S = UIS(), Wd = UIW();
    Player& P = G.player;
    Vector3 camF = Vector3Normalize(G.cam.target - G.cam.position);
    // nomi sopra le teste
    for (auto& r : sRemotes) {
        if (!Visible(r) || !LocalInGame()) continue;
        Vector3 head = r.pos + V3(0, 2.1f - r.crouchT * 0.4f, 0);
        Vector3 to = head - G.cam.position;
        float d = Vector3Length(to);
        if (d > 300 || Vector3DotProduct(to, camF) < 0.1f * d) continue;
        Vector2 sp = GetWorldToScreen(head, G.cam);
        float x = sp.x / S, y = sp.y / S;
        float a = d < 40 ? 1.0f : std::max(0.35f, 1.0f - (d - 40) / 220.0f);
        if (!LineOfSight(P.zone, G.cam.position, head)) a *= 0.45f;
        std::string tag = r.name + Fmt("  [%d]", r.level);
        if (d > 15) tag += Fmt("  %dm", (int)d);
        Vector2 m = UIMeasure(tag, 16);
        UIRect(x - m.x / 2 - 6, y - 22, m.x + 12, 22, Color{0, 0, 0, (unsigned char)(120 * a)});
        UIText(tag, x, y - 20, 16, ColAlpha(CHAT_OTHER, a), 1);
        if (r.sayT > 0 && d < 60) {
            float sa = a * Clamp01(r.sayT);
            auto lines = UIWrap(r.say, 15, 260);
            float yy = y - 26 - lines.size() * 19;
            float w = 0;
            for (auto& l : lines) w = std::max(w, UIMeasure(l, 15).x);
            UIRect(x - w / 2 - 8, yy - 3, w + 16, lines.size() * 19 + 4, Color{20, 30, 22, (unsigned char)(190 * sa)});
            for (auto& l : lines) { UIText(l, x, yy, 15, ColAlpha(Col(235, 255, 235), sa), 1); yy += 19; }
        }
    }
    // stato della rete
    int n = 0;
    for (auto& r : sRemotes) n += r.hasState;
    std::string st;
    Color sc = PIP;
    if (sStatus == 0) { st = "RETE DISATTIVATA"; sc = PIP_DIM; }
    else if (sStatus == 1) { st = "CONNESSIONE ALLA RETE..."; sc = Col(255, 220, 110); }
    else st = n == 0 ? "IN RETE  -  nessun altro viandante" : Fmt("IN RETE  -  %d %s", n, n == 1 ? "viandante" : "viandanti");
    if (PlatformTouch()) UIText(st, Wd / 2, 6, 15, ColAlpha(sc, 0.9f), 1); // in alto a destra ci sono i pulsanti
    else UIText(st, Wd - 16, 10, 15, ColAlpha(sc, 0.9f), 2);
    if (G.state == GS_PLAY && !PlatformTouch()) UIText("T chat   Z saluta   P giocatori", Wd - 16, 30, 13, ColAlpha(PIP_DIM, 0.8f), 2);
    if (sShowList && LocalInGame()) {
        float px = PlatformTouch() ? Wd / 2 - 170 : Wd - 356, py = PlatformTouch() ? 28 : 52, h = 34 + std::max(1, n) * 24;
        UIRect(px, py, 340, h, Color{0, 0, 0, 150});
        UIRectLines(px, py, 340, h, 1, PIP_DIM);
        UIText("VIANDANTI NELLA STANZA", px + 12, py + 8, 15, PIP);
        float yy = py + 32;
        if (n == 0) UIText("Nessuno, per ora. Invita qualcuno!", px + 12, yy, 15, PIP_DIM);
        for (auto& r : sRemotes) {
            if (!r.hasState) continue;
            int rtt = NetRtt(r.id);
            UIText(r.name + Fmt(" [%d]", r.level), px + 12, yy, 15, CHAT_OTHER);
            UIText(r.where + (rtt >= 0 ? Fmt("  %dms", rtt) : std::string()), px + 328, yy, 13, PIP_DIM, 2);
            yy += 24;
        }
    }
    // chat
    bool open = G.state == GS_CHAT;
    float cy = 528;
    int shown = 0;
    for (int i = (int)sChat.size() - 1; i >= 0 && shown < (open ? 10 : 6); i--) {
        const ChatLine& c = sChat[i];
        float a = open ? 1.0f : Clamp01((16.0f - c.t) / 2.0f);
        if (a <= 0) break;
        auto lines = UIWrap(c.text, 16, 520);
        for (int k = (int)lines.size() - 1; k >= 0; k--) {
            Vector2 m = UIMeasure(lines[k], 16);
            UIRect(18, cy - 2, m.x + 12, 22, Color{0, 0, 0, (unsigned char)(110 * a)});
            UIText(lines[k], 24, cy, 16, ColAlpha(c.col, a));
            cy -= 22;
        }
        shown++;
    }
    if (open) {
        float by = 552;
        UIRect(18, by, 540, 30, Color{0, 0, 0, 190});
        UIRectLines(18, by, 540, 30, 1, PIP);
        std::string shownInput = sChatInput;
        while (UIMeasure(shownInput, 17).x > 500) shownInput.erase(0, 1);
        UIText("> " + shownInput + ((int)(G.time * 2) % 2 ? "_" : ""), 26, by + 5, 17, Color{220, 255, 225, 255});
        UIText("INVIO invia   ESC annulla", 558, by + 34, 13, PIP_DIM, 2);
    }
}

// ---------------------------------------------------------------------------
//  Menu del multigiocatore
// ---------------------------------------------------------------------------
static GameState sOnlineBack = GS_MENU;
static int sOnSel = 0;
static bool sEditRoom = false;
static std::string sRoomEdit;

void OnlineOpen(GameState back) {
    sOnlineBack = back;
    sOnSel = 0;
    sEditRoom = false;
    SetGameState(GS_ONLINE);
}

void OnlineUpdate() {
    if (sEditRoom) {
        TypeInto(sRoomEdit, 24);
        if (IsKeyPressed(KEY_ESCAPE)) { sEditRoom = false; return; }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
#if DUSTFALL_WEB
            js_net_set_room(sRoomEdit.c_str());
#endif
            sRemotes.clear();
            sEditRoom = false;
            AddChat("Sei entrato nella stanza \"" + NetRoom() + "\".", CHAT_SYS);
            PlaySfx(SFX_UI_OK);
        }
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) { SetGameState(sOnlineBack); return; }
    const int n = 3;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { sOnSel = (sOnSel + 1) % n; PlaySfx(SFX_UI_MOVE, 0.5f); }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { sOnSel = (sOnSel + n - 1) % n; PlaySfx(SFX_UI_MOVE, 0.5f); }
    float Wd = UIW();
    int clicked = -1;
    for (int i = 0; i < n; i++)
        if (UIHover(Wd / 2 - 330, 230 + i * 50 - 4, 660, 40)) {
            if (Vector2Length(GetMouseDelta()) > 0) sOnSel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
        }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) clicked = sOnSel;
    std::string typed;
    if (PlatformTakeText("stanza", typed) && !typed.empty()) {
#if DUSTFALL_WEB
        js_net_set_room(typed.c_str());
#endif
        sRemotes.clear();
        AddChat("Sei entrato nella stanza \"" + NetRoom() + "\".", CHAT_SYS);
    }
    if (clicked == 0) {
        if (PlatformTouch()) PlatformAskText("stanza", "Nome della stanza (lo stesso per te e i tuoi amici)", NetRoom(), 24);
        else {
            sEditRoom = true;
            sRoomEdit = NetRoom();
            while (GetCharPressed() > 0) {
            }
        }
        PlaySfx(SFX_UI_OK);
    } else if (clicked == 1) {
#if DUSTFALL_WEB
        js_net_copy_invite();
#endif
        Notify(PlatformTouch() ? "Manda il link ai tuoi amici!" : "Link di invito copiato: mandalo ai tuoi amici!");
        PlaySfx(SFX_UI_OK);
    } else if (clicked == 2) {
        PlaySfx(SFX_UI_BACK);
        SetGameState(sOnlineBack);
    }
}

void OnlineDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 190});
    UIText("MULTIGIOCATORE", Wd / 2, 70, 48, PIP, 1);
    int n = 0;
    for (auto& r : sRemotes) n += r.hasState;
    std::string st = sStatus == 0 ? "Rete disattivata" : (sStatus == 1 ? "Connessione alla rete in corso..." : Fmt("In rete  -  %d %s nella stanza", n, n == 1 ? "altro viandante" : "altri viandanti"));
    UIText(st, Wd / 2, 140, 20, sStatus == 2 ? PIP : Col(255, 220, 110), 1);
    float x = Wd / 2 - 320;
    const char* labels[] = {"Stanza", "Invita gli amici", "Indietro"};
    for (int i = 0; i < 3; i++) {
        float y = 230 + i * 50;
        bool sel = i == sOnSel;
        if (sel) UIRect(x - 10, y - 4, 660, 40, Color{30, 110, 55, 160});
        UIText(labels[i], x, y, 26, sel ? Color{220, 255, 225, 255} : PIP);
    }
    std::string room = sEditRoom ? sRoomEdit + ((int)(G.time * 2) % 2 ? "_" : "") : NetRoom();
    UIText(room, x + 260, 230, 26, sEditRoom ? Col(255, 230, 140) : Color{220, 255, 225, 255});
    UIText(PlatformTouch() ? "condividi il link" : "copia il link negli appunti", x + 260, 284, 18, PIP_DIM);
    UIText(NetInvite(), Wd / 2, 340, 17, Col(255, 225, 150), 1);
    if (sEditRoom) UIText("Scrivi il nome della stanza e premi INVIO (ESC annulla)", Wd / 2, 380, 16, Col(255, 230, 140), 1);
    float y = 410;
    UIText("VIANDANTI COLLEGATI", x, y, 22, PIP);
    y += 34;
    if (n == 0) UIText("Nessuno, per ora. Condividi il link della stanza per giocare insieme.", x, y, 18, PIP_DIM);
    for (auto& r : sRemotes) {
        if (!r.hasState || y > 600) continue;
        int rtt = NetRtt(r.id);
        UIText(r.name + Fmt("  [liv. %d]", r.level), x, y, 19, CHAT_OTHER);
        UIText(r.where + (rtt >= 0 ? Fmt("   %d ms, ", rtt) : std::string("   ")) + NetVia(r.id), x + 640, y, 17, PIP_DIM, 2);
        y += 26;
    }
    UIText("Chi è nella stessa stanza si incontra nelle Terre Bruciate. Ognuno ha il proprio mondo e le proprie missioni:", Wd / 2, 630, 15, PIP_DIM, 1);
    UIText("T chatta, Z saluta, P mostra i giocatori; dalla mappa del Pip-Boy puoi raggiungere un amico.", Wd / 2, 652, 15, PIP_DIM, 1);
}

// ---------------------------------------------------------------------------
//  Mappa del Pip-Boy
// ---------------------------------------------------------------------------
void NetMapDraw(const std::function<Vector2(Vector3)>& toMap, std::string& hover) {
    float s = UIS();
    for (auto& r : sRemotes) {
        if (!r.hasState || !r.inGame || r.zone != ZONE_WASTE) continue;
        Vector2 p = toMap(r.pos);
        bool hv = UIHover(p.x - 10, p.y - 10, 20, 20);
        Color c = hv ? Col(255, 245, 210) : CHAT_OTHER;
        DrawPoly(V2(p.x * s, p.y * s), 4, 7 * s, 45, c);
        DrawPoly(V2(p.x * s, p.y * s), 4, 3.5f * s, 45, PIP_BG);
        UIText(r.name, p.x + 10, p.y + 2, 14, c);
        if (hv) hover = "Raggiungi " + r.name;
    }
}

int NetMapClick(const std::function<Vector2(Vector3)>& toMap) {
    for (int i = 0; i < (int)sRemotes.size(); i++) {
        auto& r = sRemotes[i];
        if (!r.hasState || !r.inGame || r.zone != ZONE_WASTE) continue;
        Vector2 p = toMap(r.pos);
        if (UIClicked(p.x - 10, p.y - 10, 20, 20)) return i;
    }
    return -1;
}
