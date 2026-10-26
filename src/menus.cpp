// Menu e schermate: principale, introduzione, creazione, dialogo, pausa, svenimento, finale, riposo, salvataggi, opzioni.
#include "game.h"
#include "net.h"
#include "platform.h"

static int sMenuSel = 0;

static bool MenuItem(const std::string& label, float x, float y, float size, bool selected, bool enabled = true, int align = 1) {
    Vector2 m = UIMeasure(label, size);
    float bx = align == 1 ? x - m.x / 2 - 16 : x - 16;
    bool hover = enabled && UIHover(bx, y - 4, m.x + 32, size + 10);
    Color c = !enabled ? PIP_DARK : (selected || hover ? Color{220, 255, 225, 255} : PIP);
    if (selected && enabled) UIRect(bx, y - 4, m.x + 32, size + 10, Color{30, 110, 55, 160});
    UIText(label, x, y, size, c, align);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static int VerticalNav(int sel, int n, bool letters = true) {
    if (IsKeyPressed(KEY_DOWN) || (letters && IsKeyPressed(KEY_S))) { sel = (sel + 1) % n; PlaySfx(SFX_UI_MOVE, 0.5f); }
    if (IsKeyPressed(KEY_UP) || (letters && IsKeyPressed(KEY_W))) { sel = (sel + n - 1) % n; PlaySfx(SFX_UI_MOVE, 0.5f); }
    return sel;
}

static bool Confirm() { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE); }

// ---------------------------------------------------------------------------
//  Menu principale
// ---------------------------------------------------------------------------
static bool AnySave() {
    for (int i = 0; i <= 6; i++)
        if (SaveExists(i)) return true;
    return false;
}

static int NewestSave() {
    int best = -1;
    long long bt = -1;
    for (int i = 0; i <= 6; i++) {
        if (!SaveExists(i)) continue;
        long long t = SaveTime(i);
        if (t > bt) { bt = t; best = i; }
    }
    return best;
}

enum MainAction { MA_OPENWORLD, MA_STORY, MA_CONTINUE, MA_LOAD, MA_ONLINE, MA_OPTIONS, MA_QUIT };
struct MainItem { const char* label; MainAction act; };

static std::vector<MainItem> MainItems() {
    std::vector<MainItem> v = {{"MONDO APERTO", MA_OPENWORLD}, {"NUOVA STORIA", MA_STORY}, {"CONTINUA", MA_CONTINUE}, {"CARICA PARTITA", MA_LOAD}};
    if (NetAvailable()) v.push_back({"MULTIGIOCATORE", MA_ONLINE});
    v.push_back({"OPZIONI", MA_OPTIONS});
    if (!DUSTFALL_WEB) v.push_back({"ESCI", MA_QUIT}); // nel browser si chiude la scheda
    return v;
}

static bool MainEnabled(const MainItem& it, bool hasSave) { return hasSave || (it.act != MA_CONTINUE && it.act != MA_LOAD); }
static float MainY(int i) { return 350 + i * 48; }
static bool sNewOpenWorld = false; // la creazione del personaggio porta al mondo aperto

void MenuUpdate() {
    std::vector<MainItem> items = MainItems();
    int n = (int)items.size();
    bool has = AnySave();
    bool up = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    sMenuSel = VerticalNav(sMenuSel % n, n);
    for (int k = 0; k < n && !MainEnabled(items[sMenuSel], has); k++) sMenuSel = (sMenuSel + (up ? n - 1 : 1)) % n;
    int clicked = -1;
    float Wd = UIW();
    for (int i = 0; i < n; i++)
        if (UIHover(Wd / 2 - 160, MainY(i) - 4, 320, 40) && MainEnabled(items[i], has)) sMenuSel = i;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && UIHover(Wd / 2 - 160, MainY(sMenuSel) - 4, 320, 40)) clicked = sMenuSel;
    if (Confirm()) clicked = sMenuSel;
    if (clicked < 0) return;
    switch (items[clicked].act) {
        case MA_OPENWORLD: PlaySfx(SFX_UI_OK); sNewOpenWorld = true; SetGameState(GS_CREATE); break;
        case MA_STORY: PlaySfx(SFX_UI_OK); sNewOpenWorld = false; SetGameState(GS_INTRO); break;
        case MA_CONTINUE: {
            int s = NewestSave();
            if (s >= 0 && LoadGame(s)) PlaySfx(SFX_UI_OK);
            break;
        }
        case MA_LOAD: PlaySfx(SFX_UI_OK); SaveLoadOpen(false, GS_MENU); break;
        case MA_ONLINE: PlaySfx(SFX_UI_OK); OnlineOpen(GS_MENU); break;
        case MA_OPTIONS: PlaySfx(SFX_UI_OK); OptionsOpen(GS_MENU); break;
        case MA_QUIT: G.quit = true; break;
    }
}

