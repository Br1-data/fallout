// Minigiochi: scasso delle serrature e violazione dei terminali.
#include "game.h"

// ---------------------------------------------------------------------------
//  Scasso
// ---------------------------------------------------------------------------
static struct {
    int inter = -1;
    float pin = 0, sweet = 0, tol = 5, rot = 0, health = 1, shake = 0;
    float cooldown = 0;
    bool done = false;
    float doneT = 0;
} LP;

static const int LOCK_REQ[] = {0, 0, 25, 50, 75, 999};
static const char* LOCK_LBL[] = {"", "Facile", "Media", "Difficile", "Molto difficile", ""};

void LockpickStart(int inter) {
    LP = {};
    LP.inter = inter;
    Interactable& it = G.inters[inter];
    LP.sweet = gRng.range(-80, 80);
    int margin = SkillValue(SK_LOCKPICK) - LOCK_REQ[Clamp(it.lock, 0, 5)];
    LP.tol = Clamp(4.0f + margin * 0.18f - it.lock * 0.6f, 2.0f, 16.0f);
    if (G.player.perks[PK_INFILTRATOR]) LP.tol *= 1.5f;
    SetGameState(GS_LOCKPICK);
}

void LockpickUpdate(float dt) {
    Interactable& it = G.inters[LP.inter];
    if (LP.done) {
        LP.doneT += dt;
        if (LP.doneT > 0.6f) {
            SetGameState(GS_PLAY);
            Interact(LP.inter);
        }
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        SetGameState(GS_PLAY);
        return;
    }
    if (LP.cooldown > 0) LP.cooldown -= dt;
    bool turning = (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsMouseButtonDown(MOUSE_BUTTON_LEFT)) && LP.cooldown <= 0;
    if (!turning) {
        float md = GetMouseDelta().x * 0.25f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) md -= 90 * dt;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) md += 90 * dt;
        LP.pin = Clamp(LP.pin + md, -90.0f, 90.0f);
    }
    float off = fabsf(LP.pin - LP.sweet);
    float allowed = Clamp01(1.0f - (off - LP.tol) / 40.0f);
    if (off <= LP.tol) allowed = 1.0f;
    float maxRot = allowed * 90.0f;
    if (turning) {
        LP.rot = std::min(maxRot, LP.rot + 140 * dt);
        if (LP.rot >= 89.5f) {
            it.locked = false;
            LP.done = true;
            G.player.locksPicked++;
            PlaySfx(SFX_UNLOCK);
            GiveXP(10 + it.lock * 10, "Serratura scassinata");
            return;
        }
        if (LP.rot >= maxRot - 0.5f) {
            LP.health -= dt * 1.5f;
            LP.shake = 1;
            static float scrape = 0;
            scrape += dt;
            if (scrape > 0.15f) { PlaySfx(SFX_PIN_TURN, 0.4f, gRng.range(0.9f, 1.3f)); scrape = 0; }
            if (LP.health <= 0) {
                G.player.inv.Remove(I_BOBBYPIN, 1);
                PlaySfx(SFX_PIN_BREAK);
                Notify("La forcina si è spezzata!", Col(255, 180, 120));
                LP.health = 1;
                LP.rot = 0;
                LP.cooldown = 0.5f;
                if (G.player.inv.Count(I_BOBBYPIN) <= 0) {
                    Notify("Hai finito le forcine.", Col(255, 140, 100));
                    SetGameState(GS_PLAY);
                }
            }
        } else LP.shake = 0;
    } else {
        LP.rot = std::max(0.0f, LP.rot - 200 * dt);
        LP.shake = 0;
    }
}

