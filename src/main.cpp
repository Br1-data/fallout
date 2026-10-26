// DUSTFALL - Cronache delle Terre Bruciate
// Punto d'ingresso, ciclo principale, ciclo giorno/notte e transizioni tra zone.
#include "game.h"
#include "platform.h"
#include "net.h"

#include <random>

#if DUSTFALL_WEB
#include <emscripten/emscripten.h>
#endif

Game G;

void SetGameState(GameState s) {
    G.prevState = G.state;
    G.state = s;
    bool captured = s == GS_PLAY || s == GS_SCAN || s == GS_LOCKPICK;
    if (captured) {
        if (!IsCursorHidden()) DisableCursor();
        G.mouseSkip = 3;
    } else if (IsCursorHidden()) EnableCursor();
}

void NewGame(bool openWorld) {
    G.player = Player();
    Player& P = G.player;
    P.zone = ZONE_VAULT;
    P.pos = V3(18.7f, 0, 7);
    P.yaw = PI / 2;
    P.inv.Add(I_VAULTSUIT);
    P.inv.Add(I_SCANNER);
    P.inv.Add(I_MULTITOOL);
    P.inv.Add(I_RECORDER);
    P.armor = I_VAULTSUIT;
    P.hp = MaxHP();
    P.ap = MaxAP();
    for (auto& q : QUESTS) { q.stage = 0; q.done = false; }
    G.flags.clear();
    G.timeOfDay = 8.5f;
    G.day = 1;
    G.messages.clear();
    G.bigMsgT = 0;
    G.fade = 0;
    G.pendingZone = -1;
    G.pipTab = G.pipSub = G.pipSel = 0;
    ResetDynamicWorld();
    FxClear();
    SetRadioStation(0);
    QUESTS[Q_MAIN].stage = 10;
    G.gameStarted = true;
    SetGameState(GS_PLAY);
    if (openWorld) {
        // Mondo aperto: il Sovrintendente ha già affidato la missione e l'equipaggiamento;
        // si parte alle porte di Ferrocittà, dove si ritrovano anche gli altri giocatori.
        G.flags["open_world"] = 1;
        G.flags["vault_opened"] = 1;
        if (Interactable* door = FindInterByTag("vault_door_in")) {
            door->open = true;
            if (door->collider >= 0) W.zones[ZONE_VAULT].boxes[door->collider].enabled = false;
        }
        int kit[][2] = {{I_CAMERA, 1}, {I_BAIT, 3}, {I_NOISE, 3}, {I_MEDKIT, 3}, {I_RADAWAY, 1}, {I_BOBBYPIN, 4}, {I_REPAIRKIT, 1}, {I_PARTS, 1}, {I_CAPS, 60}};
        for (auto& k : kit) P.inv.Add(k[0], k[1]);
        EquipTool(TOOL_CAMERA);
        QUESTS[Q_MAIN].stage = 20;
        AdventureBegin();
        if (W.locations.size() > 1) {
            W.locations[0].discovered = true; // il Rifugio 33
            Location& town = W.locations[1];
            town.discovered = true;           // si parte alle sue porte: è già sulla mappa per il viaggio rapido
            // ognuno parte in un punto un po' diverso, così i giocatori non nascono uno dentro l'altro
            std::random_device rd;
            float a = (rd() % 3600) * 0.1f * DEG2RAD, d = 1.5f + (rd() % 300) * 0.01f;
            Vector3 spawn = town.travelPos + V3(sinf(a) * d, 0, cosf(a) * d);
            ChangeZone(ZONE_WASTE, spawn, AngleTo(spawn, town.pos));
        }
        G.messages.clear();
        Notify("Mondo aperto: le Terre Bruciate sono tue. Vai dove vuoi!", Col(255, 230, 140));
        Notify("Un segnale arriva dalla rete dell’Alba: segui gli indizi nel diario.", Col(200, 230, 200));
        Notify("1-7: strumenti, clic: usa, E: interagisci. TAB: diario, Q: osservazione.", Col(200, 230, 200));
        return;
    }
    Notify("Parla con il Sovrintendente: guardalo e premi E.", Col(255, 230, 140));
    Notify("1-7: strumenti, clic: usa, E: interagisci. TAB: diario, Q: osservazione.", Col(200, 230, 200));
}

void ChangeZone(int zone, Vector3 pos, float yaw) {
    Player& P = G.player;
    bool newZone = P.zone != zone;
    P.zone = zone;
    float base = zone == ZONE_WASTE ? TerrainHeight(pos.x, pos.z) : W.zones[zone].floorY;
    pos.y = GroundHeight(zone, V3(pos.x, base + 0.6f, pos.z), 0.6f);
    P.pos = pos;
    P.yaw = yaw;
    P.pitch = 0;
    P.vel = V3(0, 0, 0);
    P.onGround = true;
    P.fallStartY = pos.y;
    FxClear();
    QuestsOnEnterZone(zone);
    if (newZone) BigNotify(zone == ZONE_WASTE ? "TERRE BRUCIATE" : W.zones[zone].name);
    SaveGame(0);
}

// ---------------------------------------------------------------------------
//  Illuminazione per ora del giorno
// ---------------------------------------------------------------------------
static Vector3 L3(Vector3 a, Vector3 b, float t) { return Vector3Lerp(a, b, Clamp01(t)); }

static void UpdateLighting(int zone, float tod) {
    Zone& z = W.zones[zone];
    if (!z.outdoor) {
        R.sunDir = V3(0, 1, 0);
        R.sunColor = V3(0, 0, 0);
        R.skyAmb = z.ambSky;
        R.groundAmb = z.ambGround;
        R.fogColor = z.fog;
        R.fogDensity = z.fogDensity;
        R.night = 0;
        R.lightLevel = 0.65f;
        return;
    }
    float a = (tod - 6.0f) / 24.0f * 2 * PI;
    Vector3 sun = Vector3Normalize(V3(cosf(a), sinf(a), 0.35f));
    R.skySun = sun;
    float elev = sun.y;
    float day = SmoothStepF(-0.12f, 0.22f, elev);
    float dusk = Clamp01(1.0f - fabsf(elev - 0.05f) / 0.22f);
    Vector3 dayCol = V3(1.08f, 0.97f, 0.82f), duskCol = V3(1.15f, 0.62f, 0.32f), moonCol = V3(0.22f, 0.27f, 0.42f);
    if (elev > -0.05f) {
        R.sunDir = Vector3Normalize(V3(sun.x, std::max(sun.y, 0.08f), sun.z));
        R.sunColor = L3(dayCol, duskCol, dusk * 0.85f) * Clamp01(day * 1.3f);
    } else {
        Vector3 moon = Vector3Normalize(V3(-sun.x, -sun.y, sun.z));
        R.sunDir = moon;
        R.sunColor = moonCol * SmoothStepF(-0.05f, -0.3f, elev);
    }
    R.skyAmb = L3(V3(0.1f, 0.12f, 0.19f), L3(V3(0.46f, 0.44f, 0.43f), V3(0.42f, 0.33f, 0.32f), dusk), day);
    R.groundAmb = L3(V3(0.06f, 0.065f, 0.085f), L3(V3(0.31f, 0.26f, 0.2f), V3(0.3f, 0.2f, 0.15f), dusk), day);
    R.fogColor = L3(V3(0.05f, 0.06f, 0.09f), L3(V3(0.76f, 0.66f, 0.52f), V3(0.8f, 0.5f, 0.34f), dusk), day);
    R.zenith = L3(V3(0.01f, 0.015f, 0.04f), L3(V3(0.36f, 0.5f, 0.64f), V3(0.3f, 0.28f, 0.44f), dusk), day);
    R.fogDensity = LerpF(0.0062f, 0.0042f, day);
    R.night = 1.0f - day;
    R.lightLevel = 0.25f + day * 0.85f;
}

// ---------------------------------------------------------------------------
//  Aggiornamento del mondo
// ---------------------------------------------------------------------------
static void WorldTick(float dt) {
    EntitiesUpdate(dt);
    InteractablesUpdate(dt);
    FxUpdate(dt);
    EmittersUpdate(G.player.zone, G.cam.position, dt);
}