void MenuDraw() {
    float Wd = UIW();
    DrawTexturePro(R.vignette, Rectangle{0, 0, 256, 256}, Rectangle{0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, V2(0, 0), 0, ColAlpha(BLACK, 0.7f));
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 40});
    float glow = 0.5f + 0.5f * sinf(G.time * 1.3f);
    UIText("DUSTFALL", Wd / 2 + 4, 124, 120, Color{0, 0, 0, 160}, 1);
    UIText("DUSTFALL", Wd / 2, 120, 120, ColLerp(Col(255, 200, 110), Col(255, 230, 170), glow), 1);
    UIText("L'ATLANTE DELL'ALBA", Wd / 2, 250, 28, Col(230, 200, 150), 1);
    UIRect(Wd / 2 - 260, 292, 520, 2, ColAlpha(Col(230, 200, 150), 0.6f));
    std::vector<MainItem> items = MainItems();
    bool has = AnySave();
    for (int i = 0; i < (int)items.size(); i++) MenuItem(items[i].label, Wd / 2, MainY(i), 30, i == sMenuSel, MainEnabled(items[i], has));
    const char* hint = sMenuSel < (int)items.size() && items[sMenuSel].act == MA_OPENWORLD ? "Parti subito da Ferrocittà ed esplora liberamente: è qui che si incontrano gli altri giocatori."
                       : sMenuSel < (int)items.size() && items[sMenuSel].act == MA_STORY ? "Un segnale scomparso, sei capitoli della rete e le storie delle comunità." : "";
    UIText(hint, Wd / 2, 655, 16, ColAlpha(Col(230, 200, 150), 0.75f), 1);
    UIText(DUSTFALL_WEB ? "Atlante web  -  Esplorazione, strumenti e furtività" : "Atlante  -  Esplorazione, strumenti e furtività", 20, 690, 15, ColAlpha(Col(230, 200, 150), 0.6f));
    UIText("Frecce/Mouse + INVIO", Wd - 20, 690, 15, ColAlpha(Col(230, 200, 150), 0.6f), 2);
}

// ---------------------------------------------------------------------------
//  Introduzione
// ---------------------------------------------------------------------------
static const char* INTRO[] = {
    "Anno 2161. Il Rifugio 33 apre le sue porte.",
    "",
    "Per anni una voce alla radio ha indicato acqua, sentieri e stagioni.",
    "La chiamavano Lumen. Adesso resta soltanto un fruscio.",
    "",
    "Negli archivi compare un nome: l'Atlante dell'Alba.",
    "Una rete di giardini, osservatori e memorie condivise.",
    "",
    "Ferri ti affida uno scanner, un registratore e un multiutensile.",
    "Non cerca un eroe. Cerca qualcuno disposto ad ascoltare.",
    "",
    "Oltre la montagna, ogni luogo custodisce una parte della risposta.",
};
static float sIntroT = 0;

void IntroUpdate(float dt) {
    sIntroT += dt;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || sIntroT > 32) {
        sIntroT = 0;
        SetGameState(GS_CREATE);
    }
}

void IntroDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Color{6, 5, 4, 255});
    float chars = sIntroT * 28;
    float y = 150;
    int total = 0;
    for (auto line : INTRO) {
        std::string s = line;
        int len = (int)s.size();
        if (chars < total) break;
        int show = std::min(len, (int)(chars - total));
        while (show < len && (s[show] & 0xC0) == 0x80) show++;
        UIText(s.substr(0, show), Wd / 2, y, 28, Col(230, 205, 160), 1);
        total += len + 8;
        y += 38;
    }
    UIText("INVIO per continuare", Wd / 2, 680, 16, ColAlpha(Col(230, 205, 160), 0.5f), 1);
}

// ---------------------------------------------------------------------------
//  Creazione del personaggio
// ---------------------------------------------------------------------------
static int sCrPage = 0, sCrSel = 0, sCrTags = 0;
static std::string sCrName = "Viandante";
static int sCrSpecial[ST_COUNT];
static bool sCrTag[SK_COUNT];
static bool sCrFemale = false;
static bool sCrInit = false;

static int CrPointsLeft() {
    int sum = 0;
    for (int i = 0; i < ST_COUNT; i++) sum += sCrSpecial[i];
    return 40 - sum;
}

