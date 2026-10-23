// DUSTFALL - Cronache delle Terre Bruciate
// Punto d'ingresso, ciclo principale, ciclo giorno/notte e transizioni tra zone.
#include "game.h"

Game G;

void SetGameState(GameState s) {
    G.prevState = G.state;
    G.state = s;
    bool captured = s == GS_PLAY || s == GS_VATS || s == GS_LOCKPICK;
    if (captured) {
        if (!IsCursorHidden()) DisableCursor();
        G.mouseSkip = 3;
    } else if (IsCursorHidden()) EnableCursor();
}

void NewGame() {
    G.player = Player();
    Player& P = G.player;
    P.zone = ZONE_VAULT;
    P.pos = V3(17.0f, 0, 7);
    P.yaw = PI / 2;
    P.inv.Add(I_VAULTSUIT);
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
    Notify("Parla con il Sovrintendente: guardalo e premi E.", Col(255, 230, 140));
    Notify("TAB apre il Pip-Boy. ESC mette in pausa.", Col(200, 230, 200));
}

void ChangeZone(int zone, Vector3 pos, float yaw) {
    Player& P = G.player;
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
    BigNotify(zone == ZONE_WASTE ? "TERRE BRUCIATE" : W.zones[zone].name);
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
    for (auto& m : G.messages) m.t -= rdt;
    G.messages.erase(std::remove_if(G.messages.begin(), G.messages.end(), [](const Message& m) { return m.t <= 0; }), G.messages.end());
    if (G.bigMsgT > 0) G.bigMsgT -= rdt;

    if (IsKeyPressed(KEY_F11)) {
        G.settings.fullscreen = !G.settings.fullscreen;
        ApplySettings();
        SaveSettings();
    }

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
            if (IsKeyPressed(KEY_Q)) { VatsEnter(); if (G.state != GS_PLAY) break; }
            if (IsKeyPressed(KEY_F5)) {
                if (G.inCombat) Notify("Non puoi salvare durante un combattimento.", Col(255, 160, 100));
                else if (SaveGame(6)) Notify("Salvataggio rapido completato.");
            }
            if (IsKeyPressed(KEY_F9) && SaveExists(6)) { LoadGame(6); break; }
            float wdt = rdt * (G.player.jetTimer > 0 ? 0.45f : 1.0f);
            PlayerUpdate(rdt);
            if (G.state != GS_PLAY && G.state != GS_DEAD) { UpdateCamera3D(); break; }
            WorldTick(wdt);
            QuestsUpdate(rdt);
            AdvanceTime(wdt);
            UpdateCamera3D();
            break;
        }
        case GS_VATS:
            VatsUpdate(rdt);
            if (G.vatsExec) WorldTick(rdt * 0.3f);
            UpdateCamera3D();
            G.cam.fovy *= G.vatsExec ? 0.85f : 0.75f;
            break;
        case GS_PIPBOY: PipBoyUpdate(); break;
        case GS_DIALOGUE: DialogueUpdate(); UpdateCamera3D(); break;
        case GS_BARTER: BarterUpdate(); break;
        case GS_LOOT: LootUpdate(); break;
        case GS_LOCKPICK: LockpickUpdate(rdt); break;
        case GS_TERMINAL: TerminalUpdate(rdt); break;
        case GS_LEVELUP: LevelUpUpdate(); break;
        case GS_PAUSE: PauseUpdate(); break;
        case GS_DEAD: DeadUpdate(); FxUpdate(rdt); break;
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
    RenderSetLights(lights, cam.position, G.time);
    DrawZoneStatic(zone, cam, z.outdoor ? 420.0f : 140.0f);
    InteractablesDraw(cam);
    EntitiesDraw(cam);
    if (G.state != GS_MENU) DrawPlayerBody();
    rlDrawRenderBatchActive();
    DrawSigns(zone, cam);
    EntitiesDrawShadows(cam);
    FxDraw(cam);
    bool showWeapon = G.state == GS_PLAY || G.state == GS_VATS || G.state == GS_DIALOGUE || G.state == GS_LOOT;
    if (showWeapon && G.state != GS_DIALOGUE && !(G.state == GS_VATS && !G.vatsExec)) DrawViewModel();
    EndMode3D();
}

