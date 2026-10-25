// Pip-Boy, baratto, saccheggio e avanzamento di livello.
#include "game.h"
#include "net.h"

static const char* TABS[] = {"STAT", "INV", "DATI", "MAPPA", "RADIO"};
static const char* SUBTABS[][5] = {
    {"STATO", "S.P.E.C.I.A.L.", "ABILITÀ", "TALENTI", "GENERALE"},
    {"ATTREZZI", "ABBIGLIAMENTO", "AIUTI", "VARIE", ""},
    {"MISSIONI", "LUOGHI", "CATALOGO", "", ""},
    {"", "", "", "", ""},
    {"", "", "", "", ""},
};
static const int SUBCOUNT[] = {5, 4, 3, 1, 1};

struct PipRect { float x, y, w, h; };

// cornice del dispositivo e schermo con linee di scansione
static PipRect PipFrame() {
    float Wd = UIW();
    float sw = std::min(Wd - 80, 1160.0f);
    float x = (Wd - sw) / 2, y = 30, h = 660;
    UIRect(0, 0, Wd, 720, Color{0, 0, 0, 200});
    UIRect(x - 22, y - 18, sw + 44, h + 36, Col(58, 54, 40));
    UIRectLines(x - 22, y - 18, sw + 44, h + 36, 3, Col(30, 28, 20));
    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? x + sw + 8 : x - 12, sy = (i & 2) ? y + h + 6 : y - 8;
        UIRect(sx - 4, sy - 4, 8, 8, Col(120, 115, 95));
    }
    UIRect(x, y, sw, h, PIP_BG);
    float S = UIS();
    for (float ly = y; ly < y + h; ly += 3) DrawRectangle((int)(x * S), (int)(ly * S), (int)(sw * S), std::max(1, (int)(1 * S)), Color{0, 0, 0, 55});
    float roll = fmodf(G.time * 60, h);
    UIRect(x, y + roll, sw, 18, Color{80, 255, 130, 8});
    return {x, y, sw, h};
}

static std::vector<int> InvList(int sub) {
    std::vector<int> out;
    for (auto& s : G.player.inv.items) {
        ItemType t = ITEMS[s.id].type;
        bool ok = false;
        switch (sub) {
            case 0: ok = t == IT_TOOL; break;
            case 1: ok = t == IT_ARMOR; break;
            case 2: ok = t == IT_AID; break;
            case 3: ok = (t == IT_MISC || t == IT_KEY) && s.id != I_CAPS; break;
        }
        if (ok) out.push_back(s.id);
    }
    std::sort(out.begin(), out.end(), [](int a, int b) { return strcmp(ITEMS[a].name, ITEMS[b].name) < 0; });
    return out;
}

static std::vector<std::string> ItemStats(int id) {
    std::vector<std::string> s;
    const ItemDef& d = ITEMS[id];
    if (d.type == IT_TOOL) {
        if (d.tool == TOOL_CAMERA) {
            s.push_back("Clic sinistro: scatta una foto");
            s.push_back("Clic destro: zoom");
            s.push_back(Fmt("Foto nel catalogo: %d / %d", (int)G.player.photos.size(), (int)PHOTO_SUBJECTS.size()));
        } else if (d.tool == TOOL_BAIT) {
            float dur = 12.0f + SkillValue(SK_NATURE) * 0.12f;
            if (G.player.perks[PK_ANIMALFRIEND]) dur *= 1.5f;
            s.push_back(Fmt("Attira gli animali per %d s", (int)dur));
            s.push_back(Fmt("Raggio %d m", (int)(22 + SkillValue(SK_NATURE) * 0.1f)));
            s.push_back("Lancio: clic sinistro o tasto B");
        } else if (d.tool == TOOL_NOISE) {
            s.push_back(Fmt("Rumore udibile a %d m", 25 + (G.player.perks[PK_THROWER] ? 8 : 0)));
            s.push_back("Distrae persone, mutanti e robot");
            s.push_back("Lancio: clic sinistro o tasto G");
        }
    } else if (d.type == IT_ARMOR) {
        if (d.radRes > 0) s.push_back(Fmt("Res. radiazioni +%d%%", (int)(d.radRes * 100)));
        if (d.sneak) s.push_back(Fmt("Furtività %+d", d.sneak));
        if (d.carry) s.push_back(Fmt("Peso trasportabile %+d", d.carry));
        if (d.talk) s.push_back(Fmt("Eloquenza e Baratto %+d", d.talk));
        if (id == I_EXOSUIT) s.push_back("Molto rumoroso quando cammini");
    } else if (d.type == IT_AID) {
        if (d.heal > 0) s.push_back(Fmt("PF +%d", (int)d.heal));
        if (d.rads != 0) s.push_back(Fmt("RAD %+d", (int)d.rads));
        if (d.radX > 0) s.push_back("Res. radiazioni +50% (3 min)");
        if (d.ap > 0) s.push_back(Fmt("PA +%d", (int)d.ap));
        if (d.stealth > 0) s.push_back(Fmt("Quasi invisibile per %d s", (int)d.stealth));
        if (id == I_HERBTEA) s.push_back("INT +2, PER +2 (2 min)");
        if (id == I_COFFEE) s.push_back("Recupero PA raddoppiato (1 min)");
    }
    s.push_back(Fmt("Peso %.1f   Valore %d", d.weight, d.value));
    return s;
}

static const char* KarmaTitle(int k) {
    if (k >= 250) return "Messaggero di speranza";
    if (k >= 100) return "Protettore";
    if (k >= 25) return "Viandante onesto";
    if (k > -25) return "Neutrale";
    if (k > -100) return "Furfante";
    if (k > -250) return "Predatore";
    return "Flagello delle Terre Bruciate";
}

void DropItem(int id) {
    if (ITEMS[id].type == IT_KEY) {
        Notify("Non puoi lasciare un oggetto di missione.");
        return;
    }
    if (id == G.player.armor && G.player.inv.Count(id) == 1) EquipArmor(id);
    TakeItem(id, 1);
    Interactable it;
    it.type = INT_ITEM;
    it.zone = G.player.zone;
    it.pos = G.player.pos + YawDir(G.player.yaw) * 0.8f;
    it.pos.y = GroundHeight(it.zone, it.pos + V3(0, 0.5f, 0), 1.0f);
    it.name = ITEMS[id].name;
    it.inv.Add(id, 1);
    it.size = V3(0.4f, 0.3f, 0.4f);
    G.inters.push_back(it);
    PlaySfx(SFX_UI_BACK);
}