static void AdvanceTime(float dt) {
    G.timeOfDay += dt * (0.5f / 60.0f);
    while (G.timeOfDay >= 24) {
        G.timeOfDay -= 24;
        G.day++;
    }
}

static void UpdateFrame(float rdt) {
    G.time += rdt;
    NetUpdate(rdt);
    for (auto& m : G.messages) m.t -= rdt;
    G.messages.erase(std::remove_if(G.messages.begin(), G.messages.end(), [](const Message& m) { return m.t <= 0; }), G.messages.end());
    if (G.bigMsgT > 0) G.bigMsgT -= rdt;

#if DUSTFALL_WEB
    // F11 è gestito dalla pagina (serve un gesto dell'utente); qui si segue lo stato reale
    G.settings.fullscreen = PlatformIsFullscreen();
    // ESC nel browser sblocca il puntatore senza arrivare al gioco: equivale alla pausa
    static bool sWasLocked = false;
    bool locked = PlatformPointerLocked();
    if (sWasLocked && !locked && G.state == GS_PLAY && G.fade == 0) SetGameState(GS_PAUSE);
    sWasLocked = locked;
#else
    if (IsKeyPressed(KEY_F11)) {
        G.settings.fullscreen = !G.settings.fullscreen;
        ApplySettings();
        SaveSettings();
    }
#endif

    // transizione tra zone (dissolvenza)
    if (G.fade > 0) {
        G.fade += rdt * 2.5f;
        if (G.fade >= 1) {
            ChangeZone(G.pendingZone, G.pendingPos, G.pendingYaw);
            G.fade = -1;
        }
        UpdateCamera3D();
        return;
    }
    if (G.fade < 0) G.fade = std::min(0.0f, G.fade + rdt * 2.0f);

    switch (G.state) {
        case GS_MENU: {
            MenuUpdate();
            float t = G.time * 0.04f;
            Vector3 c = V3(0, TerrainHeight(0, 20), 20);
            G.cam.position = c + V3(sinf(t) * 95, 32 + sinf(t * 0.7f) * 6, cosf(t) * 95);
            G.cam.target = c + V3(0, 6, 0);
            G.cam.up = V3(0, 1, 0);
            G.cam.fovy = 60;
            G.cam.projection = CAMERA_PERSPECTIVE;
            G.player.zone = ZONE_WASTE;
            FxUpdate(rdt);
            EmittersUpdate(ZONE_WASTE, G.cam.position, rdt);
            break;
        }
        case GS_INTRO: IntroUpdate(rdt); break;
        case GS_CREATE: CreateUpdate(); break;
        case GS_PLAY: {
            if (IsKeyPressed(KEY_ESCAPE)) { SetGameState(GS_PAUSE); break; }
            if (IsKeyPressed(KEY_TAB)) { PlaySfx(SFX_UI_OK); SetGameState(GS_PIPBOY); break; }
            if (IsKeyPressed(KEY_Q)) { ScanEnter(); break; }
            if (IsKeyPressed(KEY_F5)) {
                if (G.chased) Notify("Non puoi salvare mentre qualcuno ti insegue.", Col(255, 160, 100));
                else if (SaveGame(6)) Notify("Salvataggio rapido completato.");
            }
            if (IsKeyPressed(KEY_F9) && SaveExists(6)) { LoadGame(6); break; }
            if (NetPlayKeys()) break;
            PlayerUpdate(rdt);
            if (G.state != GS_PLAY) { UpdateCamera3D(); break; }
            WorldTick(rdt);
            QuestsUpdate(rdt);
            AdvanceTime(rdt);
            UpdateCamera3D();
            break;
        }
        case GS_SCAN:
            ScanUpdate(rdt);
            UpdateCamera3D();
            break;
        case GS_PIPBOY: PipBoyUpdate(); break;
        case GS_DIALOGUE: DialogueUpdate(); UpdateCamera3D(); break;
        case GS_BARTER: BarterUpdate(); break;
        case GS_LOOT: LootUpdate(); break;
        case GS_LOCKPICK: LockpickUpdate(rdt); break;
        case GS_TERMINAL: TerminalUpdate(rdt); break;
        case GS_LEVELUP: LevelUpUpdate(); break;
        case GS_PAUSE: PauseUpdate(); break;
        case GS_FAINT: FaintUpdate(); FxUpdate(rdt); break;
        case GS_ENDING: EndingUpdate(rdt); break;
        case GS_SLEEP: {
            float before = G.timeOfDay;
            SleepUpdate(rdt);
            if (G.timeOfDay >= 24) { G.timeOfDay -= 24; G.day++; }
            (void)before;
            break;
        }
        case GS_SAVELOAD: SaveLoadUpdate(); break;
        case GS_OPTIONS: OptionsUpdate(); break;
        case GS_CHAT:
            // mentre si scrive il mondo va avanti (è una partita in compagnia)
            ChatUpdate();
            WorldTick(rdt);
            QuestsUpdate(rdt);
            AdvanceTime(rdt);
            UpdateCamera3D();
            break;
        case GS_ONLINE: OnlineUpdate(); break;
    }
    AudioUpdate(rdt);
}

// ---------------------------------------------------------------------------
//  Disegno
// ---------------------------------------------------------------------------
static void DrawWorld() {
    int zone = G.player.zone;
    float tod = G.state == GS_MENU ? 17.4f : G.timeOfDay;
    UpdateLighting(zone, tod);
    Camera3D cam = G.cam;
    Color clear = Color{(unsigned char)(R.fogColor.x * 255), (unsigned char)(R.fogColor.y * 255), (unsigned char)(R.fogColor.z * 255), 255};
    ClearBackground(clear);
    BeginMode3D(cam);
    Zone& z = W.zones[zone];
    if (z.outdoor) DrawSky(cam);
    RenderApplyFrameUniforms(cam.position, G.time);
    std::vector<LightSrc> lights = z.lights;
    FxCollectLights(zone, lights);
    if (zone == ZONE_VAULT) {
        // luce del nucleo del reattore: rossa se guasto, azzurra se riparato
        bool fixed = G.flags["reactor_fixed"] != 0;
        LightSrc l;
        l.pos = V3(-22, 3.5f, 8);
        l.color = fixed ? Col(110, 190, 255) : Col(255, 110, 50);
        l.range = 16;
        l.intensity = fixed ? 1.4f : 1.1f;
        l.flicker = fixed ? 0.0f : 0.5f;
        lights.push_back(l);
    }
    if (zone == ZONE_WASTE && G.flags["farm_power"]) {
        // la fattoria con il generatore riparato
        LightSrc l;
        l.pos = V3(-112, TerrainHeight(-112, 165) + 2.8f, 165);
        l.color = Col(255, 210, 140);
        l.range = 14;
        l.intensity = 1.3f;
        lights.push_back(l);
    }
    RenderSetLights(lights, cam.position, G.time);
    DrawZoneStatic(zone, cam, z.outdoor ? 420.0f : 140.0f);
    InteractablesDraw(cam);
    EntitiesDraw(cam);
    if (G.state != GS_MENU) DrawPlayerBody();
    rlDrawRenderBatchActive();
    DrawSigns(zone, cam);
    EntitiesDrawShadows(cam);
    NetDraw3D(cam);
    FxDraw(cam);
    if (G.state == GS_PLAY || G.state == GS_LOOT) DrawViewModel();
    EndMode3D();
}