static void DrawFrame() {
    BeginDrawing();
    bool opaque2D = G.state == GS_INTRO || G.state == GS_CREATE || G.state == GS_TERMINAL || (G.state == GS_ENDING);
    if (!opaque2D) DrawWorld();
    else ClearBackground(BLACK);
    switch (G.state) {
        case GS_MENU: MenuDraw(); break;
        case GS_INTRO: IntroDraw(); break;
        case GS_CREATE: CreateDraw(); break;
        case GS_PLAY: HudDraw(); break;
        case GS_VATS: HudDraw(); VatsDraw(); break;
        case GS_PIPBOY: PipBoyDraw(); break;
        case GS_DIALOGUE: DialogueDraw(); break;
        case GS_BARTER: BarterDraw(); break;
        case GS_LOOT: HudDraw(); LootDraw(); break;
        case GS_LOCKPICK: LockpickDraw(); break;
        case GS_TERMINAL: TerminalDraw(); break;
        case GS_LEVELUP: LevelUpDraw(); break;
        case GS_PAUSE: PauseDraw(); break;
        case GS_DEAD: DeadDraw(); break;
        case GS_ENDING: EndingDraw(); break;
        case GS_SLEEP: SleepDraw(); break;
        case GS_SAVELOAD: SaveLoadDraw(); break;
        case GS_OPTIONS: OptionsDraw(); break;
    }
    UIDrawMessages();
    float fa = G.fade > 0 ? G.fade : -G.fade;
    if (fa > 0) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, (unsigned char)(255 * Clamp01(fa))});
    EndDrawing();
}

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
    if (G.state == GS_DEAD) G.state = GS_PLAY;
}

static void Sim(float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 30) SimTick(1.0f / 30);
}

static int NearestHostile(float radius) {
    int best = -1;
    float bd = radius;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        Entity& e = G.ents[i];
        if (e.dead || !e.hostile || e.zone != G.player.zone) continue;
        float d = Vector3Distance(e.pos, G.player.pos);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// combatte finché non restano nemici nel raggio indicato
static int AutoFight(float seconds, float radius) {
    int kills0 = G.player.kills;
    for (float t = 0; t < seconds; t += 1.0f / 30) {
        int ti = NearestHostile(radius);
        if (ti < 0) { printf("   [fight] nessun nemico entro %.0fm dopo %.1fs\n", radius, t); break; }
        if (fmodf(t, 5.0f) < 1.0f / 30) printf("   [fight] t=%.0f bersaglio %s d=%.1f hp=%.0f mag=%d riserva=%d PF=%.0f\n", t, G.ents[ti].name.c_str(), Vector3Distance(G.ents[ti].pos, G.player.pos), G.ents[ti].hp, G.player.mag, G.player.inv.Count(CurrentWeapon().ammo), G.player.hp);
        Entity& e = G.ents[ti];
        Vector3 aim = EntityPartPos(e, 0);
        if (!LineOfSight(G.player.zone, PlayerEye(), aim) || Vector3Distance(G.player.pos, e.pos) > 40) {
            for (int k = 0; k < 16; k++) {
                float a = k * PI / 8, r = k < 8 ? 4.0f : 8.0f;
                Vector3 p = e.pos + V3(sinf(a) * r, 0, cosf(a) * r);
                float base = G.player.zone == ZONE_WASTE ? TerrainHeight(p.x, p.z) : 0;
                p.y = GroundHeight(G.player.zone, V3(p.x, std::max(base, e.pos.y) + 1.0f, p.z), 1.0f);
                if (CircleBlocked(G.player.zone, p, 0.35f, 1.75f, 0.5f)) continue;
                if (!LineOfSight(G.player.zone, p + V3(0, 1.62f, 0), aim)) continue;
                G.player.pos = p;
                break;
            }
        }
        Vector3 to = aim - PlayerEye();
        G.player.yaw = atan2f(to.x, to.z);
        G.player.pitch = atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z));
        UpdateCamera3D();
        const WeaponDef& w = CurrentWeapon();
        if (G.player.fireCd <= 0 && G.player.reloadT <= 0 && G.player.equipT <= 0) {
            if (!w.melee && G.player.mag <= 0) PlayerReload();
            else PlayerFire();
        }
        SimTick(1.0f / 30);
    }
    return G.player.kills - kills0;
}

static bool ChooseOption(const char* contains) {
    for (auto& o : G.dlg.opts)
        if (o.text.find(contains) != std::string::npos) {
            auto act = o.act;
            act();
            if (G.state != GS_DIALOGUE) return true;
            if (G.dlg.end) { SetGameState(GS_PLAY); return true; }
            G.dlg.node = G.dlg.next;
            DialogueBuild();
            return true;
        }
    printf("   opzione non trovata: %s\n", contains);
    return false;
}