static void DrawVaultFigure(float cx, float cy, float s, float hpFrac) {
    float S = UIS();
    Color c = PIP;
    auto L = [&](float x0, float y0, float x1, float y1) { DrawLineEx(V2((cx + x0 * s) * S, (cy + y0 * s) * S), V2((cx + x1 * s) * S, (cy + y1 * s) * S), 3 * S, c); };
    float bob = sinf(G.time * 2) * 2;
    DrawCircleLines((int)(cx * S), (int)((cy - 70 * s + bob) * S), 26 * s * S, c);
    DrawCircleLines((int)(cx * S), (int)((cy - 70 * s + bob) * S), 25 * s * S, c);
    L(-9, -76 + bob, -9, -70 + bob);
    L(9, -76 + bob, 9, -70 + bob);
    L(-12, -58 + bob, 12, -58 + bob);
    L(0, -44, 0, 20);
    L(-30, -36, 30, -36);
    L(-30, -36, -44, 6);
    L(30, -36, 50, -60);
    L(50, -60, 44, -76);
    L(0, 20, -22, 74);
    L(0, 20, 22, 74);
    L(-22, 74, -34, 74);
    L(22, 74, 34, 74);
    const char* st = hpFrac > 0.7f ? "IN FORMA" : (hpFrac > 0.35f ? "AMMACCATO" : "ESAUSTO");
    UIText(st, cx, cy + 90 * s, 20, hpFrac > 0.35f ? PIP : Col(255, 120, 90), 1);
}

void PipBoyUpdate() {
    Player& P = G.player;
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        PlaySfx(SFX_UI_BACK);
        SetGameState(GS_PLAY);
        return;
    }
    int oldTab = G.pipTab, oldSub = G.pipSub;
    if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_RIGHT_BRACKET)) G.pipTab = (G.pipTab + 1) % 5;
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_LEFT_BRACKET)) G.pipTab = (G.pipTab + 4) % 5;
    for (int i = 0; i < 5; i++)
        if (IsKeyPressed(KEY_ONE + i)) G.pipTab = i;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) G.pipSub = (G.pipSub + 1) % SUBCOUNT[G.pipTab];
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) G.pipSub = (G.pipSub + SUBCOUNT[G.pipTab] - 1) % SUBCOUNT[G.pipTab];
    // clic sulle schede
    PipRect r{(UIW() - std::min(UIW() - 80, 1160.0f)) / 2, 30, std::min(UIW() - 80, 1160.0f), 660};
    for (int i = 0; i < 5; i++) {
        float tx = r.x + 40 + i * (r.w - 80) / 5;
        if (UIClicked(tx, r.y + 12, (r.w - 80) / 5 - 10, 34)) G.pipTab = i;
    }
    for (int i = 0; i < SUBCOUNT[G.pipTab]; i++) {
        float tx = r.x + 40 + i * 190;
        if (SUBTABS[G.pipTab][i][0] && UIClicked(tx, r.y + 56, 180, 26)) G.pipSub = i;
    }
    if (G.pipSub >= SUBCOUNT[G.pipTab]) G.pipSub = 0;
    if (oldTab != G.pipTab || oldSub != G.pipSub) {
        G.pipSel = 0;
        if (oldTab != G.pipTab) G.pipSub = 0;
        PlaySfx(SFX_UI_MOVE);
    }
    int count = 0;
    if (G.pipTab == 0) count = G.pipSub == 1 ? ST_COUNT : (G.pipSub == 2 ? SK_COUNT : (G.pipSub == 3 ? PK_COUNT : 0));
    else if (G.pipTab == 1) count = (int)InvList(G.pipSub).size();
    else if (G.pipTab == 2) {
        if (G.pipSub == 0) {
            for (int q = 0; q < CAMPAIGN_QUEST_COUNT; q++)
                count += QUESTS[q].stage > 0 || QUESTS[q].done;
        } else count = G.pipSub == 1 ? (int)W.locations.size() : (int)PHOTO_SUBJECTS.size();
    }
    else if (G.pipTab == 4) count = 3;
    float wheel = GetMouseWheelMove();
    if ((IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN) || wheel < -0.5f) && count > 0) { G.pipSel = (G.pipSel + 1) % count; PlaySfx(SFX_UI_MOVE, 0.5f); }
    if ((IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || wheel > 0.5f) && count > 0) { G.pipSel = (G.pipSel + count - 1) % count; PlaySfx(SFX_UI_MOVE, 0.5f); }
    if (count > 0) G.pipSel = Clamp(G.pipSel, 0, count - 1);
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    if (G.pipTab == 0 && G.pipSub == 0 && P.pendingLevels > 0 && (enter || IsKeyPressed(KEY_L))) {
        LevelUpOpen();
        return;
    }
    if (G.pipTab == 1) {
        auto list = InvList(G.pipSub);
        // clic su un elemento
        for (int i = 0; i < (int)list.size(); i++)
            if (UIClicked(r.x + 30, r.y + 100 + i * 30 - G.pipScroll, 520, 28)) {
                if (G.pipSel == i) enter = true;
                G.pipSel = i;
            }
        if (!list.empty() && enter) {
            UseItem(list[G.pipSel]);
            PlaySfx(SFX_UI_OK);
        }
        if (!list.empty() && IsKeyPressed(KEY_X)) DropItem(list[G.pipSel]);
    }
    if (G.pipTab == 4 && enter) {
        int st[] = {1, 2, 0};
        SetRadioStation(st[G.pipSel]);
        PlaySfx(SFX_UI_OK);
    }
    if (G.pipTab == 3) {
        // viaggio rapido con clic sulla mappa
        float ms = std::min(r.h - 190, r.w * 0.6f);
        float mx = r.x + 40, my = r.y + 100;
        for (int i = 0; i < (int)W.locations.size(); i++) {
            Location& l = W.locations[i];
            if (!l.discovered) continue;
            float px = mx + (l.pos.x + WORLD_HALF) / (WORLD_HALF * 2) * ms, py = my + (l.pos.z + WORLD_HALF) / (WORLD_HALF * 2) * ms;
            if (UIClicked(px - 10, py - 10, 20, 20)) {
                if (G.chased || HostilesNear(50)) {
                    Notify("Non puoi viaggiare mentre qualcuno ti insegue.", Col(255, 160, 100));
                    PlaySfx(SFX_UI_BACK);
                } else {
                    float dist = DistXZ(P.pos, l.pos);
                    G.timeOfDay += 0.5f + dist / 400.0f;
                    G.pendingZone = ZONE_WASTE;
                    G.pendingPos = l.travelPos;
                    G.pendingYaw = AngleTo(l.travelPos, l.pos);
                    G.fade = 0.001f;
                    SetGameState(GS_PLAY);
                    PlaySfx(SFX_UI_OK);
                    return;
                }
            }
        }
        // multigiocatore: clic su un altro giocatore per raggiungerlo
        int who = NetMapClick([&](Vector3 p) { return V2(mx + (p.x + WORLD_HALF) / (WORLD_HALF * 2) * ms, my + (p.z + WORLD_HALF) / (WORLD_HALF * 2) * ms); });
        if (who >= 0) {
            const RemotePlayer& rp = NetPlayers()[who];
            if (G.chased || HostilesNear(50)) {
                Notify("Non puoi viaggiare mentre qualcuno ti insegue.", Col(255, 160, 100));
                PlaySfx(SFX_UI_BACK);
            } else {
                Vector3 dest = rp.pos - YawDir(rp.yaw) * 2.5f; // alle sue spalle
                G.timeOfDay += 0.5f + DistXZ(P.pos, rp.pos) / 400.0f;
                G.pendingZone = ZONE_WASTE;
                G.pendingPos = dest;
                G.pendingYaw = AngleTo(dest, rp.pos);
                G.fade = 0.001f;
                SetGameState(GS_PLAY);
                Notify("Raggiungi " + rp.name + ".");
                PlaySfx(SFX_UI_OK);
                return;
            }
        }
    }
}