void LockpickDraw() {
    float Wd = UIW(), S = UIS();
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 170});
    Interactable& it = G.inters[LP.inter];
    float cx = Wd / 2, cy = 360;
    float shake = LP.shake * gRng.range(-2, 2);
    DrawCircle((int)(cx * S), (int)(cy * S), 200 * S, Col(70, 66, 58));
    DrawCircle((int)(cx * S), (int)(cy * S), 185 * S, Col(150, 140, 110));
    DrawCircleLines((int)(cx * S), (int)(cy * S), 185 * S, Col(60, 55, 45));
    // cilindro rotante
    float rot = LP.rot * DEG2RAD;
    DrawCircle((int)((cx + shake) * S), (int)(cy * S), 120 * S, Col(185, 170, 130));
    Vector2 kh1 = V2(cx + shake + sinf(rot) * 0, cy);
    float kw = 18, kh = 70;
    Vector2 corners[4] = {V2(-kw, -kh), V2(kw, -kh), V2(kw, kh), V2(-kw, kh)};
    Vector2 pts[4];
    for (int i = 0; i < 4; i++) {
        float x = corners[i].x, y = corners[i].y;
        pts[i] = V2((kh1.x + x * cosf(rot) - y * sinf(rot)) * S, (kh1.y + x * sinf(rot) + y * cosf(rot)) * S);
    }
    DrawTriangle(pts[0], pts[3], pts[2], Col(25, 22, 18));
    DrawTriangle(pts[0], pts[2], pts[1], Col(25, 22, 18));
    // forcina
    float pa = (LP.pin - 90) * DEG2RAD;
    Vector2 tip = V2(cx + cosf(pa) * 30, cy + sinf(pa) * 30);
    Vector2 end = V2(cx + cosf(pa) * 215, cy + sinf(pa) * 215);
    Color pinCol = LP.health < 0.4f ? Col(255, 140, 100) : Col(200, 200, 205);
    DrawLineEx(V2(tip.x * S, tip.y * S), V2(end.x * S, end.y * S), 6 * S, pinCol);
    DrawLineEx(V2(end.x * S, end.y * S), V2((end.x + cosf(pa + 0.3f) * 40) * S, (end.y + sinf(pa + 0.3f) * 40) * S), 6 * S, pinCol);
    // cacciavite
    float sa = rot + PI / 2;
    Vector2 s0 = V2(cx + shake + cosf(sa) * 40, cy + sinf(sa) * 40);
    Vector2 s1 = V2(cx + shake + cosf(sa + 0.25f) * 230, cy + sinf(sa + 0.25f) * 230);
    DrawLineEx(V2(s0.x * S, s0.y * S), V2(s1.x * S, s1.y * S), 9 * S, Col(120, 125, 130));
    DrawLineEx(V2((s0.x + (s1.x - s0.x) * 0.55f) * S, (s0.y + (s1.y - s0.y) * 0.55f) * S), V2(s1.x * S, s1.y * S), 18 * S, Col(170, 40, 30));
    UIText("SCASSO", cx, 60, 34, PIP, 1);
    UIText(Fmt("Serratura: %s    Scasso: %d    Forcine: %d", LOCK_LBL[Clamp(it.lock, 0, 5)], SkillValue(SK_LOCKPICK), G.player.inv.Count(I_BOBBYPIN)), cx, 104, 20, PIP, 1);
    UIText("Mouse/A-D: muovi la forcina    Tieni SPAZIO/clic: gira    ESC: esci", cx, 650, 18, PIP_DIM, 1);
    if (LP.done) UIText("APERTO!", cx, 590, 36, Col(200, 255, 200), 1);
}

// ---------------------------------------------------------------------------
//  Terminali
// ---------------------------------------------------------------------------
static const char* WORDS[] = {
    "CANNA", "FUOCO", "TERRA", "PIANO", "LUOGO", "NOTTE", "CARNE", "PORTA", "BOMBA", "CERTO", "FORZA", "GUSTO", "MONTE",
    "NUOVO", "OMBRA", "PASTA", "RADIO", "SALVO", "TORRE", "VENTO", "ZAINO", "ACQUA", "BANCO", "CORVO", "DENTE", "FIUME",
    "GATTO", "LUNGO", "MASSO", "NERVO", "PALLA", "ROSSO", "SALTO", "TETTO", "VERDE",
    "SEGNALE", "RIFUGIO", "CAPANNA", "DESERTO", "MINIERA", "CAMPANA", "MACCHIA", "SOLDATO", "CAVERNA", "SCHERMO", "BATTUTA",
    "FERRATA", "MISSILE", "PALAZZO", "RADIOSO", "VIAGGIO", "ZAFFIRO", "PISTOLA", "ELMETTO", "BOTTONE", "CANNONE", "FORTUNA", "MACERIE",
    "ESPLOSIVO", "SICUREZZA", "PRIGIONIA", "CORAZZATA", "MUNIZIONE", "FREQUENZA", "DISTRUTTO", "CONTROLLO", "PROGRAMMA",
    "BATTAGLIA", "ELETTRICO", "MECCANICO", "OSSERVARE", "PRIMAVERA", "GUARDIANO", "INVERNALE", "TERMINALE", "AUTORIZZA",
};
static const char GARBAGE[] = "!@#$%^&*-_=+;:'\",./?|\\~`";
static const int ROWS = 16, COLS = 12, NCH = ROWS * COLS * 2;