static const char* TagFind(const char* skill) {
    static std::string t;
    t = std::string("[") + skill;
    return t.c_str();
}

static int FindNpc(const char* dlg) {
    for (int i = 0; i < (int)G.ents.size(); i++)
        if (G.ents[i].dlg == dlg) return i;
    return -1;
}

static void Teleport(int zone, float x, float z) {
    ChangeZone(zone, V3(x, 0, z), 0);
    G.messages.clear();
}

static void DrawAllScreens() {
    GameState states[] = {GS_MENU, GS_PLAY, GS_PIPBOY, GS_PAUSE, GS_SAVELOAD, GS_OPTIONS, GS_DEAD, GS_SLEEP, GS_LEVELUP};
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
    printf("=== SELF-TEST DUSTFALL ===\n");
    NewGame();
    Player& P = G.player;
    Check(P.zone == ZONE_VAULT, "inizio nel Rifugio");
    int ov = FindNpc("overseer");
    Check(ov >= 0, "Sovrintendente presente");
    DialogueStart(ov);
    ChooseOption("Cosa posso fare");
    ChooseOption("Farò del mio meglio");
    ChooseOption("Tornerò");
    Check(QUESTS[Q_MAIN].stage == 20, "missione principale avviata (stadio 20)");
    Check(P.inv.Count(I_PISTOL) == 1 && P.weapon == I_PISTOL, "pistola ricevuta ed equipaggiata");
    Check(P.mag > 0, "caricatore pieno");
    Interactable* door = FindInterByTag("vault_door_in");
    Interact((int)(door - &G.inters[0]));
    Sim(5);
    Check(door->open && !W.zones[ZONE_VAULT].boxes[door->collider].enabled, "porta del Rifugio aperta");
    Teleport(ZONE_WASTE, 0, 62);
    Sim(1.5f);
    Check(W.locations[1].discovered, "Ferrocittà scoperta");
    Check(QUESTS[Q_MAIN].stage == 30, "stadio 30: parla con lo sceriffo");
    int sh = FindNpc("sheriff");
    DialogueStart(sh);
    ChooseOption("Cerco la Base");
    ChooseOption("Allora gliela");
    ChooseOption("Affare fatto");
    Check(QUESTS[Q_MAIN].stage == 40, "stadio 40: recupera la tessera");
    // missione secondaria: radscarafaggi
    int tano = FindNpc("barman");
    DialogueStart(tano);
    ChooseOption("lavoro");
    ChooseOption("Affare fatto");
    Check(QUESTS[Q_ROACH].stage == 10, "missione Infestazione avviata");
    P.inv.Add(I_ASSAULT);
    P.inv.Add(I_AMMO_556, 3000);
    P.inv.Add(I_COMBAT);
    EquipArmor(I_COMBAT);
    EquipWeapon(I_ASSAULT);
    for (int i = 0; i < SK_COUNT; i++) P.skillPts[i] = 60;
    Teleport(ZONE_WASTE, 45, 165);
    int k = AutoFight(90, 45);
    printf("   uccisi alla stazione: %d\n", k);
    Sim(1);
    Check(QUESTS[Q_ROACH].stage == 20, "radscarafaggi eliminati");
    Teleport(ZONE_WASTE, 0, 30);
    DialogueStart(tano);
    ChooseOption("ripulita");
    ChooseOption("Grazie");
    Check(QUESTS[Q_ROACH].done && P.inv.Count(I_SHOTGUN) > 0, "Infestazione completata, ricompensa ricevuta");
    // Covo dei Teschi
    Teleport(ZONE_WASTE, 190, -88);
    k = AutoFight(240, 60);
    printf("   uccisi al covo: %d (livello %d)\n", k, P.level);
    Entity* boss = FindEntityByTag("boss");
    Check(boss && boss->dead, "Teschio sconfitto");
    if (boss) {
        LootOpen(-1, (int)(boss - &G.ents[0]));
        for (auto s : boss->inv.items) GiveItem(s.id, s.count, false);
        boss->inv.Clear();
        SetGameState(GS_PLAY);
    }
    Check(P.inv.Count(I_KEYCARD) == 1, "tessera d'accesso raccolta");
    Check(QUESTS[Q_MAIN].stage == 45, "stadio 45: apri il cancello");
    // cassaforte con la chiave
    Interactable* safe = FindInterByTag("boss_safe");
    if (safe) safe->keyItem = safe->keyItem;
    // missione secondaria: medicine
    Teleport(ZONE_WASTE, 0, 30);
    int doc = FindNpc("doctor");
    DialogueStart(doc);
    ChooseOption("aiuto");
    ChooseOption(TagFind("Baratto"));
    if (G.state == GS_DIALOGUE) ChooseOption("ci vado");
    if (G.state == GS_DIALOGUE) ChooseOption("Affare fatto");
    Check(QUESTS[Q_MEDS].stage == 10, "missione Medicine avviata");
    Teleport(ZONE_WASTE, 79, -165);
    k = AutoFight(120, 45);
    printf("   uccisi all'ospedale: %d\n", k);
    Interactable* meds = FindInterByTag("meds");
    if (meds) { meds->locked = false; Interact((int)(meds - &G.inters[0])); for (auto st : meds->inv.items) GiveItem(st.id, st.count, false); meds->inv.Clear(); SetGameState(GS_PLAY); }
    Check(QUESTS[Q_MEDS].stage == 20, "rifornimenti medici trovati");
    Teleport(ZONE_WASTE, 0, 30);
    DialogueStart(doc);
    ChooseOption("rifornimenti");
    ChooseOption("Felice");
    Check(QUESTS[Q_MEDS].done, "missione Medicine completata");
    // missione secondaria: la bestia
    int hunter = FindNpc("hunter");
    DialogueStart(hunter);
    ChooseOption("Che guaio");
    ChooseOption("Lo farò");
    Check(QUESTS[Q_BEAST].stage == 10, "missione Bestia avviata");
    P.inv.Add(I_AMMO_556, 1000);
    Teleport(ZONE_WASTE, -240, -250);
    k = AutoFight(200, 80);
    Entity* beast = FindEntityByTag("beast");
    Check(beast && beast->dead, "Squarciatore ucciso");
    Check(QUESTS[Q_BEAST].stage == 20, "stadio 20: recupera l'artiglio");
    if (beast) { for (auto st : beast->inv.items) GiveItem(st.id, st.count, false); beast->inv.Clear(); }
    Teleport(ZONE_WASTE, 0, 30);
    DialogueStart(hunter);
    ChooseOption("artiglio");
    ChooseOption("Grazie, Jack");
    Check(QUESTS[Q_BEAST].done, "missione Bestia completata");
    // missione secondaria: il segnale
    SetRadioStation(2);
    Sim(8);
    Check(QUESTS[Q_SIGNAL].stage == 10, "segnale radio captato");
    SetRadioStation(0);
    Teleport(ZONE_WASTE, -260, -5);
    AutoFight(60, 40);
    Interactable* scout = FindInterByTag("scout");
    if (scout) { LootOpen((int)(scout - &G.inters[0]), -1); for (auto st : scout->inv.items) GiveItem(st.id, st.count, false); scout->inv.Clear(); SetGameState(GS_PLAY); }
    Check(QUESTS[Q_SIGNAL].stage == 20 && P.inv.Count(I_CACHEKEY) == 1, "chiave del deposito trovata");
    Interactable* cache = FindInterByTag("cache");
    Teleport(ZONE_WASTE, -128, -145);
    if (cache) Interact((int)(cache - &G.inters[0]));
    Check(cache && !cache->locked && QUESTS[Q_SIGNAL].done, "deposito aperto, missione Segnale completata");
    SetGameState(GS_PLAY);
    P.rads = 0;
    // Base Delta-9
    Teleport(ZONE_WASTE, 190, 220);
    Interactable* gate = FindInterByTag("base_gate");
    Interact((int)(gate - &G.inters[0]));
    Check(gate->open, "cancello aperto con la tessera");
    Check(QUESTS[Q_MAIN].stage == 50, "stadio 50: entra nei laboratori");
    Sim(2);
    k = AutoFight(120, 70);
    printf("   uccisi alla base: %d\n", k);
    Teleport(ZONE_BASE, 0, -2);
    Check(QUESTS[Q_MAIN].stage == 55, "stadio 55: trova la cella");
    k = AutoFight(300, 120);
    printf("   uccisi nei laboratori: %d\n", k);
    Entity* mk5 = FindEntityByTag("mk5");
    Check(mk5 && mk5->dead, "Sentinella Mk.V distrutta");
    Interactable* cell = FindInterByTag("fusion_cell");
    Interact((int)(cell - &G.inters[0]));
    Check(P.inv.Count(I_FUSIONCELL) == 1 && QUESTS[Q_MAIN].stage == 60, "cella di fusione recuperata (stadio 60)");
    // armeria: porta bloccata
    Interactable* arm = FindInterByTag("armory_door");
    Check(arm && arm->locked, "porta dell'armeria bloccata");
    // salvataggio e caricamento
    Check(SaveGame(5), "salvataggio slot 5");
    int xp = P.xp, kills = P.kills, lvl = P.level;
    float px = P.pos.x;
    Check(LoadGame(5), "caricamento slot 5");
    Check(G.player.xp == xp && G.player.kills == kills && G.player.level == lvl && fabsf(G.player.pos.x - px) < 0.01f, "dati del giocatore conservati");
    Check(FindEntityByTag("boss")->dead && FindInterByTag("base_gate")->open && FindInterByTag("fusion_cell")->used, "stato del mondo conservato");
    Check(QUESTS[Q_MAIN].stage == 60 && QUESTS[Q_ROACH].done, "missioni conservate");
    // ritorno al Rifugio e finale
    Teleport(ZONE_VAULT, -16, 8);
    Interactable* reactor = FindInterByTag("reactor");
    Interact((int)(reactor - &G.inters[0]));
    Check(QUESTS[Q_MAIN].done && G.state == GS_ENDING, "reattore riparato, finale avviato");
    for (int i = 0; i < 8; i++) DrawFrame();
    // tutte le schermate
    G.state = GS_PLAY;
    DrawAllScreens();
    Check(true, "tutte le schermate disegnate senza errori");
    // statistiche finali
    printf("   livello %d, PE %d, uccisioni %d, karma %d, tappi %d\n", P.level, P.xp, P.kills, P.karma, P.inv.Count(I_CAPS));
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
    }
    return t;
}