void PipBoyDraw() {
    Player& P = G.player;
    PipRect r = PipFrame();
    // schede
    for (int i = 0; i < 5; i++) {
        float tx = r.x + 40 + i * (r.w - 80) / 5;
        float tw = (r.w - 80) / 5 - 10;
        bool sel = i == G.pipTab;
        if (sel) UIRectLines(tx, r.y + 12, tw, 34, 2, PIP);
        UIText(TABS[i], tx + tw / 2, r.y + 17, 24, sel ? PIP : PIP_DIM, 1);
    }
    UIRect(r.x + 20, r.y + 50, r.w - 40, 2, PIP_DIM);
    for (int i = 0; i < SUBCOUNT[G.pipTab]; i++) {
        if (!SUBTABS[G.pipTab][i][0]) continue;
        float tx = r.x + 40 + i * 190;
        UIText(SUBTABS[G.pipTab][i], tx, r.y + 58, 18, i == G.pipSub ? PIP : PIP_DARK);
    }
    // barra inferiore
    float by = r.y + r.h - 62;
    UIRect(r.x + 20, by - 6, r.w - 40, 2, PIP_DIM);
    int hours = (int)G.timeOfDay, mins = (int)((G.timeOfDay - hours) * 60);
    std::string info = Fmt("PF %d/%d    PA %d/%d    LV %d    PE %d/%d    Peso %d/%d    Tappi %d    Giorno %d  %02d:%02d",
                           (int)P.hp, (int)MaxHP(), (int)P.ap, (int)MaxAP(), P.level, P.xp, XPForLevel(P.level + 1), (int)P.inv.Weight(), (int)CarryMax(), P.inv.Count(I_CAPS), G.day, hours, mins);
    UIText(info, r.x + r.w / 2, by + 4, 17, PIP, 1);

    float lx = r.x + 30, ly = r.y + 100;
    float rx = r.x + r.w * 0.52f, rw = r.w * 0.45f;
    auto ListRow = [&](int i, const std::string& left, const std::string& right, bool mark = false) {
        float y = ly + i * 30;
        bool sel = i == G.pipSel;
        if (sel) UIRect(lx - 6, y - 3, rx - lx - 30, 28, PIP_DARK);
        if (mark) UIRect(lx, y + 8, 8, 8, PIP);
        UIText(left, lx + (mark ? 16 : 0), y, 20, sel ? Color{200, 255, 210, 255} : PIP);
        if (!right.empty()) UIText(right, rx - 44, y, 20, sel ? Color{200, 255, 210, 255} : PIP, 2);
    };
    auto Desc = [&](const std::string& title, const std::string& body, float y0 = 0) {
        float y = ly + y0;
        UIText(title, rx, y, 24, PIP);
        auto lines = UIWrap(body, 18, rw);
        for (size_t i = 0; i < lines.size(); i++) UIText(lines[i], rx, y + 36 + i * 22, 18, PIP_DIM);
    };

    if (G.pipTab == 0) {
        switch (G.pipSub) {
            case 0: {
                float hpF = P.hp / MaxHP();
                DrawVaultFigure(r.x + r.w * 0.25f, r.y + 300, 1.6f, hpF);
                float y = ly;
                UIText(P.name, rx, y, 28, PIP);
                y += 40;
                UIText(Fmt("Livello %d    PE %d / %d", P.level, P.xp, XPForLevel(P.level + 1)), rx, y, 20, PIP);
                y += 30;
                UIText(Fmt("PF %d / %d", (int)P.hp, (int)HPCap()), rx, y, 20, PIP); y += 26;
                UIText(Fmt("PA %d / %d", (int)P.ap, (int)MaxAP()), rx, y, 20, PIP); y += 26;
                UIText(Fmt("RAD %d   (resistenza %d%%)", (int)P.rads, (int)(PlayerRadRes() * 100)), rx, y, 20, P.rads > 200 ? Col(255, 230, 80) : PIP); y += 26;
                UIText(Fmt("Foto nel catalogo %d / %d", (int)P.photos.size(), (int)PHOTO_SUBJECTS.size()), rx, y, 20, PIP); y += 26;
                UIText(Fmt("Karma: %s (%d)", KarmaTitle(P.karma), P.karma), rx, y, 20, PIP); y += 40;
                UIText("EFFETTI", rx, y, 20, PIP_DIM); y += 28;
                bool any = false;
                auto Eff = [&](const std::string& s) { UIText(s, rx + 10, y, 18, PIP); y += 24; any = true; };
                if (P.radXTimer > 0) Eff(Fmt("Anti-Rad (%ds)", (int)P.radXTimer));
                if (P.teaTimer > 0) Eff(Fmt("Tisana del deserto (%ds)", (int)P.teaTimer));
                if (P.coffeeTimer > 0) Eff(Fmt("Caffè d'anteguerra (%ds)", (int)P.coffeeTimer));
                if (P.stealthT > 0) Eff(Fmt("Mantello ottico (%ds)", (int)P.stealthT));
                if (P.rads >= 200) Eff(Fmt("Avvelenamento da radiazioni: -%d PF max", (int)(MaxHP() - HPCap())));
                if (P.inv.Weight() > CarryMax()) Eff("Sovraccarico: movimento rallentato");
                if (!any) UIText("Nessuno", rx + 10, y, 18, PIP_DARK);
                if (P.pendingLevels > 0) {
                    float a = 0.6f + 0.4f * sinf(G.time * 5);
                    UIRect(rx - 10, r.y + r.h - 130, rw, 44, ColAlpha(PIP_DARK, a));
                    UIText("AVANZAMENTO DI LIVELLO! Premi INVIO", rx + rw / 2 - 10, r.y + r.h - 120, 22, PIP, 1);
                }
                break;
            }
            case 1:
                for (int i = 0; i < ST_COUNT; i++) {
                    int base = P.special[i], eff = Spec(i);
                    ListRow(i, SPECIAL_NAMES[i], eff != base ? Fmt("%d (+%d)", eff, eff - base) : Fmt("%d", base));
                }
                Desc(SPECIAL_NAMES[G.pipSel], SPECIAL_DESC[G.pipSel]);
                break;
            case 2:
                for (int i = 0; i < SK_COUNT; i++) ListRow(i, SKILL_NAMES[i], Fmt("%d", SkillValue((Skill)i)), P.tagged[i]);
                Desc(SKILL_NAMES[G.pipSel], std::string(SKILL_DESC[G.pipSel]) + "\n\nStatistica: " + SPECIAL_NAMES[SKILL_GOV[G.pipSel]] + (P.tagged[G.pipSel] ? "\nAbilità contrassegnata (+15)." : ""));
                break;
            case 3: {
                int k = 0;
                for (int i = 0; i < PK_COUNT; i++)
                    if (P.perks[i]) {
                        ListRow(k, PERKS[i].name, "", true);
                        if (k == G.pipSel) Desc(PERKS[i].name, PERKS[i].desc);
                        k++;
                    }
                if (k == 0) UIText("Nessun talento. Ne ottieni uno a ogni livello.", lx, ly, 20, PIP_DIM);
                break;
            }
            case 4: {
                float y = ly;
                auto Row = [&](const std::string& a, const std::string& b) {
                    UIText(a, lx, y, 20, PIP);
                    UIText(b, lx + 460, y, 20, PIP, 2);
                    y += 30;
                };
                Row("Karma", Fmt("%s (%d)", KarmaTitle(P.karma), P.karma));
                Row("Missioni completate", Fmt("%d", P.questsDone));
                Row("Luoghi scoperti", Fmt("%d / %d", (int)std::count_if(W.locations.begin(), W.locations.end(), [](const Location& l) { return l.discovered; }), (int)W.locations.size()));
                Row("Foto scattate", Fmt("%d", (int)P.photos.size()));
                Row("Riparazioni", Fmt("%d", P.repairs));
                Row("Distrazioni riuscite", Fmt("%d", P.distractions));
                Row("Volte scoperto", Fmt("%d", P.timesCaught));
                Row("Serrature scassinate", Fmt("%d", P.locksPicked));
                Row("Terminali violati", Fmt("%d", P.termsHacked));
                Row("Kit medici usati", Fmt("%d", P.medkitsUsed));
                Row("Strada percorsa", Fmt("%.1f km", P.walked / 1000.0f));
                Row("Tappi guadagnati", Fmt("%d", P.capsEarned));
                Row("Tempo di gioco", Fmt("%dh %02dm", (int)(P.playTime / 3600), ((int)P.playTime / 60) % 60));
                break;
            }
        }
    } else if (G.pipTab == 1) {
        auto list = InvList(G.pipSub);
        float maxRows = (r.h - 200) / 30;
        G.pipScroll = std::max(0.0f, (G.pipSel - maxRows + 2) * 30);
        BeginScissorMode((int)(r.x * UIS()), (int)((ly - 4) * UIS()), (int)(r.w * UIS()), (int)((r.h - 180) * UIS()));
        for (int i = 0; i < (int)list.size(); i++) {
            int id = list[i];
            int n = P.inv.Count(id);
            bool eq = (P.tool != TOOL_HANDS && id == ToolItem(P.tool)) || id == P.armor;
            std::string name = ITEMS[id].name;
            if (n > 1) name += Fmt(" (%d)", n);
            float y = ly + i * 30 - G.pipScroll;
            bool sel = i == G.pipSel;
            if (sel) UIRect(lx - 6, y - 3, rx - lx - 30, 28, PIP_DARK);
            if (eq) UIRect(lx, y + 8, 8, 8, PIP);
            UIText(name, lx + 16, y, 20, sel ? Color{200, 255, 210, 255} : PIP);
        }
        EndScissorMode();
        if (list.empty()) UIText("Niente.", lx, ly, 20, PIP_DIM);
        else {
            int id = list[G.pipSel];
            UIText(ITEMS[id].name, rx, ly, 24, PIP);
            auto st = ItemStats(id);
            float y = ly + 40;
            for (auto& s : st) { UIText(s, rx, y, 19, PIP); y += 26; }
            y += 10;
            for (auto& l : UIWrap(ITEMS[id].desc, 18, rw)) { UIText(l, rx, y, 18, PIP_DIM); y += 22; }
            bool eqd = (P.tool != TOOL_HANDS && id == ToolItem(P.tool)) || id == P.armor;
            const char* act = ITEMS[id].type == IT_TOOL ? (eqd ? "INVIO) In mano" : "INVIO) Prendi in mano")
                              : ITEMS[id].type == IT_ARMOR ? (eqd ? "INVIO) Togli" : "INVIO) Indossa")
                              : (ITEMS[id].type == IT_AID ? "INVIO) Usa" : ((id == I_SCOUTNOTE || id == I_MAINTCODE) ? "INVIO) Leggi" : ""));
            UIText(std::string(act) + (ITEMS[id].type != IT_KEY ? "    X) Lascia" : ""), rx, r.y + r.h - 100, 18, PIP_DIM);
        }
    } else if (G.pipTab == 2) {
        if (G.pipSub == 0) {
            std::vector<int> order;
            for (int q = 0; q < CAMPAIGN_QUEST_COUNT; q++)
                if (QUESTS[q].stage > 0 && !QUESTS[q].done) order.push_back(q);
            for (int q = 0; q < CAMPAIGN_QUEST_COUNT; q++)
                if (QUESTS[q].done) order.push_back(q);
            if (order.empty()) UIText("Nessuna missione.", lx, ly, 20, PIP_DIM);
            if (G.pipSel >= (int)order.size()) G.pipSel = std::max(0, (int)order.size() - 1);
            for (int i = 0; i < (int)order.size(); i++) {
                int q = order[i];
                bool completed = QUESTS[q].done;
                ListRow(i, std::string(QUESTS[q].title) + (completed ? "  [COMPLETATA]" : ""), "", !completed);
            }
            if (!order.empty()) {
                int q = order[G.pipSel];
                Desc(QUESTS[q].title, QuestDescription(q));
                float y = ly + 200;
                if (!QUESTS[q].done) {
                    UIText("OBIETTIVI", rx, y, 20, PIP);
                    y += 30;
                    for (auto& o : QuestObjectives(q)) {
                        for (auto& l : UIWrap("• " + o.text, 18, rw)) { UIText(l, rx, y, 18, PIP); y += 22; }
                        y += 6;
                    }
                } else UIText("Completata.", rx, y, 20, PIP_DIM);
            }
        } else if (G.pipSub == 1) {
            int k = 0;
            for (auto& l : W.locations) ListRow(k++, l.discovered ? l.name : "???", l.discovered ? Fmt("%d m", (int)DistXZ(P.pos, l.pos)) : "");
        } else {
            // catalogo fotografico delle Terre Bruciate
            int n = (int)PHOTO_SUBJECTS.size();
            float maxRows = (r.h - 200) / 30;
            G.pipScroll = std::max(0.0f, (G.pipSel - maxRows + 2) * 30);
            BeginScissorMode((int)(r.x * UIS()), (int)((ly - 4) * UIS()), (int)(r.w * UIS()), (int)((r.h - 180) * UIS()));
            for (int i = 0; i < n; i++) {
                const PhotoSubject& ps = PHOTO_SUBJECTS[i];
                bool have = HasPhoto(ps.key);
                float y = ly + i * 30 - G.pipScroll;
                bool sel = i == G.pipSel;
                if (sel) UIRect(lx - 6, y - 3, rx - lx - 30, 28, PIP_DARK);
                if (have) UIRect(lx, y + 8, 8, 8, PIP);
                UIText(have ? ps.name : (ps.landmark ? "??? (luogo)" : "??? (creatura)"), lx + 16, y, 20, sel ? Color{200, 255, 210, 255} : (have ? PIP : PIP_DIM));
                if (have) UIText(Fmt("%d PE", ps.xp), rx - 44, y, 18, PIP_DIM, 2);
            }
            EndScissorMode();
            if (n > 0) {
                const PhotoSubject& ps = PHOTO_SUBJECTS[Clamp(G.pipSel, 0, n - 1)];
                bool have = HasPhoto(ps.key);
                UIText(Fmt("CATALOGO  %d / %d", (int)P.photos.size(), n), rx, ly, 24, PIP);
                float fx = rx, fy = ly + 44, fw = rw - 20, fh = 150;
                UIRectLines(fx, fy, fw, fh, 2, PIP_DIM);
                if (have) {
                    // "stampa" stilizzata: orizzonte e sagoma
                    UIRect(fx + 4, fy + fh * 0.62f, fw - 8, fh * 0.38f - 4, PIP_DARK);
                    UIText(ps.landmark ? "[ PAESAGGIO ]" : "[ CREATURA ]", fx + fw / 2, fy + fh / 2 - 12, 22, PIP, 1);
                    UIText(ps.name, rx, fy + fh + 14, 22, PIP);
                    float y = fy + fh + 48;
                    for (auto& l : UIWrap(ps.desc, 18, rw)) { UIText(l, rx, y, 18, PIP_DIM); y += 22; }
                } else {
                    UIText("?", fx + fw / 2, fy + fh / 2 - 22, 44, PIP_DARK, 1);
                    UIText("Non ancora fotografato.", rx, fy + fh + 14, 20, PIP_DIM);
                    UIText(ps.landmark ? "Un luogo notevole delle Terre Bruciate." : "Una creatura delle Terre Bruciate.", rx, fy + fh + 44, 18, PIP_DIM);
                }
            }
        }
    } else if (G.pipTab == 3) {
        float ms = std::min(r.h - 190, r.w * 0.6f);
        float mx = r.x + 40, my = r.y + 100;
        DrawTexturePro(W.mapTex, Rectangle{0, 0, (float)W.mapTex.width, (float)W.mapTex.height}, Rectangle{mx * UIS(), my * UIS(), ms * UIS(), ms * UIS()}, V2(0, 0), 0, WHITE);
        UIRectLines(mx, my, ms, ms, 2, PIP_DIM);
        auto M = [&](Vector3 p) { return V2(mx + (p.x + WORLD_HALF) / (WORLD_HALF * 2) * ms, my + (p.z + WORLD_HALF) / (WORLD_HALF * 2) * ms); };
        std::string hover;
        for (auto& l : W.locations) {
            if (!l.discovered) continue;
            Vector2 p = M(l.pos);
            bool hv = UIHover(p.x - 10, p.y - 10, 20, 20);
            UIRect(p.x - 6, p.y - 6, 12, 12, hv ? Color{220, 255, 220, 255} : PIP);
            UIRect(p.x - 3, p.y - 3, 6, 6, PIP_BG);
            UIText(l.name, p.x + 10, p.y - 9, 15, PIP);
            if (hv) hover = l.name;
        }
        for (int q = 0; q < CAMPAIGN_QUEST_COUNT; q++) {
            if (QUESTS[q].stage == 0 || QUESTS[q].done) continue;
            auto objs = QuestObjectives(q);
            for (auto& o : objs) {
                if (!o.hasPos || o.zone != ZONE_WASTE) continue;
                Vector2 p = M(o.pos);
                float s = UIS();
                Color qc = q == Q_MAIN ? Col(255, 230, 120) : Col(200, 255, 200);
                DrawTriangle(V2(p.x * s, (p.y + 4) * s), V2((p.x + 7) * s, (p.y - 8) * s), V2((p.x - 7) * s, (p.y - 8) * s), qc);
                break;
            }
        }
        if (P.zone == ZONE_WASTE) {
            Vector2 pp = M(P.pos);
            float s = UIS();
            Vector2 f = V2(sinf(P.yaw), cosf(P.yaw)), rt = V2(cosf(P.yaw), -sinf(P.yaw));
            Vector2 a = V2((pp.x + f.x * 10) * s, (pp.y + f.y * 10) * s), b = V2((pp.x - f.x * 6 + rt.x * 6) * s, (pp.y - f.y * 6 + rt.y * 6) * s), c = V2((pp.x - f.x * 6 - rt.x * 6) * s, (pp.y - f.y * 6 - rt.y * 6) * s);
            DrawTriangle(a, c, b, Color{230, 255, 230, 255});
            DrawTriangle(a, b, c, Color{230, 255, 230, 255});
        }
        NetMapDraw(M, hover);
        float ix = mx + ms + 30;
        UIText("TERRE BRUCIATE", ix, my, 24, PIP);
        UIText(P.zone == ZONE_WASTE ? "Posizione attuale" : std::string("Ti trovi in: ") + W.zones[P.zone].name, ix, my + 36, 18, PIP_DIM);
        UIText("Clic su un luogo scoperto", ix, my + 80, 18, PIP);
        UIText("per il viaggio rapido.", ix, my + 102, 18, PIP);
        if (NetAvailable()) UIText("Rombi: altri giocatori.", ix, my + 124, 18, Col(255, 225, 150));
        if (!hover.empty()) UIText("> " + hover, ix, my + 150, 22, Color{220, 255, 220, 255});
        
    } else if (G.pipTab == 4) {
        const char* names[] = {"Radio Ferrocittà", "Segnale di soccorso", "Spegni la radio"};
        int st[] = {1, 2, 0};
        for (int i = 0; i < 3; i++) ListRow(i, names[i], gRadioStation == st[i] && i < 2 ? "IN ONDA" : "", gRadioStation == st[i]);
        float cx = rx, cy = ly + 150, w = rw - 20;
        UIRectLines(cx, cy - 80, w, 160, 1, PIP_DIM);
        UIRect(cx, cy, w, 1, PIP_DARK);
        float S = UIS();
        Vector2 prev{};
        for (int i = 0; i <= 120; i++) {
            float x = cx + w * i / 120.0f;
            float amp = gRadioStation == 0 ? 0.02f : (gRadioStation == 2 ? (fmodf(G.time * 2, 1.0f) < 0.3f ? 0.8f : 0.1f) : 0.6f);
            float y = cy + sinf(i * 0.35f + G.time * 9) * 50 * amp * sinf(i * 0.05f + G.time) + (gRadioStation ? gRng.range(-4, 4) * amp : 0);
            Vector2 p = V2(x * S, y * S);
            if (i > 0) DrawLineEx(prev, p, 2 * S, PIP);
            prev = p;
        }
        UIText("INVIO) Sintonizza", rx, ly + 260, 18, PIP_DIM);
    }
    UIText("Q/E scheda   A/D sezione   W/S scorri   INVIO seleziona   TAB chiudi", r.x + r.w / 2, r.y + r.h - 26, 15, PIP_DIM, 1);
}

