// Interfaccia: font, primitive 2D, notifiche e HUD.
#include "game.h"
#include "embedded_data.h"

const Color PIP = {80, 255, 130, 255};
const Color PIP_DIM = {50, 170, 85, 255};
const Color PIP_DARK = {20, 70, 35, 255};
const Color PIP_BG = {6, 22, 12, 255};

static std::vector<std::pair<int, Font>> sUiFonts, sTermFonts;

static std::vector<int> Codepoints() {
    std::vector<int> cps;
    for (int c = 32; c < 127; c++) cps.push_back(c);
    for (int c = 160; c < 256; c++) cps.push_back(c);
    int extra[] = {0x2022, 0x2026, 0x2019, 0x2018, 0x201C, 0x201D, 0x2014, 0x2013, 0x2190, 0x2191, 0x2192, 0x2193, 0x25B2, 0x25BC, 0x25A0, 0x25CF, 0x2588};
    for (int c : extra) cps.push_back(c);
    return cps;
}

void UIInit() {
    std::vector<int> cps = Codepoints();
    for (int s : {14, 18, 24, 32, 44, 64, 96}) {
        Font f = LoadFontFromMemory(".ttf", FONT_UI_DATA, (int)FONT_UI_SIZE, s, cps.data(), (int)cps.size());
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        sUiFonts.push_back({s, f});
    }
    for (int s : {18, 24, 32, 44}) {
        Font f = LoadFontFromMemory(".ttf", FONT_TERM_DATA, (int)FONT_TERM_SIZE, s, cps.data(), (int)cps.size());
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        sTermFonts.push_back({s, f});
    }
}

void UIShutdown() {
    for (auto& f : sUiFonts) UnloadFont(f.second);
    for (auto& f : sTermFonts) UnloadFont(f.second);
}

Font& UIFontFor(float px, bool term) {
    auto& list = term ? sTermFonts : sUiFonts;
    for (auto& f : list)
        if (f.first >= px * 0.95f) return f.second;
    return list.back().second;
}

float UIS() { return GetScreenHeight() / 720.0f; }
float UIW() { return GetScreenWidth() / UIS(); }

static void DrawLine1(const std::string& s, float x, float y, float size, Color c, int align, bool term) {
    float S = UIS();
    float px = size * S;
    Font& f = UIFontFor(px, term);
    float sp = term ? 0 : px * 0.03f;
    Vector2 m = MeasureTextEx(f, s.c_str(), px, sp);
    float dx = align == 1 ? -m.x * 0.5f : (align == 2 ? -m.x : 0);
    DrawTextEx(f, s.c_str(), V2(roundf(x * S + dx), roundf(y * S)), px, sp, c);
}

void UIText(const std::string& s, float x, float y, float size, Color c, int align, bool term) {
    size_t start = 0;
    int line = 0;
    while (true) {
        size_t nl = s.find('\n', start);
        std::string ln = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        DrawLine1(ln, x, y + line * size * 1.18f, size, c, align, term);
        if (nl == std::string::npos) break;
        start = nl + 1;
        line++;
    }
}

Vector2 UIMeasure(const std::string& s, float size, bool term) {
    float S = UIS();
    float px = size * S;
    Font& f = UIFontFor(px, term);
    Vector2 m = MeasureTextEx(f, s.c_str(), px, term ? 0 : px * 0.03f);
    return V2(m.x / S, m.y / S);
}

void UIRect(float x, float y, float w, float h, Color c) {
    float S = UIS();
    DrawRectangleRec(Rectangle{x * S, y * S, w * S, h * S}, c);
}

void UIRectLines(float x, float y, float w, float h, float thick, Color c) {
    float S = UIS();
    DrawRectangleLinesEx(Rectangle{x * S, y * S, w * S, h * S}, std::max(1.0f, thick * S), c);
}

bool UIHover(float x, float y, float w, float h) {
    if (IsCursorHidden()) return false;
    float S = UIS();
    Vector2 m = GetMousePosition();
    return m.x >= x * S && m.x <= (x + w) * S && m.y >= y * S && m.y <= (y + h) * S;
}