static struct {
    int inter = -1;
    int mode = 0;          // 0 hacking, 1 menu, 2 lettura
    char grid[NCH + 1];
    int wordAt[NCH];
    std::vector<std::string> words;
    std::vector<int> wordPos;
    std::vector<bool> dud;
    int pass = 0, attempts = 4, maxAttempts = 4;
    std::vector<std::string> log;
    std::vector<std::pair<int, int>> brackets;
    std::vector<bool> bracketUsed;
    int cursor = 0;
    int sel = 0, reading = -1;
    float typeT = 0;
    float exitT = 0;
} TM;

static int Likeness(const std::string& a, const std::string& b) {
    int n = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++)
        if (a[i] == b[i]) n++;
    return n;
}

static void SetupHack(int level) {
    int len = level <= 1 ? 5 : (level == 2 ? 7 : 9);
    std::vector<std::string> pool;
    for (auto w : WORDS)
        if ((int)strlen(w) == len) pool.push_back(w);
    for (size_t i = pool.size() - 1; i > 0; i--) std::swap(pool[i], pool[gRng.irange(0, (int)i)]);
    int count = std::min((int)pool.size(), level <= 1 ? 12 : 11);
    TM.words.assign(pool.begin(), pool.begin() + count);
    TM.pass = gRng.irange(0, count - 1);
    TM.dud.assign(count, false);
    for (int i = 0; i < NCH; i++) {
        TM.grid[i] = GARBAGE[gRng.irange(0, (int)sizeof(GARBAGE) - 2)];
        TM.wordAt[i] = -1;
    }
    TM.grid[NCH] = 0;
    TM.wordPos.assign(count, 0);
    for (int w = 0; w < count; w++) {
        for (int tries = 0; tries < 400; tries++) {
            int p = gRng.irange(0, NCH - len - 1);
            bool ok = true;
            for (int k = -1; k <= len; k++) {
                int q = p + k;
                if (q >= 0 && q < NCH && TM.wordAt[q] >= 0) ok = false;
            }
            if (!ok) continue;
            for (int k = 0; k < len; k++) {
                TM.grid[p + k] = TM.words[w][k];
                TM.wordAt[p + k] = w;
            }
            TM.wordPos[w] = p;
            break;
        }
    }
    // coppie di parentesi sulla stessa riga
    TM.brackets.clear();
    const char* open = "([{<";
    const char* close = ")]}>";
    for (int k = 0; k < 6; k++) {
        for (int tries = 0; tries < 50; tries++) {
            int row = gRng.irange(0, ROWS * 2 - 1);
            int start = row * COLS + gRng.irange(0, COLS - 3);
            int L = gRng.irange(2, 5);
            int endp = start + L;
            if (endp >= (row + 1) * COLS) continue;
            bool ok = true;
            for (int q = start; q <= endp; q++)
                if (TM.wordAt[q] >= 0) ok = false;
            if (!ok) continue;
            int t = gRng.irange(0, 3);
            TM.grid[start] = open[t];
            TM.grid[endp] = close[t];
            for (int q = start + 1; q < endp; q++)
                if (strchr("()[]{}<>", TM.grid[q])) TM.grid[q] = '.';
            TM.brackets.push_back({start, endp});
            break;
        }
    }
    TM.bracketUsed.assign(TM.brackets.size(), false);
    TM.maxAttempts = 4 + (G.player.perks[PK_HACKER] ? 1 : 0);
    TM.attempts = TM.maxAttempts;
    TM.log.clear();
    TM.cursor = 0;
}

static const int HACK_REQ[] = {0, 25, 50, 75};