// ---------------------------------------------------------------------------
//  Saccheggio
// ---------------------------------------------------------------------------
static Inventory* LootInv() {
    if (G.lootInter >= 0) return &G.inters[G.lootInter].inv;
    return nullptr;
}

void LootOpen(int inter) {
    G.lootInter = inter;
    G.lootEnt = -1;
    G.pipSel = 0;
    PlaySfx(SFX_DOOR, 0.5f, 1.4f);
    Interactable& it = G.inters[inter];
    if (it.tag == "cache" && !it.locked && QUESTS[Q_SIGNAL].stage >= 20) CompleteQuest(Q_SIGNAL);
    if (it.tag == "exosuit" && !G.flags["found_exo"]) {
        G.flags["found_exo"] = 1;
        BigNotify("ESOSCHELETRO DA CARICO", "Porta qualsiasi cosa. Peccato per il fracasso.");
    }
    SetGameState(GS_LOOT);
}

void LootUpdate() {
    Inventory* inv = LootInv();
    if (!inv || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || (IsKeyPressed(KEY_E) && inv->Empty())) {
        SetGameState(GS_PLAY);
        return;
    }
    int n = (int)inv->items.size();
    if (n > 0) {
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN) || GetMouseWheelMove() < -0.5f) G.pipSel = (G.pipSel + 1) % n;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || GetMouseWheelMove() > 0.5f) G.pipSel = (G.pipSel + n - 1) % n;
        G.pipSel = Clamp(G.pipSel, 0, n - 1);
    }
    float Wd = UIW();
    float x = Wd / 2 - 300, y = 170;
    bool take = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || (IsKeyPressed(KEY_E) && n > 0);
    for (int i = 0; i < n; i++)
        if (UIClicked(x + 10, y + 60 + i * 30, 580, 28)) {
            G.pipSel = i;
            take = true;
        }
    if (take && n > 0) {
        ItemStack s = inv->items[G.pipSel];
        int cnt = s.count;
        if (s.id == I_CAPS && G.player.perks[PK_FORTUNE]) cnt *= 2;
        inv->Remove(s.id, s.count);
        GiveItem(s.id, cnt, false);
        PlaySfx(SFX_PICKUP);
    }
    if (IsKeyPressed(KEY_R) || UIClicked(x + 10, y + 420, 200, 30)) {
        for (auto& s : inv->items) {
            int cnt = s.count;
            if (s.id == I_CAPS && G.player.perks[PK_FORTUNE]) cnt *= 2;
            GiveItem(s.id, cnt, false);
        }
        if (!inv->Empty()) PlaySfx(SFX_PICKUP);
        inv->Clear();
        SetGameState(GS_PLAY);
    }
}