void CreateUpdate() {
    if (!sCrInit) {
        for (int i = 0; i < ST_COUNT; i++) sCrSpecial[i] = 5;
        for (int i = 0; i < SK_COUNT; i++) sCrTag[i] = false;
        sCrPage = 0;
        sCrSel = 0;
        sCrTags = 0;
        sCrInit = true;
    }
    float Wd = UIW();
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (sCrPage == 1) sCrPage = 0;
        else { sCrInit = false; SetGameState(GS_MENU); }
        return;
    }
    if (sCrPage == 0) {
        // righe: 0 nome, 1 aspetto, 2..8 SPECIAL, 9 continua
        int n = 10;
        sCrSel = VerticalNav(sCrSel, n, sCrSel != 0); // mentre si scrive il nome W/S sono lettere
        for (int i = 0; i < n; i++)
            if (UIHover(Wd / 2 - 420, 150 + i * 46, 560, 40) && Vector2Length(GetMouseDelta()) > 0) sCrSel = i;
        if (sCrSel == 0 && PlatformTouch()) {
            // sul telefono il nome si scrive nella finestra con la tastiera del sistema
            if (UIClicked(Wd / 2 - 420, 150 - 4, 560, 40) || Confirm()) PlatformAskText("nome", "Nome del personaggio", sCrName, 18);
        }
        std::string typed;
        if (PlatformTakeText("nome", typed)) {
            typed.erase(std::remove(typed.begin(), typed.end(), '|'), typed.end());
            if (!typed.empty()) sCrName = typed; // la finestra limita già la lunghezza
        }
        if (sCrSel == 0) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (sCrName.size() < 18 && ch >= 32 && ch != '|') {
                    if (ch < 128) sCrName += (char)ch;
                    else if (ch < 256) { sCrName += (char)(0xC0 | (ch >> 6)); sCrName += (char)(0x80 | (ch & 0x3F)); }
                }
                ch = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && !sCrName.empty()) {
                do sCrName.pop_back();
                while (!sCrName.empty() && (sCrName.back() & 0xC0) == 0x80);
                if (!sCrName.empty() && (unsigned char)sCrName.back() >= 0xC0) sCrName.pop_back();
            }
        }
        bool inc = IsKeyPressed(KEY_RIGHT) || (sCrSel != 0 && IsKeyPressed(KEY_D));
        bool dec = IsKeyPressed(KEY_LEFT) || (sCrSel != 0 && IsKeyPressed(KEY_A));
        if (sCrSel >= 2 && sCrSel <= 8) {
            if (UIClicked(Wd / 2 - 20, 150 + sCrSel * 46, 30, 36)) dec = true;
            if (UIClicked(Wd / 2 + 70, 150 + sCrSel * 46, 30, 36)) inc = true;
        }
        if (sCrSel == 1 && (inc || dec || UIClicked(Wd / 2 - 20, 150 + 46, 200, 36))) { sCrFemale = !sCrFemale; PlaySfx(SFX_UI_MOVE); }
        if (sCrSel >= 2 && sCrSel <= 8) {
            int& v = sCrSpecial[sCrSel - 2];
            if (inc && v < 10 && CrPointsLeft() > 0) { v++; PlaySfx(SFX_UI_MOVE, 0.5f); }
            if (dec && v > 1) { v--; PlaySfx(SFX_UI_MOVE, 0.5f); }
        }
        bool cont = (sCrSel == 9 && Confirm()) || UIClicked(Wd / 2 - 140, 150 + 9 * 46, 280, 40);
        if (cont) {
            if (sCrName.empty()) sCrName = "Viandante";
            if (CrPointsLeft() > 0) { Notify("Distribuisci tutti i punti S.P.E.C.I.A.L.", Col(255, 200, 120)); PlaySfx(SFX_UI_BACK); }
            else { sCrPage = 1; sCrSel = 0; PlaySfx(SFX_UI_OK); }
        }
    } else {
        int n = SK_COUNT + 1;
        sCrSel = VerticalNav(sCrSel, n);
        for (int i = 0; i < n; i++)
            if (UIHover(Wd / 2 - 420, 140 + i * 40, 520, 36) && Vector2Length(GetMouseDelta()) > 0) sCrSel = i;
        bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && UIHover(Wd / 2 - 420, 140 + sCrSel * 40, 520, 36);
        if ((Confirm() || click) && sCrSel < SK_COUNT) {
            if (sCrTag[sCrSel]) { sCrTag[sCrSel] = false; sCrTags--; }
            else if (sCrTags < 3) { sCrTag[sCrSel] = true; sCrTags++; }
            PlaySfx(SFX_UI_MOVE);
        } else if ((Confirm() || click) && sCrSel == SK_COUNT) {
            if (sCrTags < 3) { Notify("Contrassegna tre abilità.", Col(255, 200, 120)); PlaySfx(SFX_UI_BACK); return; }
            NewGame(sNewOpenWorld);
            Player& P = G.player;
            P.name = sCrName;
            P.female = sCrFemale;
            for (int i = 0; i < ST_COUNT; i++) P.special[i] = sCrSpecial[i];
            for (int i = 0; i < SK_COUNT; i++) P.tagged[i] = sCrTag[i];
            P.hp = MaxHP();
            P.ap = MaxAP();
            sCrInit = false;
            PlaySfx(SFX_UI_OK);
            SaveGame(0);
        }
    }
}

void CreateDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, PIP_BG);
    float S = UIS();
    for (float ly = 0; ly < 720; ly += 3) DrawRectangle(0, (int)(ly * S), GetScreenWidth(), std::max(1, (int)S), Color{0, 0, 0, 50});
    if (sCrPage == 0) {
        UIText("CREAZIONE DEL PERSONAGGIO", Wd / 2, 40, 36, PIP, 1);
        UIText(Fmt("Punti da distribuire: %d", CrPointsLeft()), Wd / 2, 92, 22, CrPointsLeft() > 0 ? Col(255, 230, 120) : PIP, 1);
        float x = Wd / 2 - 400;
        auto Row = [&](int i, const std::string& l) {
            float y = 150 + i * 46;
            if (i == sCrSel) UIRect(x - 14, y - 4, 560, 40, PIP_DARK);
            UIText(l, x, y, 26, PIP);
        };
        Row(0, "Nome");
        std::string nm = sCrName + (sCrSel == 0 && (int)(G.time * 2) % 2 ? "_" : "");
        UIText(nm, Wd / 2 - 20, 150, 26, Color{220, 255, 225, 255});
        Row(1, "Aspetto");
        UIText(sCrFemale ? "< Donna >" : "< Uomo >", Wd / 2 - 20, 150 + 46, 26, Color{220, 255, 225, 255});
        for (int i = 0; i < ST_COUNT; i++) {
            Row(2 + i, SPECIAL_NAMES[i]);
            float y = 150 + (2 + i) * 46;
            UIText("<", Wd / 2 - 5, y, 26, sCrSpecial[i] > 1 ? PIP : PIP_DARK, 1);
            UIText(Fmt("%d", sCrSpecial[i]), Wd / 2 + 40, y, 26, Color{220, 255, 225, 255}, 1);
            UIText(">", Wd / 2 + 85, y, 26, sCrSpecial[i] < 10 && CrPointsLeft() > 0 ? PIP : PIP_DARK, 1);
            for (int k = 0; k < 10; k++) UIRect(Wd / 2 + 110 + k * 9, y + 8, 6, 16, k < sCrSpecial[i] ? PIP : PIP_DARK);
        }
        MenuItem("CONTINUA", Wd / 2, 150 + 9 * 46, 28, sCrSel == 9, CrPointsLeft() == 0);
        float dx = Wd / 2 + 260, dy = 150;
        std::string title = sCrSel >= 2 && sCrSel <= 8 ? SPECIAL_NAMES[sCrSel - 2] : (sCrSel == 0 ? "Nome" : (sCrSel == 1 ? "Aspetto" : "Pronto?"));
        std::string desc = sCrSel >= 2 && sCrSel <= 8 ? SPECIAL_DESC[sCrSel - 2] : (sCrSel == 0 ? "Scrivi il nome del tuo personaggio." : (sCrSel == 1 ? "Scegli l'aspetto del tuo personaggio (visibile in terza persona, tasto V)." : "Conferma le statistiche e passa alla scelta delle abilità."));
        UIText(title, dx, dy, 26, PIP);
        float yy = dy + 40;
        for (auto& l : UIWrap(desc, 19, std::min(360.0f, Wd - dx - 30))) { UIText(l, dx, yy, 19, PIP_DIM); yy += 24; }
        yy += 20;
        int saved[ST_COUNT];
        for (int i = 0; i < ST_COUNT; i++) { saved[i] = G.player.special[i]; G.player.special[i] = sCrSpecial[i]; }
        UIText(Fmt("Energia %d   Fiato %d   Peso max %d", (int)MaxHP(), (int)MaxAP(), (int)CarryMax()), dx, yy, 18, PIP);
        for (int i = 0; i < ST_COUNT; i++) G.player.special[i] = saved[i];
        UIText("Frecce: scegli e modifica   INVIO: conferma   ESC: indietro", Wd / 2, 680, 16, PIP_DIM, 1);
    } else {
        UIText("ABILITÀ CONTRASSEGNATE", Wd / 2, 40, 36, PIP, 1);
        UIText(Fmt("Scegli 3 abilità (+15): %d / 3", sCrTags), Wd / 2, 90, 22, sCrTags < 3 ? Col(255, 230, 120) : PIP, 1);
        float x = Wd / 2 - 400;
        int saved[ST_COUNT];
        for (int i = 0; i < ST_COUNT; i++) { saved[i] = G.player.special[i]; G.player.special[i] = sCrSpecial[i]; }
        bool savedTag[SK_COUNT];
        for (int i = 0; i < SK_COUNT; i++) { savedTag[i] = G.player.tagged[i]; G.player.tagged[i] = sCrTag[i]; }
        for (int i = 0; i < SK_COUNT; i++) {
            float y = 140 + i * 40;
            if (i == sCrSel) UIRect(x - 14, y - 4, 520, 36, PIP_DARK);
            if (sCrTag[i]) UIRect(x - 4, y + 8, 10, 10, PIP);
            UIText(SKILL_NAMES[i], x + 16, y, 24, PIP);
            UIText(Fmt("%d", SkillValue((Skill)i)), x + 480, y, 24, sCrTag[i] ? Color{220, 255, 225, 255} : PIP, 2);
        }
        for (int i = 0; i < ST_COUNT; i++) G.player.special[i] = saved[i];
        for (int i = 0; i < SK_COUNT; i++) G.player.tagged[i] = savedTag[i];
        MenuItem("INIZIA L'AVVENTURA", Wd / 2 - 150, 140 + SK_COUNT * 40 + 10, 26, sCrSel == SK_COUNT, sCrTags == 3);
        if (sCrSel < SK_COUNT) {
            float dx = Wd / 2 + 180;
            UIText(SKILL_NAMES[sCrSel], dx, 140, 26, PIP);
            float yy = 180;
            for (auto& l : UIWrap(SKILL_DESC[sCrSel], 19, std::min(380.0f, Wd - dx - 30))) { UIText(l, dx, yy, 19, PIP_DIM); yy += 24; }
        }
        UIText("INVIO: contrassegna   ESC: indietro", Wd / 2, 680, 16, PIP_DIM, 1);
    }
}

// ---------------------------------------------------------------------------
//  Dialogo
// ---------------------------------------------------------------------------
static float DlgTop() { return 300; }
static float DlgOptY() {
    auto lines = UIWrap(G.dlg.text, 21, 960);
    return DlgTop() + 50 + lines.size() * 27 + 18;
}

void DialogueUpdate() {
    Dialogue& d = G.dlg;
    d.typeT += GetFrameTime();
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        SetGameState(GS_PLAY);
        return;
    }
    int n = (int)d.opts.size();
    if (n == 0) { SetGameState(GS_PLAY); return; }
    d.sel = VerticalNav(d.sel, n);
    float Wd = UIW();
    float oy = DlgOptY();
    bool choose = Confirm() || IsKeyPressed(KEY_E);
    for (int i = 0; i < n; i++) {
        if (IsKeyPressed(KEY_ONE + i)) { d.sel = i; choose = true; }
        if (UIHover(Wd / 2 - 490, oy + i * 34 - 3, 980, 32)) {
            if (Vector2Length(GetMouseDelta()) > 0) d.sel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { d.sel = i; choose = true; }
        }
    }
    if (!choose) return;
    if (d.typeT * 70 < d.text.size()) { d.typeT = 1000; return; }
    PlaySfx(SFX_UI_OK, 0.6f);
    auto act = d.opts[d.sel].act;
    bool siteDialogue = d.npc < 0;
    act();
    if (siteDialogue) return;
    if (G.state != GS_DIALOGUE) return;
    if (d.end) { SetGameState(GS_PLAY); return; }
    d.node = d.next;
    DialogueBuild();
}

void DialogueDraw() {
    Dialogue& d = G.dlg;
    float Wd = UIW();
    float top = DlgTop();
    UIRect(0, top, Wd, 720 - top, Color{0, 0, 0, 185});
    UIRect(0, top, Wd, 2, PIP_DIM);
    float x = Wd / 2 - 480;
    UIText(d.speaker, x, top + 12, 26, Col(255, 220, 150));
    size_t shown = std::min(d.text.size(), (size_t)(d.typeT * 70));
    while (shown < d.text.size() && (d.text[shown] & 0xC0) == 0x80) shown++;
    auto full = UIWrap(d.text, 21, 960);
    size_t remaining = shown;
    float y = top + 50;
    for (auto& l : full) {
        size_t take = std::min(remaining, l.size());
        while (take < l.size() && (l[take] & 0xC0) == 0x80) take++;
        UIText(l.substr(0, take), x, y, 21, Col(235, 230, 215));
        remaining = remaining > l.size() + 1 ? remaining - l.size() - 1 : 0;
        y += 27;
    }
    float oy = DlgOptY();
    for (int i = 0; i < (int)d.opts.size(); i++) {
        bool sel = i == d.sel;
        float yy = oy + i * 34;
        if (sel) UIRect(x - 10, yy - 3, 980, 32, Color{30, 110, 55, 170});
        UIText(Fmt("%d. ", i + 1) + d.opts[i].text, x, yy, 21, sel ? Color{220, 255, 225, 255} : PIP);
    }
}