static void DrawFrame() {
    BeginDrawing();
    bool opaque2D = G.state == GS_INTRO || G.state == GS_CREATE || G.state == GS_TERMINAL || (G.state == GS_ENDING);
    if (!opaque2D) DrawWorld();
    else ClearBackground(BLACK);
    switch (G.state) {
        case GS_MENU: MenuDraw(); NetDrawHud(); break;
        case GS_INTRO: IntroDraw(); break;
        case GS_CREATE: CreateDraw(); break;
        case GS_PLAY:
            HudDraw();
            NetDrawHud();
#if DUSTFALL_WEB
            if (!PlatformPointerLocked() && !PlatformTouch() && G.fade == 0) {
                UIRect(UIW() / 2 - 250, 456, 500, 34, Color{0, 0, 0, 160});
                UIText("Clicca per controllare la visuale con il mouse", UIW() / 2, 462, 19, PIP, 1);
            }
#endif
            break;
        case GS_SCAN: HudDraw(); ScanDraw(); break;
        case GS_PIPBOY: PipBoyDraw(); break;
        case GS_DIALOGUE: DialogueDraw(); break;
        case GS_BARTER: BarterDraw(); break;
        case GS_LOOT: HudDraw(); LootDraw(); break;
        case GS_LOCKPICK: LockpickDraw(); break;
        case GS_TERMINAL: TerminalDraw(); break;
        case GS_LEVELUP: LevelUpDraw(); break;
        case GS_PAUSE: PauseDraw(); break;
        case GS_FAINT: FaintDraw(); break;
        case GS_ENDING: EndingDraw(); break;
        case GS_SLEEP: SleepDraw(); break;
        case GS_SAVELOAD: SaveLoadDraw(); break;
        case GS_OPTIONS: OptionsDraw(); break;
        case GS_CHAT: HudDraw(); NetDrawHud(); break;
        case GS_ONLINE: OnlineDraw(); break;
    }
    UIDrawMessages();
    float fa = G.fade > 0 ? G.fade : -G.fade;
    if (fa > 0) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, (unsigned char)(255 * Clamp01(fa))});
    EndDrawing();
}

#if !DUSTFALL_WEB
static Image MakeIcon() {
    const int S = 64;
    Image img = GenImageColor(S, S, BLANK);
    ImageDrawCircle(&img, S / 2, S / 2, S / 2 - 1, Col(40, 34, 24));
    ImageDrawCircle(&img, S / 2, S / 2, S / 2 - 5, Col(225, 180, 40));
    ImageDrawCircle(&img, S / 2, S / 2, S / 2 - 9, Col(40, 70, 120));
    for (int k = 0; k < 3; k++) {
        float a = k * PI / 3;
        for (int i = 0; i < 360; i += 2) {
            float t = i * DEG2RAD;
            float x = cosf(t) * 20, y = sinf(t) * 7;
            int px = (int)(S / 2 + x * cosf(a) - y * sinf(a)), py = (int)(S / 2 + x * sinf(a) + y * cosf(a));
            ImageDrawPixel(&img, px, py, Col(240, 230, 200));
            ImageDrawPixel(&img, px + 1, py, Col(240, 230, 200));
        }
    }
    ImageDrawCircle(&img, S / 2, S / 2, 5, Col(255, 90, 60));
    return img;
}
#endif


// ---------------------------------------------------------------------------
//  Self-test: gioca automaticamente la trama principale (per il debug)
// ---------------------------------------------------------------------------
static int sFail = 0;
static void Check(bool ok, const char* what) {
    printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) sFail++;
}

static void SimTick(float dt) {
    G.time += dt;
    G.player.hp = std::max(G.player.hp, 50.0f);
    PlayerUpdate(dt);
    WorldTick(dt);
    QuestsUpdate(dt);
    UpdateCamera3D();
    if (G.fade < 0) G.fade = std::min(0.0f, G.fade + dt * 2.0f);
    if (G.state == GS_FAINT) G.state = GS_PLAY;
}

static void Sim(float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 30) SimTick(1.0f / 30);
}

// completa una dissolvenza in corso (scorta fuori, svenimento, viaggio)
static void FinishFade() {
    if (G.fade > 0 && G.pendingZone >= 0) ChangeZone(G.pendingZone, G.pendingPos, G.pendingYaw);
    G.fade = 0;
}

static bool ChooseOption(const char* contains) {
    for (auto& o : G.dlg.opts)
        if (o.text.find(contains) != std::string::npos) {
            auto act = o.act;
            bool siteDialogue = G.dlg.npc < 0;
            act();
            if (siteDialogue) return true;
            if (G.state != GS_DIALOGUE) return true;
            if (G.dlg.end) { SetGameState(GS_PLAY); return true; }
            G.dlg.node = G.dlg.next;
            DialogueBuild();
            return true;
        }
    printf("   opzione non trovata: %s\n", contains);
    for (auto& o : G.dlg.opts) printf("      - %s\n", o.text.c_str());
    return false;
}

static int FindNpc(const char* dlg) {
    for (int i = 0; i < (int)G.ents.size(); i++)
        if (G.ents[i].dlg == dlg) return i;
    return -1;
}

static int InterIdx(const char* tag) {
    Interactable* it = FindInterByTag(tag);
    return it ? (int)(it - &G.inters[0]) : -1;
}

static void Teleport(int zone, float x, float z) {
    ChangeZone(zone, V3(x, 0, z), 0);
    G.messages.clear();
}

static void LootAll(int idx) {
    if (idx < 0) return;
    Interactable& it = G.inters[idx];
    it.locked = false;
    Interact(idx);
    for (auto s : it.inv.items) GiveItem(s.id, s.count, false);
    it.inv.Clear();
    SetGameState(GS_PLAY);
}

// Raggiunge un punto realmente libero e visibile: gli strumenti non attraversano muri.
static bool ApproachSite(const char* tag) {
    int idx = InterIdx(tag);
    if (idx < 0) { Check(false, tag); return false; }
    const Interactable& site = G.inters[idx];
    ChangeZone(site.zone, V3(site.pos.x, 0, site.pos.z), 0);
    Player& p = G.player;
    for (int k = 0; k < 32; ++k) {
        float a = (k % 16) * PI / 8;
        float radius = k < 16 ? 2.1f : 3.0f;
        Vector3 pos = site.pos + V3(sinf(a) * radius, 0, cosf(a) * radius);
        float base = site.zone == ZONE_WASTE ? TerrainHeight(pos.x, pos.z) : W.zones[site.zone].floorY;
        pos.y = GroundHeight(site.zone, V3(pos.x, base + 0.6f, pos.z), 0.6f);
        if (CircleBlocked(site.zone, pos, 0.35f, 1.75f, 0.5f)) continue;
        if (Vector3Distance(pos + V3(0, p.eyeH, 0), site.pos) > 4.0f) continue;
        if (!LineOfSight(site.zone, pos + V3(0, p.eyeH, 0), site.pos)) continue;
        p.pos = pos;
        Vector3 to = site.pos - PlayerEye();
        p.yaw = atan2f(to.x, to.z);
        p.pitch = atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z));
        p.catchImmune = 99999;
        G.fade = 0;
        SetGameState(GS_PLAY);
        UpdateCamera3D();
        return true;
    }
    printf("   sito non raggiungibile: %s\n", tag);
    Check(false, "ogni sito dell'Atlante ha un punto di accesso libero e visibile");
    return false;
}

static void ToolAt(const char* tag, int tool) {
    if (!ApproachSite(tag)) return;
    G.player.tool = tool;
    G.player.useCd = 0;
    AdventureUseTool(tool);
    printf("   [strumento] %s: capitolo %d/%d/%d/%d/%d/%d\n", tag,
           QUESTS[Q_ECHO].stage, QUESTS[Q_ARCHIVE].stage, QUESTS[Q_GARDEN].stage,
           QUESTS[Q_OBSERVATORY].stage, QUESTS[Q_CONCORD].stage, QUESTS[Q_BEACON].stage);
}

static void ReadAt(const char* tag) {
    if (ApproachSite(tag)) Interact(InterIdx(tag));
}

// scatta una foto a un bersaglio cercando un punto libero e in vista attorno a esso
static bool Snap(Vector3 target, float dist) {
    Player& P = G.player;
    size_t before = P.photos.size();
    P.tool = TOOL_CAMERA;
    for (int k = 0; k < 16; k++) {
        float a = k * PI / 8, d = dist * (k < 8 ? 1.0f : 0.6f);
        Vector3 p = target + V3(sinf(a) * d, 0, cosf(a) * d);
        float base = P.zone == ZONE_WASTE ? TerrainHeight(p.x, p.z) : W.zones[P.zone].floorY;
        p.y = GroundHeight(P.zone, V3(p.x, std::max(base, target.y - 1.0f) + 1.0f, p.z), 1.0f);
        if (CircleBlocked(P.zone, p, 0.35f, 1.75f, 0.5f)) continue;
        P.pos = p;
        Vector3 to = target - PlayerEye();
        P.yaw = atan2f(to.x, to.z);
        P.pitch = atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z));
        UpdateCamera3D();
        Vector3 seen = target + Vector3Normalize(G.cam.position - target) * std::min(3.5f, d * 0.5f);
        if (!LineOfSight(P.zone, G.cam.position, seen)) continue;
        P.useCd = 0;
        TakePhoto();
        if (P.photos.size() > before) return true;
    }
    return false;
}