void LootDraw() {
    Inventory* inv = LootInv();
    if (!inv) return;
    float Wd = UIW();
    float x = Wd / 2 - 300, y = 170, w = 600, h = 470;
    UIRect(x, y, w, h, Color{4, 18, 9, 225});
    UIRectLines(x, y, w, h, 2, PIP);
    std::string title = G.inters[G.lootInter].name;
    UIText(title, x + 20, y + 14, 26, PIP);
    UIRect(x + 10, y + 50, w - 20, 2, PIP_DIM);
    if (inv->Empty()) UIText("Vuoto.", x + 20, y + 64, 20, PIP_DIM);
    for (int i = 0; i < (int)inv->items.size() && i < 11; i++) {
        auto& s = inv->items[i];
        bool sel = i == G.pipSel;
        float ry = y + 60 + i * 30;
        if (sel) UIRect(x + 10, ry - 2, w - 20, 28, PIP_DARK);
        UIText(s.count > 1 ? Fmt("%s (%d)", ITEMS[s.id].name, s.count) : std::string(ITEMS[s.id].name), x + 20, ry, 20, sel ? Color{200, 255, 210, 255} : PIP);
        UIText(Fmt("P %.1f  V %d", ITEMS[s.id].weight, ITEMS[s.id].value), x + w - 20, ry, 16, PIP_DIM, 2);
    }
    UIText("INVIO/Clic) Prendi    R) Prendi tutto    ESC) Chiudi", x + 20, y + h - 40, 17, PIP_DIM);
    UIText(Fmt("Peso: %d / %d", (int)G.player.inv.Weight(), (int)CarryMax()), x + w - 20, y + h - 40, 17, G.player.inv.Weight() > CarryMax() ? Col(255, 140, 90) : PIP_DIM, 2);
}