// ---------------------------------------------------------------------------
//  Pausa
// ---------------------------------------------------------------------------
void PauseUpdate() {
    const int n = 6;
    sMenuSel = VerticalNav(sMenuSel, n);
    float Wd = UIW();
    int clicked = -1;
    for (int i = 0; i < n; i++)
        if (UIHover(Wd / 2 - 160, 250 + i * 54 - 4, 320, 42)) {
            if (Vector2Length(GetMouseDelta()) > 0) sMenuSel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
        }
    if (Confirm()) clicked = sMenuSel;
    if (IsKeyPressed(KEY_ESCAPE)) { SetGameState(GS_PLAY); return; }
    switch (clicked) {
        case 0: SetGameState(GS_PLAY); break;
        case 1:
            if (G.chased) { Notify("Non puoi salvare mentre qualcuno ti insegue.", Col(255, 160, 100)); break; }
            SaveLoadOpen(true, GS_PAUSE);
            break;
        case 2: SaveLoadOpen(false, GS_PAUSE); break;
        case 3: OptionsOpen(GS_PAUSE); break;
        case 4: sMenuSel = 0; SetGameState(GS_MENU); break;
        case 5:
            if (NetAvailable()) OnlineOpen(GS_PAUSE);
            else G.quit = true;
            break;
        default: break;
    }
    if (clicked >= 0) PlaySfx(SFX_UI_OK);
}

void PauseDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 170});
    UIText("PAUSA", Wd / 2, 150, 56, PIP, 1);
    const char* items[] = {"RIPRENDI", "SALVA PARTITA", "CARICA PARTITA", "OPZIONI", "MENU PRINCIPALE", NetAvailable() ? "MULTIGIOCATORE" : "ESCI DAL GIOCO"};
    for (int i = 0; i < 6; i++) MenuItem(items[i], Wd / 2, 250 + i * 54, 28, i == sMenuSel);
    UIText("WASD muovi · Mouse guarda · E interagisci · 1-7 strumenti · Clic usa l'attrezzo · Clic destro zoom · Q osserva · TAB Pip-Boy", Wd / 2, 600, 16, PIP_DIM, 1);
    UIText("Shift corri · C accovacciati · Spazio salta · B esca · G barattolo · H kit medico · F mantello ottico · V visuale · L luce · F5/F9", Wd / 2, 624, 16, PIP_DIM, 1);
    if (PlatformTouch()) UIText("Sul telefono: pollice sinistro per muoverti, dito a destra per guardare, pulsanti per tutto il resto.", Wd / 2, 576, 16, PIP, 1);
    if (NetAvailable()) UIText("Multigiocatore: T chat · Z saluta · P elenco dei giocatori · mappa del Pip-Boy per raggiungere un amico", Wd / 2, 648, 16, PIP_DIM, 1);
}

// ---------------------------------------------------------------------------
//  Svenimento: nessuno muore nelle Terre Bruciate, al massimo ci si risveglia in clinica
// ---------------------------------------------------------------------------
static float sFaintT = 0;

void FaintWakeUp() {
    Player& P = G.player;
    int lost = P.inv.Count(I_CAPS) / 10;
    if (lost > 0) TakeItem(I_CAPS, lost);
    P.rads *= 0.5f;
    P.hp = HPCap() * 0.6f;
    P.healOverTime = 0;
    P.stealthT = 0;
    P.catchImmune = 8;
    P.vel = V3(0, 0, 0);
    bool town = W.locations.size() > 1 && W.locations[1].discovered;
    if (town) {
        G.pendingZone = ZONE_WASTE;
        G.pendingPos = V3(21, 0, 37);
        G.pendingYaw = 0;
        Notify(Fmt("Ti risvegli nella clinica di Ferrocittà. La dottoressa Rosi ti ha rimesso in piedi (onorario: %d tappi).", lost), Col(255, 230, 160));
    } else {
        G.pendingZone = ZONE_VAULT;
        G.pendingPos = V3(18.7f, 0, 7);
        G.pendingYaw = PI / 2;
        Notify(Fmt("Ti risvegli nell'infermeria del Rifugio. Qualcuno ti ha riportato a casa (e ha preso %d tappi per il disturbo).", lost), Col(255, 230, 160));
    }
    G.fade = 0.001f;
    G.timeOfDay += 6;
    for (auto& e : G.ents)
        if (e.ai == AI_CHASE || e.aware) {
            e.aware = false;
            e.alert = 0;
            e.ai = AI_RETURN;
        }
    SetGameState(GS_PLAY);
}

void FaintUpdate() {
    sFaintT += GetFrameTime();
    if (sFaintT < 1.5f) return;
    float Wd = UIW();
    sMenuSel = VerticalNav(sMenuSel, 2);
    int clicked = -1;
    for (int i = 0; i < 2; i++)
        if (UIHover(Wd / 2 - 220, 420 + i * 56 - 4, 440, 42)) {
            if (Vector2Length(GetMouseDelta()) > 0) sMenuSel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
        }
    if (Confirm()) clicked = sMenuSel;
    if (clicked == 0) {
        sFaintT = 0;
        sMenuSel = 0;
        FaintWakeUp();
    } else if (clicked == 1) {
        int s = NewestSave();
        sFaintT = 0;
        sMenuSel = 0;
        if (s < 0 || !LoadGame(s)) FaintWakeUp();
    }
}