static Entity* FirstOfKind(EntKind k, int zone, Vector3 near, float radius) {
    Entity* best = nullptr;
    float bd = radius;
    for (auto& e : G.ents) {
        if (e.kind != k || e.zone != zone) continue;
        float d = DistXZ(e.pos, near);
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}

static void DrawAllScreens() {
    GameState states[] = {GS_MENU, GS_PLAY, GS_SCAN, GS_PIPBOY, GS_PAUSE, GS_SAVELOAD, GS_OPTIONS, GS_FAINT, GS_SLEEP, GS_LEVELUP};
    GameState keep = G.state;
    for (GameState st : states) {
        G.state = st;
        if (st == GS_LEVELUP) { G.player.pendingLevels = std::max(1, G.player.pendingLevels); LevelUpOpen(); }
        for (int tab = 0; tab < 5; tab++) {
            G.pipTab = tab;
            for (int sub = 0; sub < 5; sub++) {
                G.pipSub = sub;
                DrawFrame();
                if (st != GS_PIPBOY) break;
            }
            if (st != GS_PIPBOY) break;
        }
    }
    G.state = keep;
}

static int RunSelfTest() {
    printf("=== SELF-TEST DUSTFALL: ATLANTE DELL’ALBA, ESPLORAZIONE E FURTIVITA’ ===\n");
    NewGame();
    Player& P = G.player;
    const float SAFE = 99999;
    Check(P.zone == ZONE_VAULT, "inizio nel Rifugio");
    int ov = FindNpc("overseer");
    Check(ov >= 0, "Sovrintendente presente");
    DialogueStart(ov);
    ChooseOption("Cosa posso fare");
    ChooseOption("Farò del mio meglio");
    ChooseOption("Tornerò");
    Check(QUESTS[Q_MAIN].stage == 20, "missione principale avviata (stadio 20)");
    Check(P.inv.Count(I_CAMERA) == 1 && P.tool == TOOL_CAMERA, "fotocamera ricevuta e in mano");
    Check(P.inv.Count(I_BAIT) == 3 && P.inv.Count(I_NOISE) == 3, "esche e barattoli ricevuti");
    Interactable* door = FindInterByTag("vault_door_in");
    Interact(InterIdx("vault_door_in"));
    Sim(5);
    Check(door->open && !W.zones[ZONE_VAULT].boxes[door->collider].enabled, "porta del Rifugio aperta");
    Snap(V3(-22, 3.5f, 8), 9);
    Check(HasPhoto("reactor"), "foto del reattore");

    // --- Ferrocittà ---
    P.catchImmune = SAFE;
    Teleport(ZONE_WASTE, 0, 62);
    Sim(1.5f);
    Check(W.locations[1].discovered, "Ferrocittà scoperta");
    Check(QUESTS[Q_MAIN].stage == 30, "stadio 30: parla con lo sceriffo");
    int sh = FindNpc("sheriff");
    DialogueStart(sh);
    ChooseOption("Cerco la Base");
    ChooseOption("Allora ci parlerò");
    ChooseOption("Buono a sapersi");
    Check(QUESTS[Q_MAIN].stage == 40, "stadio 40: ottieni la tessera");
    for (int i = 0; i < SK_COUNT; i++) P.skillPts[i] = 60; // come dopo qualche livello
    P.inv.Add(I_REPAIRKIT, 3); // acquistati da Gus
    P.inv.Add(I_PARTS, 4);
    DialogueStart(sh);
    ChooseOption("Posso aiutare");
    ChooseOption("Ci penso io");
    Check(QUESTS[Q_WATER].stage == 10, "missione Acqua avviata");
    Interact(InterIdx("pump"));
    Check(FindInterByTag("pump")->used && QUESTS[Q_WATER].stage == 20, "pompa dell'acqua riparata");
    DialogueStart(sh);
    ChooseOption("La pompa dell'acqua funziona");
    ChooseOption("Felice di aiutare");
    Check(QUESTS[Q_WATER].done, "missione Acqua completata");
    Snap(V3(2, TerrainHeight(2, -2) + 12.5f, -2), 22);
    Check(HasPhoto("water_tower"), "foto della torre dell'acqua");

    // --- Stazione Atomo: sacchi, esche, essere scacciati ---
    int tano = FindNpc("barman");
    DialogueStart(tano);
    ChooseOption("Hai qualche lavoro");
    ChooseOption("Affare fatto");
    Check(QUESTS[Q_ROACH].stage == 10, "missione Ospiti indesiderati avviata");
    Teleport(ZONE_WASTE, 60, 150);
    Entity* roach = FirstOfKind(EK_ROACH, ZONE_WASTE, V3(60, 0, 185), 30);
    Check(roach != nullptr, "radscarafaggi alla stazione");
    if (roach) {
        int caught = P.timesCaught;
        float hp = P.hp;
        int capsBefore = P.inv.Count(I_CAPS);
        P.catchImmune = 0;
        P.pos = roach->pos + V3(0.9f, 0, 0);
        roach->aware = true;
        roach->ai = AI_CHASE;
        roach->lastSeen = P.pos;
        for (int i = 0; i < 30 && P.timesCaught == caught; i++) SimTick(1.0f / 30);
        Check(P.timesCaught == caught + 1 && G.fade > 0, "un radscarafaggio ti scaccia (nessun combattimento)");
        Check(P.hp >= hp - 0.01f && P.inv.Count(I_CAPS) == capsBefore && P.inv.Count(I_SCANNER) == 1, "essere scoperti non ferisce e non sottrae oggetti o tappi");
        FinishFade();
        Check(DistXZ(P.pos, V3(60, 0, 180)) > 25, "riaccompagnato lontano dal territorio");
        P.catchImmune = SAFE;
        Teleport(ZONE_WASTE, 60, 150);
        Sim(4);
        Vector3 rp = roach->pos;
        ThrowObject(ZONE_WASTE, rp + V3(2, 2, 0), V3(0, -2, 0), TOOL_BAIT);
        Sim(1.5f);
        Check(roach->ai == AI_DISTRACT, "l'esca distrae i radscarafaggi");
        Check(Snap(EntityCenter(*roach), 14) || HasPhoto("roach"), "foto di un radscarafaggio");
    }
    for (auto& it : G.inters)
        if (it.tag == "roach_sack" && !it.used) Interact((int)(&it - &G.inters[0]));
    Sim(1);
    Check(P.inv.Count(I_ROTFOOD) == 3 && QUESTS[Q_ROACH].stage == 20, "tre sacchi di cibo marcio portati via");
    Check(G.flags["roaches_left"] == 1, "i radscarafaggi lasciano la stazione");
    Snap(V3(46, TerrainHeight(46, 171) + 10.5f, 171), 26);
    Check(HasPhoto("atom_sign"), "foto dell'insegna atomica");
    Teleport(ZONE_WASTE, 0, 30);
    DialogueStart(tano);
    ChooseOption("I radscarafaggi hanno lasciato");
    ChooseOption("Grazie, Tano");
    Check(QUESTS[Q_ROACH].done && P.inv.Count(I_STEALTHBOY) > 0 && P.inv.Count(I_ROTFOOD) == 0, "Ospiti indesiderati completata");

    // --- Il cane di Rita ---
    int rita = FindNpc("rita");
    DialogueStart(rita);
    ChooseOption("Lo cercherò io");
    ChooseOption("Lo riporterò a casa");
    Check(QUESTS[Q_DOG].stage == 10, "missione Il cane di Rita avviata");
    Teleport(ZONE_WASTE, -140, 160);
    Sim(1);
    Check(W.locations[2].discovered || DistXZ(P.pos, V3(-120, 0, 170)) < 40, "Fattoria Abbandonata raggiunta");
    Entity* mole = FirstOfKind(EK_MOLERAT, ZONE_WASTE, V3(-100, 0, 185), 40);
    if (mole) Snap(EntityCenter(*mole), 16);
    Check(HasPhoto("molerat"), "foto di un ratto talpa");
    Snap(V3(-142, TerrainHeight(-142, 162) + 11, 162), 30);
    Check(HasPhoto("silo"), "foto del silo");
    P.inv.Add(I_MEAT, 2);
    int dog = FindNpc("ruggine");
    DialogueStart(dog);
    ChooseOption("Offri una striscia");
    ChooseOption("Andiamo a casa");
    Check(G.ents[dog].companion && QUESTS[Q_DOG].stage == 20, "Ruggine diventa tuo amico e ti segue");
    Interact(InterIdx("generator"));
    Check(G.flags["farm_power"] == 1, "generatore della fattoria riparato");
    Teleport(ZONE_WASTE, -7, 44);
    Sim(1.5f);
    Check(QUESTS[Q_DOG].stage == 30, "Ruggine riportato da Rita");
    Snap(EntityCenter(G.ents[dog]), 5);
    Check(HasPhoto("ruggine"), "foto di Ruggine");
    DialogueStart(rita);
    ChooseOption("Era nel fienile");
    ChooseOption("Grazie, Rita");
    Check(QUESTS[Q_DOG].done && G.ents[dog].companion, "Il cane di Rita completata, Ruggine resta con te");

    // --- Jack Falco: catalogo e bestia ---
    int jack = FindNpc("naturalist");
    DialogueStart(jack);
    ChooseOption("Posso aiutarti");
    ChooseOption("Lo farò");
    ChooseOption("Grazie, Jack");
    ChooseOption("bestia leggendaria");
    ChooseOption("Lo fotograferò io");
    ChooseOption("Ci vado");
    Check(QUESTS[Q_PHOTO].stage >= 10 && QUESTS[Q_BEAST].stage == 10, "missioni di Jack avviate");
    Teleport(ZONE_WASTE, -250, -255);
    Sim(1);
    Entity* beast = FindEntityByTag("beast");
    P.crouch = true;
    P.catchImmune = 0;
    Sim(0.5f);
    if (beast) Snap(EntityCenter(*beast), 13);
    Sim(0.5f);
    Check(HasPhoto("deathclaw") && QUESTS[Q_BEAST].stage == 20, "foto dello Squarciatore");
    Check(beast && beast->asleep, "lo Squarciatore continua a dormire");
    P.catchImmune = SAFE;
    LootAll(InterIdx("jack_pack"));
    Check(P.inv.Count(I_JOURNAL) == 1, "diario di Jack recuperato");
    P.crouch = false;
    Teleport(ZONE_WASTE, -258, 248 - 20);
    Snap(V3(-240, TerrainHeight(-240, 248) + 4, 248), 20);
    Check(HasPhoto("vault_door"), "foto della porta del Rifugio");

    // --- Segnale nel deserto ---
    SetRadioStation(2);
    Sim(8);
    Check(QUESTS[Q_SIGNAL].stage == 10, "segnale radio captato");
    SetRadioStation(0);
    Teleport(ZONE_WASTE, -271.5f, -4.5f);
    Sim(0.5f);
    Entity* wild = FirstOfKind(EK_DOG, ZONE_WASTE, V3(-294, 0, -10), 30);
    if (wild && !wild->companion) Snap(EntityCenter(*wild), 15);
    Check(HasPhoto("dog"), "foto di un cane selvatico");
    int vera = FindNpc("scout");
    DialogueStart(vera);
    ChooseOption("Come posso aiutarti");
    ChooseOption("Ci penso io");
    Check(QUESTS[Q_SIGNAL].stage == 20, "Vera trovata: ripara il trasmettitore");
    float g0 = TerrainHeight(-280, -10);
    Interact(InterIdx("tower_up"));
    Check(P.pos.y > g0 + 14, "salito sulla Torre Radio con la scala");
    Interact(InterIdx("transmitter"));
    Check(QUESTS[Q_SIGNAL].stage == 30, "trasmettitore riparato");
    Interact(InterIdx("tower_down"));
    Check(P.pos.y < g0 + 3, "sceso dalla torre");
    Interact(InterIdx("record"));
    Check(P.inv.Count(I_RECORD) == 1, "disco d'anteguerra raccolto");
    DialogueStart(vera);
    ChooseOption("Sono contento");
    ChooseOption("Grazie, Vera");
    Check(QUESTS[Q_SIGNAL].stage == 40 && P.inv.Count(I_CACHEKEY) == 1, "chiave del baule ricevuta");
    Teleport(ZONE_WASTE, -128, -145);
    Interact(InterIdx("cache"));
    SetGameState(GS_PLAY);
    Check(!FindInterByTag("cache")->locked && QUESTS[Q_SIGNAL].done, "baule aperto, Segnale nel deserto completata");
    Entity* ghoul = FirstOfKind(EK_GHOUL, ZONE_WASTE, V3(-170, 0, -120), 40);
    if (ghoul) Snap(EntityCenter(*ghoul), 16);
    P.rads = 0;

    // --- Medicine (ospedale delle rovine) ---
    Teleport(ZONE_WASTE, 0, 30);
    int doc = FindNpc("doctor");
    DialogueStart(doc);
    ChooseOption("Hai bisogno di aiuto");
    ChooseOption("Ci penso io");
    Check(QUESTS[Q_MEDS].stage == 10, "missione Medicine avviata");
    Teleport(ZONE_WASTE, 79, -165);
    LootAll(InterIdx("meds"));
    Check(QUESTS[Q_MEDS].stage == 20, "rifornimenti medici trovati");
    Entity* mut = FirstOfKind(EK_MUTANT, ZONE_WASTE, V3(62, 0, -234), 30);
    if (mut) Snap(EntityCenter(*mut), 18);
    Teleport(ZONE_WASTE, 0, 30);
    DialogueStart(doc);
    ChooseOption("Ho trovato i rifornimenti");
    ChooseOption("Felice di aiutare");
    Check(QUESTS[Q_MEDS].done, "missione Medicine completata");

    // --- Jack: consegne ---
    printf("   foto nel catalogo: %d\n", (int)P.photos.size());
    Check(QUESTS[Q_PHOTO].stage == 20, "otto fotografie per il catalogo");
    int cloaks = P.inv.Count(I_DARKCLOAK); // il bottino casuale può contenerne già una
    DialogueStart(jack);
    ChooseOption("Ho fotografato lo Squarciatore");
    ChooseOption("Grazie, Jack");
    ChooseOption("Ho le foto");
    ChooseOption("Grazie!");
    ChooseOption("Ho trovato il tuo diario");
    ChooseOption("piacere");
    SetGameState(GS_PLAY);
    Check(QUESTS[Q_BEAST].done && QUESTS[Q_PHOTO].done && P.inv.Count(I_DARKCLOAK) == cloaks + 1 && P.inv.Count(I_JOURNAL) == 0, "La bestia della gola e Il catalogo di Jack completate");

    // --- Deposito dei Raccoglitori ---
    Teleport(ZONE_WASTE, 194, -88);
    Sim(0.5f);
    Entity* raider = nullptr;
    for (auto& e : G.ents)
        if (e.fac == FAC_RAIDER && e.hostile && e.tag.empty() && DistXZ(e.pos, V3(230, 0, -90)) < 30) { raider = &e; break; }
    Check(raider != nullptr, "i Teschi sorvegliano il covo");
    if (raider) {
        ThrowObject(ZONE_WASTE, raider->pos + V3(3, 2, 0), V3(0, -2, 0), TOOL_NOISE);
        Sim(1.5f);
        Check(raider->ai == AI_SEARCH, "un richiamo sonoro attira un custode");
    }
    int bruto = FindNpc("raider_guard");
    DialogueStart(bruto);
    ChooseOption("Paga 50");
    ChooseOption("Grazie.");
    Sim(1);
    Check(G.flags["raider_pass"] && raider && !raider->hostile, "permesso di Bruno: i Raccoglitori ti accolgono");
    int boss = FindNpc("boss");
    Check(boss >= 0 && !G.ents[boss].hostile, "Neri disponibile a parlare");
    DialogueStart(boss);
    ChooseOption("Mi serve la tessera");
    ChooseOption("Ho un disco");
    ChooseOption("Buon ascolto");
    Check(P.inv.Count(I_KEYCARD) == 1 && QUESTS[Q_MAIN].stage == 45 && G.flags["boss_deal"] == 3, "tessera ottenuta in cambio del disco");
    Check(FindInterByTag("boss_safe")->inv.Count(I_KEYCARD) == 0, "la tessera non è più in cassaforte");

    // --- Base Delta-9 ---
    Teleport(ZONE_WASTE, 190, 220);
    Interactable* gate = FindInterByTag("base_gate");
    Interact(InterIdx("base_gate"));
    Check(gate->open, "cancello aperto con la tessera");
    Check(QUESTS[Q_MAIN].stage == 50, "stadio 50: entra nei laboratori");
    Sim(2);
    Snap(V3(200, TerrainHeight(200, 220) + 3, 220), 24);
    Entity* cam = FirstOfKind(EK_CAMERA, ZONE_WASTE, V3(270, 0, 220), 30);
    if (cam) {
        P.catchImmune = 0;
        P.crouch = false;
        Vector3 cp = cam->pos + YawDir(cam->yaw) * 8;
        Teleport(ZONE_WASTE, cp.x, cp.z);
        P.catchImmune = 0;
        for (int i = 0; i < 120 && !cam->aware; i++) SimTick(1.0f / 30);
        Check(cam->aware, "la telecamera ti vede e dà l'allarme");
        Snap(EntityCenter(*cam), 6);
        P.catchImmune = SAFE;
        Sim(6);
    }
    Entity* robot = FirstOfKind(EK_ROBOT, ZONE_WASTE, V3(250, 0, 220), 60);
    if (robot) {
        Snap(EntityCenter(*robot), 12);
        int caught = P.timesCaught;
        P.catchImmune = 0;
        Vector3 rp = robot->pos + YawDir(robot->yaw) * 5;
        P.pos = V3(rp.x, GroundHeight(ZONE_WASTE, V3(rp.x, robot->pos.y + 1, rp.z), 1.0f), rp.z);
        for (int i = 0; i < 600 && P.timesCaught == caught; i++) SimTick(1.0f / 30);
        Check(P.timesCaught > caught, "una sentinella ti scorta fuori dalla base");
        FinishFade();
        Check(DistXZ(P.pos, V3(185, 0, 222)) < 6, "riaccompagnato al cancello");
        P.catchImmune = SAFE;
    }
    Interactable* secOut = FindInterByTag("sec_term_out");
    if (secOut) TerminalAction("security_off", *secOut);
    int on = 0;
    for (auto& e : G.ents)
        if (e.zone == ZONE_WASTE && (e.kind == EK_ROBOT || e.kind == EK_CAMERA) && !e.off) on++;
    Check(on == 0, "sorveglianza esterna disattivata dal terminale");
    Teleport(ZONE_BASE, 0, -2);
    Check(QUESTS[Q_MAIN].stage == 55, "stadio 55: trova la cella");
    Interactable* secIn = FindInterByTag("sec_term_in");
    if (secIn) {
        TerminalAction("security_off", *secIn);
        TerminalAction("unlock_storage", *secIn);
    }
    Entity* mk5 = FindEntityByTag("mk5");
    on = 0;
    for (auto& e : G.ents)
        if (e.zone == ZONE_BASE && (e.kind == EK_ROBOT || e.kind == EK_CAMERA) && !e.off && e.tag != "mk5") on++;
    Check(on == 0 && mk5 && !mk5->off, "sorveglianza interna spenta, la Mk.V resta attiva");
    if (mk5) Snap(EntityCenter(*mk5), 9);
    LootAll(InterIdx("maint_locker"));
    Check(P.inv.Count(I_MAINTCODE) == 1, "codice di manutenzione trovato");
    int lab = InterIdx("lab_term");
    TerminalStart(lab);
    Check(G.state == GS_TERMINAL && G.inters[lab].hacked, "il codice apre il terminale del laboratorio");
    TerminalAction("mk5_off", G.inters[lab]);
    SetGameState(GS_PLAY);
    Check(mk5 && mk5->off, "Sentinella Mk.V in manutenzione");
    Interact(InterIdx("fusion_cell"));
    Check(P.inv.Count(I_FUSIONCELL) == 1 && QUESTS[Q_MAIN].stage == 60, "cella di fusione recuperata (stadio 60)");
    Interactable* store = FindInterByTag("storage_door");
    Check(store && !store->locked, "il terminale ha sbloccato il magazzino");

    // La cella da sola non può concludere la nuova storia.
    Teleport(ZONE_VAULT, -16, 8);
    Interact(InterIdx("reactor"));
    Check(!QUESTS[Q_MAIN].done && G.state != GS_ENDING && P.inv.Count(I_FUSIONCELL) == 1,
          "la vecchia consegna della cella non salta i sei capitoli dell'Atlante");
    Check(QUESTS[Q_ECHO].stage == 10, "la prima traccia dell'Atlante è nel diario");
    Check(P.inv.Count(I_SCANNER) == 1 && P.inv.Count(I_MULTITOOL) == 1 && P.inv.Count(I_RECORDER) == 1,
          "scanner, multistrumento e registratore sono strumenti riutilizzabili");
    DropItem(I_SCANNER);
    Check(P.inv.Count(I_SCANNER) == 1, "il kit essenziale resta disponibile per tutta la campagna");
    P.tool = TOOL_SCANNER;
    AdventureUseTool(TOOL_SCANNER);
    Check(QUESTS[Q_ECHO].stage == 10, "lo scanner non risolve un indizio da un'altra zona");
    if (ApproachSite("atlas_echo_relay")) {
        P.inv.Remove(I_SCANNER, 1);
        AdventureUseTool(TOOL_SCANNER);
        Check(QUESTS[Q_ECHO].stage == 10, "un'analisi richiede lo strumento presente nel kit");
        P.inv.Add(I_SCANNER);
    }
    int oldXP = P.xp;
    ToolAt("atlas_echo_relay", TOOL_RECORDER);
    Check(QUESTS[Q_ECHO].stage == 10 && P.xp == oldXP, "uno strumento inadatto non risolve l'indizio");
    ToolAt("atlas_echo_relay", TOOL_SCANNER);
    Check(QUESTS[Q_ECHO].stage == 20, "lo scanner individua la frequenza del ripetitore");
    ToolAt("atlas_echo_voice", TOOL_RECORDER);
    Check(QUESTS[Q_ECHO].done && QUESTS[Q_ARCHIVE].stage == 10, "la voce registrata conduce all'archivio");
    oldXP = P.xp;
    ToolAt("atlas_echo_voice", TOOL_RECORDER);
    Check(P.xp == oldXP, "registrare di nuovo una traccia non duplica i PE");

    ReadAt("atlas_archive_desk");
    Check(QUESTS[Q_ARCHIVE].stage == 20, "i registri spiegano la chiusura dell'archivio");
    ToolAt("atlas_archive_memory", TOOL_RECORDER);
    ToolAt("atlas_archive_route", TOOL_MULTITOOL);
    Check(QUESTS[Q_ARCHIVE].done && QUESTS[Q_GARDEN].stage == 10, "ripristinato il percorso dell'archivio senza affrontare guardiani");

    ToolAt("atlas_soil_a", TOOL_SCANNER);
    ToolAt("atlas_soil_b", TOOL_SCANNER);
    ToolAt("atlas_soil_c", TOOL_SCANNER);
    Check(QUESTS[Q_GARDEN].stage == 20, "tre analisi del suolo aprono il progetto della serra");
    ToolAt("atlas_garden_valve", TOOL_MULTITOOL);
    ReadAt("atlas_seedbed");
    Check(ChooseOption("Fiori delicati"), "si puo' sperimentare una soluzione diversa");
    Check(QUESTS[Q_GARDEN].stage == 30 && !QUESTS[Q_GARDEN].done, "una scelta inadatta non consuma risorse e permette di riprovare");
    ReadAt("atlas_seedbed");
    Check(ChooseOption("Semi resistenti"), "scelta dei semi basata sulle analisi del terreno");
    Check(QUESTS[Q_GARDEN].done && QUESTS[Q_OBSERVATORY].stage == 10, "il giardino riparte e rivela l'osservatorio");

    ToolAt("atlas_sky_west", TOOL_SCANNER);
    ToolAt("atlas_sky_east", TOOL_SCANNER);
    ToolAt("atlas_sky_north", TOOL_SCANNER);
    Check(QUESTS[Q_OBSERVATORY].stage == 20, "i tre riferimenti sbloccano l'ottica della Specola");
    ToolAt("atlas_lens", TOOL_MULTITOOL);
    Check(QUESTS[Q_OBSERVATORY].stage == 30, "ottica della Specola calibrata");
    ReadAt("atlas_star_chart");
    Check(ChooseOption("Nord, Ovest, Est"), "l'enigma della Specola permette un tentativo errato");
    Check(QUESTS[Q_OBSERVATORY].stage == 30, "la rotta errata mantiene l'enigma disponibile");
    ReadAt("atlas_star_chart");
    Check(ChooseOption("Ovest, Est, Nord"), "ricostruita la sequenza dei tre orizzonti");
    Check(QUESTS[Q_OBSERVATORY].done && QUESTS[Q_CONCORD].stage == 10, "la mappa del cielo conduce alle comunità");

    ToolAt("atlas_voice_town", TOOL_RECORDER);
    Check(ChooseOption("Registriamo"), "gli abitanti autorizzano la testimonianza");
    ToolAt("atlas_voice_depot", TOOL_RECORDER);
    Check(ChooseOption("Registriamo"), "i raccoglitori autorizzano la propria testimonianza");
    ReadAt("atlas_council");
    Check(ChooseOption("Condividiamo l'acqua"), "la comunità sceglie il primo uso della rete");
    Check(ChooseOption("Solo le mappe"), "si decide quali informazioni condividere");
    Check(QUESTS[Q_CONCORD].done && QUESTS[Q_BEACON].stage == 10, "l'accordo tra le comunità apre l'ultimo capitolo");

    ToolAt("atlas_link_radio", TOOL_MULTITOOL);
    ToolAt("atlas_link_farm", TOOL_MULTITOOL);
    ToolAt("atlas_link_base", TOOL_MULTITOOL);
    Check(QUESTS[Q_BEACON].stage == 20, "tre collegamenti riattivati con il multistrumento");
    ToolAt("atlas_beacon", TOOL_RECORDER);
    Check(ChooseOption("Invia l'Atlante"), "il primo segnale parte con la scelta del giocatore");
    Check(QUESTS[Q_BEACON].done && AdventureReadyForEnding(), "tutti i sei capitoli sono necessari per il finale");

    // --- salvataggio e caricamento ---
    Check(SaveGame(5), "salvataggio slot 5");
    int xp = P.xp, lvl = P.level, photos = (int)P.photos.size(), caught = P.timesCaught;
    float px = P.pos.x;
    Check(LoadGame(5), "caricamento slot 5");
    Check(G.player.xp == xp && G.player.level == lvl && (int)G.player.photos.size() == photos && G.player.timesCaught == caught && fabsf(G.player.pos.x - px) < 0.01f, "dati del giocatore conservati");
    Check(FindInterByTag("base_gate")->open && FindInterByTag("fusion_cell")->used && FindEntityByTag("mk5")->off, "stato del mondo conservato");
    Check(FindEntityByTag("ruggine")->companion && G.flags["raider_pass"], "compagno e lasciapassare conservati");
    Check(QUESTS[Q_MAIN].stage == 60 && QUESTS[Q_ROACH].done && QUESTS[Q_DOG].done, "missioni conservate");
    Check(QUESTS[Q_BEACON].done && QUESTS[Q_CONCORD].done && AdventureReadyForEnding(), "capitoli, indizi e accordi dell’Atlante conservati nel salvataggio");

    // --- svenimento: ci si risveglia in clinica ---
    int caps = P.inv.Count(I_CAPS);
    P.hp = 1;
    PlayerHurt(10, false);
    Check(G.state == GS_FAINT, "svenimento quando i PF finiscono");
    FaintWakeUp();
    FinishFade();
    Check(G.state == GS_PLAY && P.zone == ZONE_WASTE && DistXZ(P.pos, V3(22, 0, 38)) < 8, "risveglio nella clinica di Ferrocittà");
    Check(P.inv.Count(I_CAPS) == caps - caps / 10 && P.inv.Count(I_FUSIONCELL) == 1, "onorario pagato, la cella è ancora con te");

    // --- ritorno al Rifugio e finale ---
    Teleport(ZONE_VAULT, -16, 8);
    Interact(InterIdx("reactor"));
    Check(QUESTS[Q_MAIN].done && G.state == GS_ENDING, "reattore riparato, finale avviato");
    for (int i = 0; i < 8; i++) DrawFrame();
    G.state = GS_PLAY;
    DrawAllScreens();
    Check(true, "tutte le schermate disegnate senza errori");
    int done = 0;
    for (auto& q : QUESTS) done += q.done;
    Check(done == Q_COUNT, "tutte le quattordici missioni della nuova avventura completate");

    printf("   livello %d, PE %d, foto %d, volte scoperto %d, karma %d, tappi %d\n", P.level, P.xp, (int)P.photos.size(), P.timesCaught, P.karma, P.inv.Count(I_CAPS));
    // --- mondo aperto: si parte fuori, a Ferrocittà, con l'equipaggiamento ---
    NewGame(true);
    Check(!QUESTS[Q_BEACON].done && !AdventureReadyForEnding(), "una nuova partita azzera la rete e le scelte precedenti");
    Check(G.player.zone == ZONE_WASTE && DistXZ(G.player.pos, W.locations[1].travelPos) < 6, "mondo aperto: si parte alle porte di Ferrocittà");
    Check(G.player.inv.Count(I_CAMERA) == 1 && G.player.tool == TOOL_CAMERA && QUESTS[Q_MAIN].stage == 20, "mondo aperto: fotocamera in mano e missione principale avviata");
    Check(FindInterByTag("vault_door_in")->open && G.flags["vault_opened"], "mondo aperto: la porta del Rifugio è aperta");
    Sim(1.5f);
    Check(QUESTS[Q_MAIN].stage == 30, "mondo aperto: Ferrocittà scoperta, la storia prosegue");
    printf("=== %s (%d errori) ===\n", sFail ? "FALLITO" : "SUPERATO", sFail);
    return sFail ? 1 : 0;
}

// Opzioni da riga di comando per test automatici (screenshot)
struct TestOpts {
    std::string shot;
    int frames = 120;
    bool newgame = false;
    int zone = -1;
    float x = 0, z = 0, yaw = 0, pitch = 0, tod = -1;
    bool hasPos = false;
    int state = -1;
    bool third = false;
    int pipTab = 0, pipSub = 0;
    std::string give;
    std::string name;
    bool openWorld = false;
};

static TestOpts ParseArgs(int argc, char** argv) {
    TestOpts t;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](float def) { return i + 1 < argc ? (float)atof(argv[++i]) : def; };
        if (a == "--shot" && i + 1 < argc) t.shot = argv[++i];
        else if (a == "--frames") t.frames = (int)next(120);
        else if (a == "--newgame") t.newgame = true;
        else if (a == "--zone") t.zone = (int)next(0);
        else if (a == "--pos") { t.x = next(0); t.z = next(0); t.hasPos = true; }
        else if (a == "--yaw") t.yaw = next(0) * DEG2RAD;
        else if (a == "--pitch") t.pitch = next(0) * DEG2RAD;
        else if (a == "--time") t.tod = next(12);
        else if (a == "--state") t.state = (int)next(-1);
        else if (a == "--third") t.third = true;
        else if (a == "--pip") { t.pipTab = (int)next(0); t.pipSub = (int)next(0); }
        else if (a == "--give" && i + 1 < argc) t.give = argv[++i];
        else if (a == "--name" && i + 1 < argc) t.name = argv[++i];
        else if (a == "--openworld") { t.openWorld = true; t.newgame = true; }
    }
    return t;
}