// ---------------------------------------------------------------------------
//  Baratto
// ---------------------------------------------------------------------------
static int sBarterCol = 0, sBarterSel[2] = {0, 0};

static float BarterFactor() { return Clamp(SkillValue(SK_BARTER) / 100.0f * 0.6f + Spec(ST_CHA) * 0.03f, 0.0f, 0.9f); }
static int BuyPrice(int id) { return std::max(1, (int)roundf(ITEMS[id].value * (1.6f - BarterFactor() * 0.6f))); }
static int SellPrice(int id) { return ITEMS[id].value <= 0 ? 0 : std::max(1, (int)floorf(ITEMS[id].value * (0.3f + BarterFactor() * 0.4f))); }

static std::vector<int> PlayerSellable() {
    std::vector<int> out;
    for (auto& s : G.player.inv.items) {
        if (s.id == I_CAPS || ITEMS[s.id].type == IT_KEY || ITEMS[s.id].value <= 0) continue;
        int n = s.count - (s.id == G.player.armor ? 1 : 0);
        if (n > 0) out.push_back(s.id);
    }
    return out;
}

void BarterOpen(int vendor, int npc) {
    G.barterVendor = vendor;
    G.barterNpc = npc;
    sBarterCol = 1;
    sBarterSel[0] = sBarterSel[1] = 0;
    SetGameState(GS_BARTER);
}