void FaintDraw() {
    float Wd = UIW();
    float a = Clamp01(sFaintT / 1.5f);
    UIRect(0, 0, Wd, 720, Color{10, 8, 6, (unsigned char)(220 * a)});
    UIText("TI SERVE UNA PAUSA", Wd / 2, 220, 64, ColAlpha(Col(255, 210, 150), a), 1);
    UIText("Il cammino ti ha affaticato. Riposa al Rifugio e riprendi da dove eri arrivato.", Wd / 2, 320, 20, ColAlpha(Col(230, 210, 180), a), 1);
    if (sFaintT >= 1.5f) {
        MenuItem("RIPRENDI IL CAMMINO", Wd / 2, 420, 28, sMenuSel == 0);
        MenuItem("CARICA L'ULTIMO SALVATAGGIO", Wd / 2, 476, 28, sMenuSel == 1);
    }
}

// ---------------------------------------------------------------------------
//  Finale
// ---------------------------------------------------------------------------
static std::vector<std::string> sEndSlides;

void EndingStart() {
    Player& P = G.player;
    sEndSlides.clear();
    sEndSlides.push_back("Quando l'Atlante dell'Alba tornò in linea, sullo schermo non comparve un ordine. Comparvero i nomi di chi aveva reso possibile quel viaggio: cartografi, custodi, giardinieri e persone che avevano imparato di nuovo a parlarsi.");
    if (G.flags["atlas_priority"] == 1)
        sEndSlides.push_back("La prima mappa condivisa seguì l'acqua. Le comunità impararono a leggere insieme piogge, cisterne e sentieri. Nessuno poteva aprire da solo tutte le valvole: quel limite divenne la loro promessa.");
    else
        sEndSlides.push_back("La prima mappa condivisa seguì i semi. Ogni comunità custodì una piccola banca e insegnò alle altre a farla crescere. Il giardino non aveva un centro: aveva molti ingressi.");
    if (G.flags["atlas_voice"] == 1)
        sEndSlides.push_back("Le voci raccolte lungo il viaggio entrarono nell'archivio comune. Storie diverse rimasero diverse: Lumen imparò ad ascoltarle tutte, e a lasciare sempre spazio a una nuova voce.");
    else
        sEndSlides.push_back("Le mappe e le conoscenze pratiche viaggiarono libere. I ricordi personali rimasero con chi li aveva affidati al registratore. Anche un silenzio poteva essere una scelta da rispettare.");
    int deal = G.flags["boss_deal"];
    if (deal == 3)
        sEndSlides.push_back("Al deposito, la radio riparata di Neri tornò a suonare. Fra una canzone e l'altra, i custodi trasmettevano gli orari delle carovane e segnavano nuovi sentieri sulle mappe.");
    else if (deal == 1)
        sEndSlides.push_back("Il patto con Neri aprì il deposito ai viaggiatori. I custodi condivisero i loro ricambi con Ferrocittà, che ricambiò con acqua e notizie dalle fattorie.");
    else if (deal == 2)
        sEndSlides.push_back("Neri investì i tappi ricevuti in una radio nuova. Il deposito divenne una tappa rumorosa, ma accogliente, lungo la rotta dell'Atlante.");
    else
        sEndSlides.push_back("Il vecchio deposito rimase appartato. Le sue luci, però, comparvero sulla nuova mappa: ora esisteva un sentiero per raggiungerlo, quando i suoi custodi avessero voluto aprirsi agli altri.");
    if (QUESTS[Q_MEDS].done)
        sEndSlides.push_back("I rifornimenti portarono sollievo alla clinica di Rosi. Nei mesi seguenti, i suoi appunti di erboristeria furono fra i documenti più consultati della rete.");
    if (QUESTS[Q_WATER].done)
        sEndSlides.push_back("Sotto la torre dell'acqua di Ferrocittà nacque un orto. La pompa riparata era diventata una cosa quotidiana: proprio per questo, ogni mattina, qualcuno se ne prendeva cura.");
    if (QUESTS[Q_ROACH].done)
        sEndSlides.push_back("La Stazione Atomo riaprì senza disturbare gli animali. Tano lasciò una piccola zona di raccolta fuori dai magazzini: una soluzione semplice che i viaggiatori impararono a copiare.");
    if (QUESTS[Q_BEAST].done)
        sEndSlides.push_back("La fotografia della grande creatura della gola insegnò a Jack dove passare e dove lasciare spazio. Sulla mappa, il suo territorio fu segnato come un luogo da osservare con rispetto.");
    if (QUESTS[Q_DOG].done)
        sEndSlides.push_back("Ruggine continuò a esplorare accanto a " + P.name + ". Rita sosteneva che fosse lui a scegliere la strada, e il più delle volte aveva ragione.");
    sEndSlides.push_back("Il Rifugio 33 non era più una porta chiusa sulla montagna. La cella di fusione sosteneva la rete, e sul tavolo di Ferri c'era un atlante pieno di spazi bianchi. Il viaggio di " + P.name + " aveva dato loro un significato: luoghi ancora da conoscere.");
    G.endingSlide = 0;
    G.endingT = 0;
    SetGameState(GS_ENDING);
    SetRadioStation(1);
}