static TestOpts sTest;
static bool sSelfTest = false;
static int sFrame = 0;

static void DrawLoading() {
    BeginDrawing();
    ClearBackground(Col(8, 7, 5));
    UIText("DUSTFALL", UIW() / 2, 300, 72, Col(255, 200, 110), 1);
    UIText("Generazione delle Terre Bruciate...", UIW() / 2, 400, 24, Col(230, 200, 150), 1);
    EndDrawing();
}

// dopo la generazione del mondo
static void StartGame() {
    AudioInit();
    ApplySettings();
    ResetDynamicWorld();
    G.player.zone = ZONE_WASTE;
    SetGameState(GS_MENU);
}

static void ApplyTestOpts() {
    TestOpts& test = sTest;
    if (test.newgame) {
        NewGame(test.openWorld);
        if (!test.name.empty()) G.player.name = test.name;
        if (!test.give.empty()) {
            // es. "--give all": equipaggiamento completo per i test
            int items[][2] = {{I_CAMERA, 1}, {I_BAIT, 10}, {I_NOISE, 10}, {I_MEDKIT, 10}, {I_STEALTHBOY, 2}, {I_REPAIRKIT, 3}, {I_PARTS, 5}, {I_CAPS, 500}, {I_MEAT, 3}, {I_DARKCLOAK, 1}};
            for (auto& it : items) G.player.inv.Add(it[0], it[1]);
            EquipTool(TOOL_CAMERA);
            G.player.photos = {"roach", "molerat", "deathclaw", "water_tower", "radio_tower", "atom_sign"};
            QUESTS[Q_MAIN].stage = 40;
            QUESTS[Q_ROACH].stage = 10;
            QUESTS[Q_PHOTO].stage = 10;
            for (auto& l : W.locations) l.discovered = true;
        }
        if (test.zone >= 0) {
            ChangeZone(test.zone, V3(test.x, 0, test.z), test.yaw);
        } else if (test.hasPos) ChangeZone(G.player.zone, V3(test.x, 0, test.z), test.yaw);
        if (test.zone >= 0 || test.hasPos) G.player.yaw = test.yaw;
        G.player.pitch = test.pitch;
        if (test.tod >= 0) G.timeOfDay = test.tod;
        G.player.thirdPerson = test.third;
        G.messages.clear();
        G.bigMsgT = 0;
        if (test.state >= 0) {
            if (test.state == GS_SCAN) ScanEnter();
            else if (test.state == GS_DIALOGUE) {
                for (int i = 0; i < (int)G.ents.size(); i++)
                    if (G.ents[i].zone == G.player.zone && !G.ents[i].dlg.empty() && Vector3Distance(G.ents[i].pos, G.player.pos) < 8) { DialogueStart(i); break; }
            } else if (test.state == GS_TERMINAL) {
                for (int i = 0; i < (int)G.inters.size(); i++)
                    if (G.inters[i].type == INT_TERMINAL && G.inters[i].zone == G.player.zone && Vector3Distance(G.inters[i].pos, G.player.pos) < 8) { G.inters[i].hackLevel = std::min(G.inters[i].hackLevel, 1); G.player.skillPts[SK_SCIENCE] = 60; TerminalStart(i); break; }
            } else if (test.state == GS_LOCKPICK) {
                G.player.inv.Add(I_BOBBYPIN, 5);
                for (int i = 0; i < (int)G.inters.size(); i++)
                    if (G.inters[i].locked && G.inters[i].lock < 5 && G.inters[i].zone == G.player.zone && Vector3Distance(G.inters[i].pos, G.player.pos) < 12) { G.player.skillPts[SK_LOCKPICK] = 80; LockpickStart(i); break; }
            } else SetGameState((GameState)test.state);
            G.pipTab = test.pipTab;
            G.pipSub = test.pipSub;
        }
        if (test.state == GS_LEVELUP) { G.player.pendingLevels = 1; G.player.level = 2; LevelUpOpen(); }
    }
    if (!test.shot.empty() && test.state == GS_MENU) { G.state = GS_MENU; }
}