void BarterUpdate() {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        SetGameState(GS_PLAY);
        return;
    }
    Vendor& v = G.vendors[G.barterVendor];
    auto mine = PlayerSellable();
    std::vector<int> theirs;
    for (auto& s : v.inv.items) theirs.push_back(s.id);
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) sBarterCol = 0;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) sBarterCol = 1;
    int n = sBarterCol == 0 ? (int)mine.size() : (int)theirs.size();
    int& sel = sBarterSel[sBarterCol];
    if (n > 0) {
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN) || GetMouseWheelMove() < -0.5f) sel = (sel + 1) % n;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || GetMouseWheelMove() > 0.5f) sel = (sel + n - 1) % n;
    }
    sBarterSel[0] = Clamp(sBarterSel[0], 0, std::max(0, (int)mine.size() - 1));
    sBarterSel[1] = Clamp(sBarterSel[1], 0, std::max(0, (int)theirs.size() - 1));
    float Wd = UIW();
    float cw = 520, x0 = Wd / 2 - cw - 20, x1 = Wd / 2 + 20, y = 140;
    bool act = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E);
    for (int c = 0; c < 2; c++) {
        int cnt = c == 0 ? (int)mine.size() : (int)theirs.size();
        int off = std::max(0, sBarterSel[c] - 12);
        for (int i = off; i < cnt && i < off + 14; i++)
            if (UIClicked(c == 0 ? x0 : x1, y + 60 + (i - off) * 28, cw, 26)) {
                sBarterCol = c;
                sBarterSel[c] = i;
                act = true;
            }
    }
    if (!act) return;
    if (sBarterCol == 0 && !mine.empty()) {
        int id = mine[sBarterSel[0]];
        int price = SellPrice(id);
        if (v.caps < price) { Notify("Il mercante non ha abbastanza tappi."); PlaySfx(SFX_UI_BACK); return; }
        TakeItem(id, 1);
        v.inv.Add(id, 1);
        v.caps -= price;
        G.player.inv.Add(I_CAPS, price);
        PlaySfx(SFX_PICKUP, 0.7f, 1.2f);
    } else if (sBarterCol == 1 && !theirs.empty()) {
        int id = theirs[sBarterSel[1]];
        int price = BuyPrice(id);
        if (G.player.inv.Count(I_CAPS) < price) { Notify("Non hai abbastanza tappi."); PlaySfx(SFX_UI_BACK); return; }
        int qty = 1;
        int total = 0;
        for (int k = 0; k < qty && G.player.inv.Count(I_CAPS) >= price; k++) {
            v.inv.Remove(id, 1);
            G.player.inv.Remove(I_CAPS, price);
            G.player.inv.Add(id, 1);
            v.caps += price;
            total++;
        }
        if (total > 0) PlaySfx(SFX_PICKUP, 0.7f, 1.0f);
    }
}

void BarterDraw() {
    Vendor& v = G.vendors[G.barterVendor];
    float Wd = UIW();
    float cw = 520, x0 = Wd / 2 - cw - 20, x1 = Wd / 2 + 20, y = 140, h = 480;
    UIRect(x0 - 20, y - 70, cw * 2 + 80, h + 120, Color{4, 18, 9, 230});
    UIRectLines(x0 - 20, y - 70, cw * 2 + 80, h + 120, 2, PIP);
    UIText("BARATTO - " + v.name, Wd / 2, y - 56, 26, PIP, 1);
    auto mine = PlayerSellable();
    for (int c = 0; c < 2; c++) {
        float x = c == 0 ? x0 : x1;
        UIRect(x, y, cw, h, Color{0, 0, 0, 80});
        UIRectLines(x, y, cw, h, sBarterCol == c ? 2 : 1, sBarterCol == c ? PIP : PIP_DARK);
        UIText(c == 0 ? Fmt("I TUOI OGGETTI   (Tappi: %d)", G.player.inv.Count(I_CAPS)) : Fmt("%s   (Tappi: %d)", G.ents[G.barterNpc].name.c_str(), v.caps), x + 14, y + 14, 20, PIP);
        std::vector<int> ids;
        if (c == 0) ids = mine;
        else for (auto& s : v.inv.items) ids.push_back(s.id);
        int off = std::max(0, sBarterSel[c] - 12);
        for (int i = off; i < (int)ids.size() && i < off + 14; i++) {
            int id = ids[i];
            int cnt = c == 0 ? G.player.inv.Count(id) : v.inv.Count(id);
            bool sel = i == sBarterSel[c] && sBarterCol == c;
            float ry = y + 60 + (i - off) * 28;
            if (sel) UIRect(x + 6, ry - 2, cw - 12, 26, PIP_DARK);
            UIText(cnt > 1 ? Fmt("%s (%d)", ITEMS[id].name, cnt) : std::string(ITEMS[id].name), x + 14, ry, 18, sel ? Color{200, 255, 210, 255} : PIP);
            UIText(Fmt("%d", c == 0 ? SellPrice(id) : BuyPrice(id)), x + cw - 14, ry, 18, PIP, 2);
        }
        if (ids.empty()) UIText("Niente da scambiare.", x + 14, y + 60, 18, PIP_DIM);
    }
    UIText("A/D colonna   W/S scorri   INVIO/Clic compra o vendi   ESC chiudi", Wd / 2, y + h + 14, 17, PIP_DIM, 1);
}

// ---------------------------------------------------------------------------
//  Avanzamento di livello
// ---------------------------------------------------------------------------
static int sLvStep = 0, sLvPoints = 0, sLvAlloc[SK_COUNT], sLvSel = 0;

static std::vector<int> AvailablePerks() {
    std::vector<int> out;
    for (int i = 0; i < PK_COUNT; i++)
        if (!G.player.perks[i] && G.player.level >= PERKS[i].level && G.player.special[PERKS[i].req] >= PERKS[i].reqVal) out.push_back(i);
    return out;
}