void EndingUpdate(float dt) {
    G.endingT += dt;
    bool next = Confirm() || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_ESCAPE);
    if (G.endingSlide < (int)sEndSlides.size()) {
        if (next && G.endingT > 0.6f) {
            G.endingSlide++;
            G.endingT = 0;
            PlaySfx(SFX_UI_OK, 0.5f);
        }
        return;
    }
    float Wd = UIW();
    sMenuSel = VerticalNav(sMenuSel, 2);
    int clicked = -1;
    for (int i = 0; i < 2; i++)
        if (UIHover(Wd / 2 - 220, 540 + i * 54 - 4, 440, 42)) {
            if (Vector2Length(GetMouseDelta()) > 0) sMenuSel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
        }
    if (Confirm() && G.endingT > 0.5f) clicked = sMenuSel;
    if (clicked == 0) {
        SaveGame(0);
        SetGameState(GS_PLAY);
    } else if (clicked == 1) {
        sMenuSel = 0;
        SetGameState(GS_MENU);
    }
}

void EndingDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Color{8, 7, 5, 255});
    Color txt = Col(235, 210, 165);
    if (G.endingSlide < (int)sEndSlides.size()) {
        float a = Clamp01(G.endingT / 1.0f);
        const std::string& s = sEndSlides[G.endingSlide];
        auto lines = UIWrap(s, 26, 860);
        float y = 360 - lines.size() * 18;
        for (auto& l : lines) { UIText(l, Wd / 2, y, 26, ColAlpha(txt, a), 1); y += 36; }
        UIText(Fmt("%d / %d", G.endingSlide + 1, (int)sEndSlides.size()), Wd / 2, 660, 16, ColAlpha(txt, 0.5f), 1);
        return;
    }
    Player& P = G.player;
    UIText("FINE", Wd / 2, 90, 80, txt, 1);
    UIText("Grazie per aver giocato a DUSTFALL", Wd / 2, 200, 24, txt, 1);
    std::string stats = Fmt("Livello %d    Foto %d    Missioni completate %d    Volte scoperto %d    Reputazione %d", P.level, (int)P.photos.size(), P.questsDone, P.timesCaught, P.karma);
    UIText(stats, Wd / 2, 270, 20, ColAlpha(txt, 0.8f), 1);
    UIText(Fmt("Tempo di gioco: %dh %02dm", (int)(P.playTime / 3600), ((int)P.playTime / 60) % 60), Wd / 2, 304, 20, ColAlpha(txt, 0.8f), 1);
    MenuItem("CONTINUA A ESPLORARE", Wd / 2, 540, 28, sMenuSel == 0);
    MenuItem("MENU PRINCIPALE", Wd / 2, 594, 28, sMenuSel == 1);
}

// ---------------------------------------------------------------------------
//  Riposo
// ---------------------------------------------------------------------------
static int sSleepHours = 8;
static float sSleepProgress = -1;

void SleepOpen() {
    sSleepHours = 8;
    sSleepProgress = -1;
    SetGameState(GS_SLEEP);
}

void SleepUpdate(float dt) {
    Player& P = G.player;
    if (sSleepProgress >= 0) {
        float step = dt * 6.0f;
        sSleepProgress += step;
        G.timeOfDay += step;
        P.hp = std::min(HPCap(), P.hp + MaxHP() * 0.15f * step);
        if (sSleepProgress >= sSleepHours) {
            P.hp = HPCap();
            Notify(Fmt("Hai riposato %d ore.", sSleepHours));
            SetGameState(GS_PLAY);
            SaveGame(0);
        }
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) { SetGameState(GS_PLAY); return; }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) sSleepHours = std::min(24, sSleepHours + 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) sSleepHours = std::max(1, sSleepHours - 1);
    float Wd = UIW();
    if (UIClicked(Wd / 2 - 120, 330, 40, 40)) sSleepHours = std::max(1, sSleepHours - 1);
    if (UIClicked(Wd / 2 + 80, 330, 40, 40)) sSleepHours = std::min(24, sSleepHours + 1);
    if (Confirm() || UIClicked(Wd / 2 - 100, 420, 200, 44)) sSleepProgress = 0;
}

void SleepDraw() {
    float Wd = UIW();
    if (sSleepProgress >= 0) {
        float a = Clamp01(sSleepProgress / 0.6f) * Clamp01((sSleepHours - sSleepProgress) / 0.6f + 0.3f);
        UIRect(0, 0, Wd, 720, Color{0, 0, 0, (unsigned char)(255 * std::min(1.0f, a + 0.3f))});
        UIText(Fmt("Riposo... %d / %d ore", (int)sSleepProgress, sSleepHours), Wd / 2, 340, 28, PIP, 1);
        return;
    }
    UIRect(Wd / 2 - 260, 230, 520, 260, Color{4, 18, 9, 230});
    UIRectLines(Wd / 2 - 260, 230, 520, 260, 2, PIP);
    UIText("Per quanto vuoi riposare?", Wd / 2, 260, 26, PIP, 1);
    UIText("<", Wd / 2 - 100, 334, 32, PIP, 1);
    UIText(Fmt("%d ore", sSleepHours), Wd / 2, 334, 32, Color{220, 255, 225, 255}, 1);
    UIText(">", Wd / 2 + 100, 334, 32, PIP, 1);
    MenuItem("DORMI", Wd / 2, 426, 28, true);
}

// ---------------------------------------------------------------------------
//  Salvataggi
// ---------------------------------------------------------------------------
static bool sSaving = false;
static GameState sSaveBack = GS_MENU;

void SaveLoadOpen(bool saving, GameState back) {
    sSaving = saving;
    sSaveBack = back;
    sMenuSel = saving ? 1 : 0;
    SetGameState(GS_SAVELOAD);
}

static const char* SlotName(int i) {
    static std::string s;
    if (i == 0) return "Salvataggio automatico";
    if (i == 6) return "Salvataggio rapido";
    s = Fmt("Slot %d", i);
    return s.c_str();
}