// comandi touch da mostrare in ogni schermata
static int TouchModeFor(GameState s) {
    switch (s) {
        case GS_PLAY: return G.fade == 0 ? TM_PLAY : TM_NONE;
        case GS_SCAN: return TM_SCAN;
        case GS_LOCKPICK: return TM_LOCKPICK;
        case GS_PIPBOY: return TM_PIPBOY;
        case GS_CHAT: return TM_NONE;
        default: return TM_MENU;
    }
}

// un fotogramma di gioco; false quando il gioco deve terminare
static bool MainFrame() {
    PlatformFrame();
    float dt = std::min(GetFrameTime(), 0.05f);
    if (!sTest.shot.empty()) dt = 1.0f / 30.0f;
    UpdateFrame(dt);
    PlatformSetTouchMode(TouchModeFor(G.state));
    DrawFrame();
    sFrame++;
    if (!sTest.shot.empty() && sFrame >= sTest.frames) {
        TakeScreenshot(sTest.shot.c_str());
        return false;
    }
    return !G.quit;
}

#if DUSTFALL_WEB
// Nel browser il ciclo è guidato da requestAnimationFrame: il primo fotogramma mostra il
// caricamento, il secondo genera il mondo, poi si gioca.
static void WebFrame() {
    static int stage = 0;
    if (stage == 0) {
        DrawLoading();
        PlatformStatus("Generazione delle Terre Bruciate...");
        stage = 1;
        return;
    }
    if (stage == 1) {
        WorldBuild();
        StartGame();
        stage = 2;
        if (sSelfTest) {
            int rc = RunSelfTest();
            EM_ASM({ if (Module.dfTestDone) Module.dfTestDone($0); }, rc);
            emscripten_cancel_main_loop();
            return;
        }
        ApplyTestOpts();
        PlatformReady();
        return;
    }
    if (!MainFrame()) emscripten_cancel_main_loop();
}
#endif