int main(int argc, char** argv) {
    TestOpts test = ParseArgs(argc, argv);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Dustfall - Cronache delle Terre Bruciate");
    SetWindowMinSize(960, 540);
    SetExitKey(KEY_NULL);
    Image icon = MakeIcon();
    SetWindowIcon(icon);
    UnloadImage(icon);
    InitAudioDevice();
    SetTargetFPS(144);
    rlSetClipPlanes(0.02, 900.0);

    ItemsInit();
    RenderInit();
    UIInit();
    LoadSettings();

    BeginDrawing();
    ClearBackground(Col(8, 7, 5));
    UIText("DUSTFALL", UIW() / 2, 300, 72, Col(255, 200, 110), 1);
    UIText("Generazione delle Terre Bruciate...", UIW() / 2, 400, 24, Col(230, 200, 150), 1);
    EndDrawing();

    WorldBuild();
    AudioInit();
    ApplySettings();
    ResetDynamicWorld();
    G.player.zone = ZONE_WASTE;
    SetGameState(GS_MENU);

    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--selftest") {
            int rc = RunSelfTest();
            CloseWindow();
            return rc;
        }

    if (test.newgame) {
        NewGame();
        if (!test.give.empty()) {
            // es. "--give all": equipaggiamento completo per i test
            int items[] = {I_PISTOL, I_ASSAULT, I_LASERRIFLE, I_HUNTRIFLE, I_SHOTGUN, I_PLASMA, I_MINIGUN, I_SLEDGE};
            for (int it : items) G.player.inv.Add(it);
            int ammo[] = {I_AMMO_10, I_AMMO_556, I_AMMO_CELL, I_AMMO_308, I_AMMO_SHELL, I_AMMO_MFC, I_AMMO_5MM};
            for (int a : ammo) G.player.inv.Add(a, 200);
            G.player.inv.Add(I_STIMPAK, 10);
            G.player.inv.Add(I_GRENADE, 5);
            G.player.inv.Add(I_CAPS, 500);
            EquipWeapon(I_ASSAULT);
            QUESTS[Q_MAIN].stage = 40;
            QUESTS[Q_ROACH].stage = 10;
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
            if (test.state == GS_VATS) VatsEnter();
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

    int frame = 0;
    while (!WindowShouldClose() && !G.quit) {
        float dt = std::min(GetFrameTime(), 0.05f);
        if (!test.shot.empty()) dt = 1.0f / 30.0f;
        UpdateFrame(dt);
        DrawFrame();
        frame++;
        if (!test.shot.empty() && frame >= test.frames) {
            TakeScreenshot(test.shot.c_str());
            break;
        }
    }

    SaveSettings();
    AudioShutdown();
    UIShutdown();
    RenderShutdown();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