void TerminalStart(int inter) {
    Interactable& it = G.inters[inter];
    TM.inter = inter;
    TM.sel = 0;
    TM.reading = -1;
    TM.typeT = 0;
    TM.exitT = 0;
    if (it.lockedOut) {
        Notify("Il terminale è bloccato.", Col(255, 160, 100));
        PlaySfx(SFX_TERM_FAIL);
        return;
    }
    if (it.hackLevel > 0 && !it.hacked && it.keyItem >= 0 && G.player.inv.Count(it.keyItem) > 0) {
        it.hacked = true;
        Notify(Fmt("Accesso con: %s", ITEMS[it.keyItem].name));
    }
    if (it.hackLevel > 0 && !it.hacked) {
        int req = HACK_REQ[Clamp(it.hackLevel, 0, 3)];
        if (SkillValue(SK_SCIENCE) < req) {
            Notify(Fmt("Protezione troppo complessa (Scienza %d richiesta).", req), Col(255, 160, 100));
            PlaySfx(SFX_UI_BACK);
            return;
        }
        SetupHack(it.hackLevel);
        TM.mode = 0;
    } else TM.mode = 1;
    PlaySfx(SFX_TERM_OK, 0.6f);
    SetGameState(GS_TERMINAL);
}

void TerminalAction(const std::string& a, Interactable& term) {
    if (a == "open_gate") {
        G.flags["gate_hacked"] = 1;
        Interactable* g = FindInterByTag("base_gate");
        if (g && !g->open) Interact((int)(g - &G.inters[0]));
    } else if (a == "security_off") {
        SecurityOff(term.zone, false);
    } else if (a == "unlock_storage") {
        Interactable* d = FindInterByTag("storage_door");
        if (d) d->locked = false;
        Notify("Il magazzino dei laboratori è sbloccato.");
    } else if (a == "unlock_safe") {
        Interactable* s = FindInterByTag("boss_safe");
        if (s) s->locked = false;
        Notify("La cassaforte è sbloccata.");
    } else if (a == "mk5_off") {
        Entity* m = FindEntityByTag("mk5");
        if (m && !m->off) {
            m->off = true;
            m->aware = false;
            m->ai = AI_IDLE;
            G.flags["mk5_off"] = 1;
            PlaySfx(SFX_ROBOT, 0.8f, 0.5f);
            Notify("La Sentinella Mk.V si spegne con un lungo sospiro meccanico.", Col(150, 255, 200));
        }
    }
}

// layout della griglia
static void GridLayout(float& x0, float& y0, float& cw, float& ch, float& colGap) {
    float Wd = UIW();
    cw = UIMeasure("M", 26, true).x;
    ch = 27;
    float colW = (6 + 1 + COLS) * cw;
    colGap = colW + 30;
    x0 = std::max(40.0f, Wd / 2 - 560);
    y0 = 190;
}

static int CellAtMouse() {
    float x0, y0, cw, ch, gap;
    GridLayout(x0, y0, cw, ch, gap);
    Vector2 m = GetMousePosition();
    float mx = m.x / UIS(), my = m.y / UIS();
    for (int c = 0; c < 2; c++) {
        float gx = x0 + c * gap + 7 * cw;
        if (mx < gx || mx >= gx + COLS * cw) continue;
        int row = (int)((my - y0) / ch);
        if (row < 0 || row >= ROWS) continue;
        int col = (int)((mx - gx) / cw);
        return (c * ROWS + row) * COLS + col;
    }
    return -1;
}

static int BracketAt(int cell) {
    for (size_t i = 0; i < TM.brackets.size(); i++)
        if (!TM.bracketUsed[i] && TM.brackets[i].first == cell) return (int)i;
    return -1;
}