void SaveLoadUpdate() {
    if (IsKeyPressed(KEY_ESCAPE)) { SetGameState(sSaveBack); return; }
    const int n = 8;
    sMenuSel = VerticalNav(sMenuSel, n);
    float Wd = UIW();
    int clicked = -1;
    for (int i = 0; i < n; i++)
        if (UIHover(Wd / 2 - 400, 160 + i * 58 - 4, 800, 50)) {
            if (Vector2Length(GetMouseDelta()) > 0) sMenuSel = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
        }
    if (Confirm()) clicked = sMenuSel;
    if (clicked < 0) return;
    if (clicked == 7) { SetGameState(sSaveBack); return; }
    int slot = clicked;
    if (sSaving) {
        if (slot == 0 || slot == 6) { PlaySfx(SFX_UI_BACK); return; }
        if (SaveGame(slot)) {
            Notify("Partita salvata.");
            PlaySfx(SFX_UI_OK);
            SetGameState(GS_PLAY);
        }
    } else {
        if (!SaveExists(slot)) { PlaySfx(SFX_UI_BACK); return; }
        PlaySfx(SFX_UI_OK);
        LoadGame(slot);
    }
}

void SaveLoadDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 215});
    UIText(sSaving ? "SALVA PARTITA" : "CARICA PARTITA", Wd / 2, 70, 40, PIP, 1);
    for (int i = 0; i < 8; i++) {
        float y = 160 + i * 58;
        bool sel = i == sMenuSel;
        if (i == 7) { MenuItem("INDIETRO", Wd / 2, y + 6, 26, sel); continue; }
        bool exists = SaveExists(i);
        bool usable = sSaving ? (i != 0 && i != 6) : exists;
        if (sel) UIRect(Wd / 2 - 400, y - 4, 800, 50, Color{30, 110, 55, 150});
        UIRectLines(Wd / 2 - 400, y - 4, 800, 50, 1, usable ? PIP_DIM : PIP_DARK);
        UIText(SlotName(i), Wd / 2 - 384, y + 6, 22, usable ? PIP : PIP_DARK);
        UIText(exists ? SaveInfo(i) : "- vuoto -", Wd / 2 + 384, y + 8, 18, usable ? PIP_DIM : PIP_DARK, 2);
    }
}

// ---------------------------------------------------------------------------
//  Opzioni
// ---------------------------------------------------------------------------
static GameState sOptBack = GS_MENU;

void OptionsOpen(GameState back) {
    sOptBack = back;
    sMenuSel = 0;
    SetGameState(GS_OPTIONS);
}

void OptionsUpdate() {
    const int n = 8;
    Settings& s = G.settings;
    if (IsKeyPressed(KEY_ESCAPE)) { SaveSettings(); SetGameState(sOptBack); return; }
    sMenuSel = VerticalNav(sMenuSel, n);
    float Wd = UIW();
    int dir = 0;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dir = 1;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) dir = -1;
    for (int i = 0; i < n; i++) {
        float y = 170 + i * 52;
        if (UIHover(Wd / 2 - 380, y - 4, 760, 44) && Vector2Length(GetMouseDelta()) > 0) sMenuSel = i;
        if (UIClicked(Wd / 2 + 60, y - 4, 40, 44)) { sMenuSel = i; dir = -1; }
        if (UIClicked(Wd / 2 + 300, y - 4, 40, 44)) { sMenuSel = i; dir = 1; }
    }
    bool enter = Confirm() || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && UIHover(Wd / 2 - 380, 170 + sMenuSel * 52 - 4, 420, 44));
    switch (sMenuSel) {
        case 0: s.sensitivity = Clamp(s.sensitivity + dir * 0.1f, 0.2f, 3.0f); break;
        case 1: s.volume = Clamp(s.volume + dir * 0.1f, 0.0f, 1.0f); break;
        case 2: s.radioVolume = Clamp(s.radioVolume + dir * 0.1f, 0.0f, 1.0f); break;
        case 3: s.fov = Clamp(s.fov + dir * 5.0f, 55.0f, 110.0f); break;
        case 4: if (dir || enter) s.invertY = !s.invertY; break;
        case 5: if (dir || enter) s.showFps = !s.showFps; break;
        case 6: if (dir || enter) { s.fullscreen = !s.fullscreen; ApplySettings(); } break;
        case 7: if (enter) { SaveSettings(); SetGameState(sOptBack); return; } break;
    }
    if (dir) { PlaySfx(SFX_UI_MOVE, 0.5f); ApplySettings(); }
}

void OptionsDraw() {
    float Wd = UIW();
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 215});
    UIText("OPZIONI", Wd / 2, 70, 40, PIP, 1);
    Settings& s = G.settings;
    std::string vals[] = {Fmt("%.1f", s.sensitivity), Fmt("%d%%", (int)roundf(s.volume * 100)), Fmt("%d%%", (int)roundf(s.radioVolume * 100)), Fmt("%d°", (int)s.fov),
                          s.invertY ? "Sì" : "No", s.showFps ? "Sì" : "No", s.fullscreen ? "Sì" : "No", ""};
    const char* names[] = {"Sensibilità del mouse", "Volume generale", "Volume radio", "Campo visivo", "Inverti asse Y", "Mostra FPS", "Schermo intero (F11)", "INDIETRO"};
    for (int i = 0; i < 8; i++) {
        float y = 170 + i * 52;
        bool sel = i == sMenuSel;
        if (i == 7) { MenuItem(names[i], Wd / 2, y + 20, 26, sel); continue; }
        if (sel) UIRect(Wd / 2 - 380, y - 4, 760, 44, Color{30, 110, 55, 150});
        UIText(names[i], Wd / 2 - 360, y + 4, 24, PIP);
        UIText("<", Wd / 2 + 80, y + 4, 24, PIP, 1);
        UIText(vals[i], Wd / 2 + 200, y + 4, 24, Color{220, 255, 225, 255}, 1);
        UIText(">", Wd / 2 + 320, y + 4, 24, PIP, 1);
    }
}