bool UIClicked(float x, float y, float w, float h) {
    return UIHover(x, y, w, h) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

std::vector<std::string> UIWrap(const std::string& s, float size, float width, bool term) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        std::string para = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        std::string line, word;
        size_t i = 0;
        auto flushWord = [&] {
            if (word.empty()) return;
            std::string t = line.empty() ? word : line + " " + word;
            if (UIMeasure(t, size, term).x > width && !line.empty()) {
                out.push_back(line);
                line = word;
            } else line = t;
            word.clear();
        };
        for (; i < para.size(); i++) {
            if (para[i] == ' ') flushWord();
            else word += para[i];
        }
        flushWord();
        out.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

void Notify(const std::string& msg, Color c) {
    G.messages.push_back({msg, c, 5.0f});
    if (G.messages.size() > 6) G.messages.erase(G.messages.begin());
}

void BigNotify(const std::string& msg, const std::string& sub) {
    G.bigMsg = msg;
    G.bigSub = sub;
    G.bigMsgT = 3.5f;
}

// ---------------------------------------------------------------------------
//  HUD
// ---------------------------------------------------------------------------
static void Bar(float x, float y, float w, float h, float frac, Color c) {
    UIRect(x, y, w, h, Color{0, 0, 0, 120});
    UIRect(x, y, w * Clamp01(frac), h, c);
    UIRectLines(x, y, w, h, 1, ColAlpha(c, 0.6f));
    for (int i = 1; i < 10; i++) UIRect(x + w * i / 10.0f, y, 1, h, Color{0, 0, 0, 90});
}

static const char* LOCK_NAMES[] = {"", "Facile", "Media", "Difficile", "Molto difficile", "Serve una chiave"};

static std::string InteractPrompt() {
    if (G.lookInter >= 0) {
        Interactable& it = G.inters[G.lookInter];
        std::string lock = it.locked ? std::string("  [") + LOCK_NAMES[Clamp(it.lock, 0, 5)] + "]" : "";
        switch (it.type) {
            case INT_CONTAINER: return "E) Apri  " + it.name + lock + (it.inv.Empty() && !it.locked ? "  [Vuoto]" : "");
            case INT_DOOR: return it.open ? "" : "E) Apri  " + it.name + lock;
            case INT_TRANSITION: return "E) " + it.name;
            case INT_TERMINAL: return "E) Accedi  " + it.name + (it.lockedOut ? "  [Bloccato]" : "");
            case INT_BED: return "E) Riposa  " + it.name;
            case INT_ITEM: return "E) Raccogli  " + it.name;
            case INT_REACTOR: return "E) Esamina  " + it.name;
            case INT_VAULTDOOR: return it.open ? "" : "E) Apri la porta del Rifugio";
            case INT_GATE: return it.open ? "" : "E) Apri  " + it.name;
            case INT_LADDER: return "E) " + it.name;
            case INT_REPAIR: {
                if (it.used) return "";
                int parts = std::max(0, it.partsReq - (G.player.perks[PK_MECHANIC] ? 1 : 0));
                return Fmt("E) Ripara  %s  [Riparazione %d, kit 1, componenti %d]", it.name.c_str(), it.skillReq, parts);
            }
        }
    }
    if (G.lookEnt >= 0) {
        Entity& e = G.ents[G.lookEnt];
        if (e.kind != EK_HUMAN) return "E) Avvicinati a  " + e.name;
        return "E) Parla  " + e.name;
    }
    return "";
}

static void Compass(float x, float y, float w) {
    Player& P = G.player;
    float h = 26;
    UIRect(x, y, w, h, Color{0, 0, 0, 110});
    UIRectLines(x, y, w, h, 1, PIP_DIM);
    float cx = x + w / 2;
    auto ToX = [&](float worldAngle, bool& vis) {
        float rel = AngleWrap(worldAngle - P.yaw);
        vis = fabsf(rel) < PI / 2;
        return cx - rel / (PI / 2) * (w / 2);
    };
    struct Card { const char* n; float a; } cards[] = {{"N", PI}, {"E", PI / 2}, {"S", 0}, {"O", -PI / 2}};
    for (int i = 0; i < 16; i++) {
        float a = i / 16.0f * 2 * PI;
        bool vis;
        float px = ToX(a, vis);
        if (vis) UIRect(px, y + h - 6, 1, 5, PIP_DIM);
    }
    for (auto& c : cards) {
        bool vis;
        float px = ToX(c.a, vis);
        if (vis) UIText(c.n, px, y + 3, 18, PIP, 1);
    }
    if (P.zone == ZONE_WASTE)
        for (auto& l : W.locations) {
            if (!l.discovered && DistXZ(l.pos, P.pos) > 200) continue;
            if (DistXZ(l.pos, P.pos) < l.radius * 0.6f) continue;
            bool vis;
            float px = ToX(AngleTo(P.pos, l.pos), vis);
            if (vis) {
                UIRect(px - 3, y - 7, 6, 6, l.discovered ? PIP : PIP_DIM);
            }
        }
    for (auto& e : G.ents) {
        if (e.zone != P.zone || !e.hostile || !e.aware) continue;
        if (Vector3Distance(e.pos, P.pos) > 70) continue;
        bool vis;
        float px = ToX(AngleTo(P.pos, e.pos), vis);
        if (vis) UIRect(px - 2, y + 4, 4, h - 8, Col(255, 60, 40));
    }
    for (int q = 0; q < Q_COUNT; q++) {
        if (QUESTS[q].stage == 0 || QUESTS[q].done) continue;
        if (q != Q_MAIN && QUESTS[Q_MAIN].stage < 20) continue;
        auto objs = QuestObjectives(q);
        for (auto& o : objs) {
            if (!o.hasPos || o.zone != P.zone) continue;
            bool vis;
            float ang = AngleTo(P.pos, o.pos);
            float px = ToX(ang, vis);
            if (!vis) px = AngleWrap(ang - P.yaw) > 0 ? x + 6 : x + w - 6;
            Color qc = q == Q_MAIN ? Col(255, 230, 120) : Col(200, 255, 200);
            float s = UIS();
            DrawTriangle(V2(px * s, (y + h + 2) * s), V2((px + 6) * s, (y + h + 11) * s), V2((px - 6) * s, (y + h + 11) * s), qc);
            if (q == Q_MAIN && vis) UIText(Fmt("%dm", (int)DistXZ(P.pos, o.pos)), px, y + h + 12, 13, qc, 1);
            break;
        }
    }
}

// mirino della fotocamera
static void Viewfinder(float Wd) {
    float k = Clamp01((G.player.aimT - 0.5f) * 2);
    Color c = ColAlpha(Col(240, 240, 230), 0.85f * k);
    float m = 70, x0 = m * 1.6f, y0 = m, x1 = Wd - m * 1.6f, y1 = 720 - m, L = 40;
    for (int sx = 0; sx < 2; sx++)
        for (int sy = 0; sy < 2; sy++) {
            float x = sx ? x1 : x0, y = sy ? y1 : y0;
            UIRect(sx ? x - L : x, sy ? y - 2 : y, L, 2, c);
            UIRect(sx ? x - 2 : x, sy ? y - L : y, 2, L, c);
        }
    UIRectLines(Wd / 2 - 60, 300, 120, 120, 1, ColAlpha(c, 0.6f));
    if (fmodf(G.time, 1.0f) < 0.6f) {
        float s = UIS();
        DrawCircle((int)((x0 + 20) * s), (int)((y0 + 22) * s), 7 * s, ColAlpha(Col(255, 60, 50), k));
    }
    UIText("ZOOM", x0 + 34, y0 + 12, 18, c);
    UIText(Fmt("FOTO %d/%d", (int)G.player.photos.size(), (int)PHOTO_SUBJECTS.size()), x1 - 8, y0 + 12, 18, c, 2);
}

void HudDraw() {
    Player& P = G.player;
    float Wd = UIW();
    // effetti a schermo: colpi, radiazioni, mantello ottico
    float lowHp = P.hp < HPCap() * 0.3f ? (0.25f + 0.15f * sinf(G.time * 5)) : 0;
    float red = std::max(P.hurtFlash * 0.85f, lowHp);
    Rectangle full{0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
    if (red > 0.01f) DrawTexturePro(R.vignette, Rectangle{0, 0, 256, 256}, full, V2(0, 0), 0, ColAlpha(Col(200, 0, 0), red));
    if (G.radRate > 0.05f) {
        float k = Clamp01(G.radRate / 15.0f) * 0.35f;
        DrawTexturePro(R.noiseTex, Rectangle{gRng.range(0, 256), gRng.range(0, 256), 256 * 2.0f, 256 * 2.0f}, full, V2(0, 0), 0, ColAlpha(Col(120, 255, 100), k * 0.4f));
    }
    if (P.stealthT > 0) DrawTexturePro(R.vignette, Rectangle{0, 0, 256, 256}, full, V2(0, 0), 0, ColAlpha(Col(60, 150, 255), 0.45f + 0.1f * sinf(G.time * 3)));
    if (G.chased) DrawTexturePro(R.vignette, Rectangle{0, 0, 256, 256}, full, V2(0, 0), 0, ColAlpha(Col(255, 120, 40), 0.18f + 0.08f * sinf(G.time * 6)));
    DrawTexturePro(R.vignette, Rectangle{0, 0, 256, 256}, full, V2(0, 0), 0, ColAlpha(BLACK, 0.45f));
    if (G.state == GS_SCAN) return;

    bool zoom = P.tool == TOOL_CAMERA && P.aimT > 0.5f;
    if (zoom) Viewfinder(Wd);
    // chi stai guardando
    {
        RayHit eh;
        Vector3 f = Vector3Normalize(G.cam.target - G.cam.position);
        if (RaycastEntities(P.zone, G.cam.position, f, zoom ? 150.0f : 60.0f, eh)) {
            Entity& e = G.ents[eh.entity];
            RayHit wh;
            if (!RaycastWorld(P.zone, G.cam.position, f, eh.t, wh) && (e.hostile || zoom)) {
                Color ec = e.ai == AI_CHASE ? Col(255, 110, 80) : (e.hostile && !e.off && !e.asleep ? Col(255, 220, 110) : PIP);
                UIText(e.name, Wd / 2, 40, 20, ec, 1);
                std::string st = EntityState(e);
                if (zoom && !e.photoKey.empty()) st += HasPhoto(e.photoKey) ? "   [già fotografato]" : "   [nuovo soggetto!]";
                UIText(st, Wd / 2, 64, 16, ColAlpha(ec, 0.85f), 1);
            }
        }
    }
    float cx = Wd / 2, cy = 360;
    Color cc = ColAlpha(PIP, 0.85f);
    if (!zoom) {
        UIRect(cx - 9, cy - 1, 6, 2, cc);
        UIRect(cx + 3, cy - 1, 6, 2, cc);
        UIRect(cx - 1, cy - 9, 2, 6, cc);
        UIRect(cx - 1, cy + 3, 2, 6, cc);
    }

    std::string prompt = InteractPrompt();
    if (!prompt.empty()) {
        Vector2 m = UIMeasure(prompt, 20);
        UIRect(cx - m.x / 2 - 10, cy + 40, m.x + 20, 30, Color{0, 0, 0, 140});
        UIText(prompt, cx, cy + 44, 20, PIP, 1);
    }

    // in basso a sinistra: PF e bussola
    float bx = 36, by = 640;
    UIText("PF", bx, by + 1, 20, PIP);
    float cap = HPCap(), mhp = MaxHP();
    Bar(bx + 34, by + 4, 230, 14, P.hp / mhp, PIP);
    if (cap < mhp) UIRect(bx + 34 + 230 * cap / mhp, by + 4, 230 * (1 - cap / mhp), 14, Color{200, 60, 40, 200});
    Compass(bx, by - 46, 264);
    if (P.rads >= 1 || G.radRate > 0.2f) {
        Color rc = G.radRate > 0.2f ? Col(255, 230, 80) : PIP_DIM;
        UIText(Fmt("RAD %d", (int)P.rads) + (G.radRate > 0.2f ? Fmt("  +%.1f/s", G.radRate) : std::string()), bx + 34, by + 22, 16, rc);
    }
    // in basso a destra: PA e attrezzo in mano
    float rx = Wd - 300;
    UIText("PA", rx + 264 - 0, by + 1, 20, PIP, 0);
    Bar(rx, by + 4, 230, 14, P.ap / MaxAP(), PIP);
    int ti = ToolItem(P.tool);
    std::string tool = ToolName(P.tool);
    if (P.tool == TOOL_BLASTER) tool += "  8 PA / colpo";
    else if (ti >= 0 && P.tool != TOOL_CAMERA) tool += Fmt("  x%d", P.inv.Count(ti));
    UIText(tool, rx + 230, by - 30, 24, PIP, 2);
    UIText(Fmt("B) Esche %d   G) Barattoli %d   H) Kit %d", P.inv.Count(I_BAIT), P.inv.Count(I_NOISE), P.inv.Count(I_MEDKIT)), rx, by + 22, 15, PIP_DIM);

    // furtività: quanto ti stanno notando
    float maxAlert = 0;
    bool seen = false;
    for (auto& e : G.ents) {
        if (e.zone != P.zone || !e.hostile || e.off || e.companion) continue;
        if (Vector3Distance(e.pos, P.pos) > 60) continue;
        if (e.aware) seen = true;
        if (e.ai != AI_DISTRACT && e.ai != AI_SEARCH) maxAlert = std::max(maxAlert, std::min(e.alert, 1.0f));
    }
    const char* t = nullptr;
    Color tc = PIP;
    if (P.stealthT > 0) { t = "[ INVISIBILE ]"; tc = Col(140, 200, 255); }
    else if (seen) { t = "[ TI HANNO VISTO! ]"; tc = Col(255, 90, 70); }
    else if (maxAlert > 0.15f) { t = "[ SOSPETTO ]"; tc = Col(255, 220, 100); }
    else if (P.crouch) t = "[ NASCOSTO ]";
    if (t) {
        UIText(t, cx, 96, 20, tc, 1);
        if (!seen && maxAlert > 0.05f && P.stealthT <= 0) Bar(cx - 60, 122, 120, 6, maxAlert, Col(255, 220, 100));
        if (P.stealthT > 0) UIText(Fmt("%ds", (int)ceilf(P.stealthT)), cx, 122, 16, tc, 1);
    }
    for (auto& e : G.ents)
        if (e.companion) {
            UIText(Fmt("%s ti segue", e.name.c_str()), bx, by + 44, 15, PIP_DIM);
            break;
        }
    if (P.pendingLevels > 0) UIText("▲ LIVELLO SUPERIORE  (TAB)", bx + 150, by + 44, 15, Col(255, 230, 120));
    if (P.inv.Weight() > CarryMax()) UIText("SOVRACCARICO", Wd / 2, 680, 18, Col(255, 140, 90), 1);

    if (G.settings.showFps) UIText(Fmt("%d FPS", GetFPS()), Wd - 16, 690, 16, PIP_DIM, 2);
}

void UIDrawMessages() {
    float Wd = UIW();
    float my = 24;
    for (auto& m : G.messages) {
        float a = Clamp01(m.t / 1.0f);
        Vector2 sz = UIMeasure(m.text, 18);
        UIRect(20, my - 2, sz.x + 16, 26, Color{0, 0, 0, (unsigned char)(110 * a)});
        UIText(m.text, 28, my, 18, ColAlpha(m.col, a));
        my += 30;
    }
    if (G.bigMsgT > 0) {
        float a = Clamp01(G.bigMsgT / 0.8f) * Clamp01((3.5f - G.bigMsgT) / 0.3f);
        Vector2 m = UIMeasure(G.bigMsg, 40);
        UIRect(Wd / 2 - m.x / 2 - 30, 150, m.x + 60, G.bigSub.empty() ? 60 : 92, Color{0, 0, 0, (unsigned char)(150 * a)});
        UIText(G.bigMsg, Wd / 2, 158, 40, ColAlpha(PIP, a), 1);
        if (!G.bigSub.empty()) UIText(G.bigSub, Wd / 2, 206, 20, ColAlpha(PIP_DIM, a), 1);
    }
}