static void HackSelect(int cell) {
    Interactable& it = G.inters[TM.inter];
    if (cell < 0) return;
    int w = TM.wordAt[cell];
    int b = BracketAt(cell);
    if (w >= 0 && !TM.dud[w]) {
        std::string guess = TM.words[w];
        TM.log.push_back(">" + guess);
        if (w == TM.pass) {
            TM.log.push_back(">Accesso consentito.");
            it.hacked = true;
            G.player.termsHacked++;
            PlaySfx(SFX_TERM_OK);
            GiveXP(15 + it.hackLevel * 15, "Terminale violato");
            TM.mode = 1;
            TM.sel = 0;
        } else {
            TM.attempts--;
            TM.log.push_back(">Accesso negato.");
            TM.log.push_back(Fmt(">Somiglianza=%d", Likeness(guess, TM.words[TM.pass])));
            PlaySfx(SFX_TERM_FAIL);
            if (TM.attempts <= 0) {
                it.lockedOut = true;
                TM.log.push_back(">TERMINALE BLOCCATO");
                TM.exitT = 1.5f;
            }
        }
    } else if (b >= 0) {
        TM.bracketUsed[b] = true;
        std::string s(TM.grid + TM.brackets[b].first, TM.grid + TM.brackets[b].second + 1);
        TM.log.push_back(">" + s);
        std::vector<int> duds;
        for (int i = 0; i < (int)TM.words.size(); i++)
            if (i != TM.pass && !TM.dud[i]) duds.push_back(i);
        if (TM.attempts < TM.maxAttempts && gRng.chance(0.35f)) {
            TM.attempts = TM.maxAttempts;
            TM.log.push_back(">Tentativi ripristinati.");
        } else if (!duds.empty()) {
            int d = duds[gRng.irange(0, (int)duds.size() - 1)];
            TM.dud[d] = true;
            for (int k = 0; k < (int)TM.words[d].size(); k++) TM.grid[TM.wordPos[d] + k] = '.';
            TM.log.push_back(">Parola errata rimossa.");
        }
        PlaySfx(SFX_TERM_OK, 0.5f, 1.4f);
    } else {
        TM.log.push_back(Fmt(">%c", TM.grid[cell]));
        TM.log.push_back(">Errore");
        PlaySfx(SFX_TYPE);
    }
    while (TM.log.size() > 16) TM.log.erase(TM.log.begin());
}

void TerminalUpdate(float dt) {
    Interactable& it = G.inters[TM.inter];
    TM.typeT += dt;
    if (TM.exitT > 0) {
        TM.exitT -= dt;
        if (TM.exitT <= 0) SetGameState(GS_PLAY);
        return;
    }
    const TerminalDef& T = TERMINALS[it.terminal];
    if (TM.mode == 0) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) { SetGameState(GS_PLAY); return; }
        int mc = CellAtMouse();
        if (mc >= 0 && Vector2Length(GetMouseDelta()) > 0.1f) TM.cursor = mc;
        if (IsKeyPressed(KEY_RIGHT)) TM.cursor = std::min(NCH - 1, TM.cursor + 1);
        if (IsKeyPressed(KEY_LEFT)) TM.cursor = std::max(0, TM.cursor - 1);
        if (IsKeyPressed(KEY_DOWN)) TM.cursor = std::min(NCH - 1, TM.cursor + COLS);
        if (IsKeyPressed(KEY_UP)) TM.cursor = std::max(0, TM.cursor - COLS);
        if (IsKeyPressed(KEY_ENTER) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mc >= 0)) HackSelect(TM.cursor);
        return;
    }
    if (TM.mode == 2) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_TAB)) {
            TM.mode = 1;
            PlaySfx(SFX_TYPE);
        }
        return;
    }
    int n = (int)T.entries.size() + 1;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { TM.sel = (TM.sel + 1) % n; PlaySfx(SFX_TYPE); }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { TM.sel = (TM.sel + n - 1) % n; PlaySfx(SFX_TYPE); }
    float x = std::max(40.0f, UIW() / 2 - 560);
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E);
    for (int i = 0; i < n; i++)
        if (UIClicked(x, 220 + i * 34, 700, 32)) {
            TM.sel = i;
            enter = true;
        }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) { SetGameState(GS_PLAY); return; }
    if (enter) {
        PlaySfx(SFX_TYPE);
        if (TM.sel == n - 1) { SetGameState(GS_PLAY); return; }
        TM.reading = TM.sel;
        TM.mode = 2;
        TM.typeT = 0;
        if (!T.entries[TM.sel].action.empty()) TerminalAction(T.entries[TM.sel].action, G.inters[TM.inter]);
    }
}

void TerminalDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Col(4, 14, 8));
    float S = UIS();
    for (float ly = 0; ly < 720; ly += 3) DrawRectangle(0, (int)(ly * S), GetScreenWidth(), std::max(1, (int)S), Color{0, 0, 0, 70});
    DrawTexturePro(R.vignette, Rectangle{0, 0, 256, 256}, Rectangle{0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, V2(0, 0), 0, ColAlpha(BLACK, 0.8f));
    Interactable& it = G.inters[TM.inter];
    const TerminalDef& T = TERMINALS[it.terminal];
    float x = std::max(40.0f, Wd / 2 - 560);
    Color c = PIP;
    UIText("ATOMTEC INDUSTRIES (TM) TERMLINK PROTOCOL", x, 40, 30, c, 0, true);
    if (TM.mode == 0) {
        UIText("INSERIRE LA PASSWORD", x, 80, 30, c, 0, true);
        std::string att = Fmt("%d TENTATIVI RIMASTI: ", TM.attempts);
        for (int i = 0; i < TM.attempts; i++) att += "# ";
        UIText(att, x, 130, 28, TM.attempts <= 1 ? Col(255, 200, 120) : c, 0, true);
        float x0, y0, cw, ch, gap;
        GridLayout(x0, y0, cw, ch, gap);
        int hw = TM.wordAt[TM.cursor];
        int hb = BracketAt(TM.cursor);
        for (int col = 0; col < 2; col++)
            for (int row = 0; row < ROWS; row++) {
                float yy = y0 + row * ch;
                float xx = x0 + col * gap;
                UIText(Fmt("0x%04X", 0xF4A0 + (col * ROWS + row) * COLS), xx, yy, 26, PIP_DIM, 0, true);
                for (int k = 0; k < COLS; k++) {
                    int idx = (col * ROWS + row) * COLS + k;
                    bool hl = idx == TM.cursor || (hw >= 0 && !TM.dud[hw] && TM.wordAt[idx] == hw) ||
                              (hb >= 0 && idx >= TM.brackets[hb].first && idx <= TM.brackets[hb].second);
                    float cx = xx + (7 + k) * cw;
                    if (hl) UIRect(cx, yy + 2, cw, ch - 2, c);
                    char s[2] = {TM.grid[idx], 0};
                    UIText(s, cx, yy, 26, hl ? Col(4, 14, 8) : c, 0, true);
                }
            }
        float lx = x0 + gap * 2 + 10;
        float ly = y0 + ROWS * ch - 24;
        for (int i = (int)TM.log.size() - 1; i >= 0 && ly > y0; i--) {
            UIText(TM.log[i], lx, ly, 26, c, 0, true);
            ly -= 26;
        }
        std::string cur = hw >= 0 && !TM.dud[hw] ? TM.words[hw] : (hb >= 0 ? std::string(TM.grid + TM.brackets[hb].first, TM.grid + TM.brackets[hb].second + 1) : std::string(1, TM.grid[TM.cursor]));
        UIText(">" + cur + ((int)(TM.typeT * 2) % 2 ? "_" : ""), lx, y0 + ROWS * ch + 4, 26, c, 0, true);
        UIText("Mouse/frecce: scegli   INVIO/clic: prova   ESC: esci", x, 680, 22, PIP_DIM, 0, true);
        return;
    }
    UIText(T.header, x, 80, 28, c, 0, true);
    UIRect(x, 118, std::min(Wd - 2 * x, 1120.0f), 2, PIP_DIM);
    if (TM.mode == 1) {
        int n = (int)T.entries.size();
        for (int i = 0; i <= n; i++) {
            std::string label = i < n ? "[" + T.entries[i].title + "]" : "[Esci]";
            bool sel = i == TM.sel;
            float yy = 220 + i * 34;
            if (sel) UIRect(x - 4, yy, UIMeasure(label, 28, true).x + 8, 32, c);
            UIText(label, x, yy, 28, sel ? Col(4, 14, 8) : c, 0, true);
        }
        UIText(">" + std::string((int)(TM.typeT * 2) % 2 ? "_" : ""), x, 220 + (n + 2) * 34, 28, c, 0, true);
    } else {
        const TermEntry& e = T.entries[TM.reading];
        std::string body = e.text;
        size_t shown = std::min(body.size(), (size_t)(TM.typeT * 90));
        while (shown < body.size() && (body[shown] & 0xC0) == 0x80) shown++;
        UIText(e.title, x, 150, 28, c, 0, true);
        UIText(body.substr(0, shown), x, 200, 28, c, 0, true);
        UIText("Premi INVIO per tornare", x, 640, 24, PIP_DIM, 0, true);
    }
}