int main(int argc, char** argv) {
    sTest = ParseArgs(argc, argv);
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--selftest") sSelfTest = true;
    PlatformInit();
    // sui telefoni niente MSAA: si disegna già ad alta densità
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | (PlatformTouch() ? 0 : FLAG_MSAA_4X_HINT));
#if DUSTFALL_WEB
    // la tela riempie la finestra del browser (raylib la ridimensiona da sé)
    InitWindow(EM_ASM_INT({ return window.innerWidth; }), EM_ASM_INT({ return window.innerHeight; }), "Dustfall - Cronache delle Terre Bruciate");
#else
    InitWindow(1280, 720, "Dustfall - Cronache delle Terre Bruciate");
    SetWindowMinSize(960, 540);
    Image icon = MakeIcon();
    SetWindowIcon(icon);
    UnloadImage(icon);
#endif
    SetExitKey(KEY_NULL);
    InitAudioDevice();
#if !DUSTFALL_WEB
    SetTargetFPS(144); // nel browser decide requestAnimationFrame
#endif
    rlSetClipPlanes(0.02, 900.0);

    ItemsInit();
    RenderInit();
    UIInit();
    LoadSettings();
    NetInit();

#if DUSTFALL_WEB
    emscripten_set_main_loop(WebFrame, 0, 1);
    return 0;
#else
    DrawLoading();
    WorldBuild();
    StartGame();

    if (sSelfTest) {
        int rc = RunSelfTest();
        CloseWindow();
        return rc;
    }
    ApplyTestOpts();

    while (!WindowShouldClose() && MainFrame()) {
    }

    SaveSettings();
    NetShutdown();
    AudioShutdown();
    UIShutdown();
    RenderShutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
#endif
}