void LevelUpOpen() {
    sLvStep = 0;
    sLvSel = 0;
    sLvPoints = 10 + G.player.special[ST_INT] + (G.player.perks[PK_EDUCATED] ? 3 : 0);
    for (int i = 0; i < SK_COUNT; i++) sLvAlloc[i] = 0;
    SetGameState(GS_LEVELUP);
    PlaySfx(SFX_LEVELUP, 0.6f);
}

void LevelUpUpdate() {
    Player& P = G.player;
    if (sLvStep == 0) {
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) sLvSel = (sLvSel + 1) % SK_COUNT;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) sLvSel = (sLvSel + SK_COUNT - 1) % SK_COUNT;
        float Wd = UIW(), x = Wd / 2 - 360;
        bool inc = IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL);
        bool dec = IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS);
        for (int i = 0; i < SK_COUNT; i++) {
            float ry = 170 + i * 34;
            if (UIClicked(x + 470, ry, 30, 28)) { sLvSel = i; dec = true; }
            if (UIClicked(x + 560, ry, 30, 28)) { sLvSel = i; inc = true; }
        }
        int cur = SkillValue((Skill)sLvSel) + sLvAlloc[sLvSel];
        if (inc && sLvPoints > 0 && cur < 100) { sLvAlloc[sLvSel]++; sLvPoints--; PlaySfx(SFX_UI_MOVE, 0.5f); }
        if (dec && sLvAlloc[sLvSel] > 0) { sLvAlloc[sLvSel]--; sLvPoints++; PlaySfx(SFX_UI_MOVE, 0.5f); }
        bool allMax = true;
        for (int i = 0; i < SK_COUNT; i++)
            if (SkillValue((Skill)i) + sLvAlloc[i] < 100) allMax = false;
        if ((IsKeyPressed(KEY_ENTER) || UIClicked(Wd / 2 - 100, 560, 200, 40)) && (sLvPoints == 0 || allMax)) {
            for (int i = 0; i < SK_COUNT; i++) P.skillPts[i] += sLvAlloc[i];
            sLvStep = 1;
            sLvSel = 0;
            PlaySfx(SFX_UI_OK);
            if (AvailablePerks().empty()) sLvStep = 2;
        }
    }
    if (sLvStep == 1) {
        auto perks = AvailablePerks();
        int n = (int)perks.size();
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN) || GetMouseWheelMove() < -0.5f) sLvSel = (sLvSel + 1) % n;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP) || GetMouseWheelMove() > 0.5f) sLvSel = (sLvSel + n - 1) % n;
        float Wd = UIW(), x = Wd / 2 - 480;
        bool ok = IsKeyPressed(KEY_ENTER);
        int off = std::max(0, sLvSel - 10);
        for (int i = off; i < n && i < off + 12; i++)
            if (UIClicked(x, 160 + (i - off) * 32, 380, 30)) {
                if (sLvSel == i) ok = true;
                sLvSel = i;
            }
        if (ok && n > 0) {
            P.perks[perks[sLvSel]] = true;
            Notify(std::string("Nuovo talento: ") + PERKS[perks[sLvSel]].name);
            sLvStep = 2;
            PlaySfx(SFX_UI_OK);
        }
    }
    if (sLvStep == 2) {
        P.pendingLevels--;
        P.hp = HPCap();
        if (P.pendingLevels > 0) LevelUpOpen();
        else SetGameState(GS_PIPBOY);
    }
}

void LevelUpDraw() {
    PipRect r = PipFrame();
    float Wd = UIW();
    UIText(Fmt("LIVELLO %d", G.player.level - G.player.pendingLevels + 1), Wd / 2, r.y + 20, 36, PIP, 1);
    if (sLvStep == 0) {
        float x = Wd / 2 - 360;
        UIText(Fmt("Distribuisci i punti abilità: %d rimasti", sLvPoints), Wd / 2, r.y + 72, 22, PIP, 1);
        for (int i = 0; i < SK_COUNT; i++) {
            float ry = 170 + i * 34;
            bool sel = i == sLvSel;
            if (sel) UIRect(x - 10, ry - 3, 740, 32, PIP_DARK);
            UIText(SKILL_NAMES[i], x, ry, 22, PIP);
            UIText("<", x + 485, ry, 22, sLvAlloc[i] > 0 ? PIP : PIP_DARK, 1);
            UIText(Fmt("%d", SkillValue((Skill)i) + sLvAlloc[i]), x + 530, ry, 22, sLvAlloc[i] > 0 ? Color{220, 255, 220, 255} : PIP, 1);
            UIText(">", x + 575, ry, 22, sLvPoints > 0 ? PIP : PIP_DARK, 1);
            if (sLvAlloc[i] > 0) UIText(Fmt("+%d", sLvAlloc[i]), x + 640, ry, 20, PIP_DIM);
        }
        UIText(SKILL_DESC[sLvSel], Wd / 2, 520, 18, PIP_DIM, 1);
        UIRectLines(Wd / 2 - 100, 560, 200, 40, 2, sLvPoints == 0 ? PIP : PIP_DARK);
        UIText("CONFERMA", Wd / 2, 568, 22, sLvPoints == 0 ? PIP : PIP_DARK, 1);
        UIText("W/S scegli   A/D togli/aggiungi   INVIO conferma", Wd / 2, 620, 17, PIP_DIM, 1);
    } else if (sLvStep == 1) {
        auto perks = AvailablePerks();
        float x = Wd / 2 - 480;
        UIText("Scegli un talento", Wd / 2, r.y + 72, 22, PIP, 1);
        int off = std::max(0, sLvSel - 10);
        for (int i = off; i < (int)perks.size() && i < off + 12; i++) {
            bool sel = i == sLvSel;
            float ry = 160 + (i - off) * 32;
            if (sel) UIRect(x - 6, ry - 3, 390, 30, PIP_DARK);
            UIText(PERKS[perks[i]].name, x, ry, 20, sel ? Color{220, 255, 220, 255} : PIP);
        }
        if (!perks.empty()) {
            const PerkDef& p = PERKS[perks[sLvSel]];
            float dx = x + 450;
            UIText(p.name, dx, 160, 28, PIP);
            float y = 210;
            for (auto& l : UIWrap(p.desc, 20, 480)) { UIText(l, dx, y, 20, PIP_DIM); y += 26; }
            UIText(Fmt("Requisiti: livello %d, %s %d", p.level, SPECIAL_NAMES[p.req], p.reqVal), dx, y + 20, 18, PIP_DIM);
        }
        UIText("W/S scegli   INVIO conferma", Wd / 2, 620, 17, PIP_DIM, 1);
    }
}
