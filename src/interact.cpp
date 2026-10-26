// Oggetti interattivi: contenitori, porte, scale, macchinari da riparare, terminali...
#include "game.h"

Interactable* FindInterByTag(const std::string& tag) {
    for (auto& it : G.inters)
        if (it.tag == tag) return &it;
    return nullptr;
}

static int LockSkill(int lock) {
    static const int req[] = {0, 0, 25, 50, 75, 999};
    return req[Clamp(lock, 0, 5)];
}

bool TryUnlock(Interactable& it) {
    if (!it.locked) return true;
    if (it.keyItem >= 0 && G.player.inv.Count(it.keyItem) > 0) {
        it.locked = false;
        PlaySfx(SFX_UNLOCK);
        Notify(Fmt("Aperto con: %s", ITEMS[it.keyItem].name));
        return true;
    }
    if (it.lock >= 5) {
        Notify(it.keyItem >= 0 ? Fmt("Serve: %s", ITEMS[it.keyItem].name) : std::string("È bloccato. Serve una chiave."), Col(255, 180, 120));
        PlaySfx(SFX_UI_BACK);
        return false;
    }
    int need = LockSkill(it.lock);
    if (SkillValue(SK_LOCKPICK) < need) {
        Notify(Fmt("Serratura troppo difficile (Scasso %d richiesto).", need), Col(255, 180, 120));
        PlaySfx(SFX_UI_BACK);
        return false;
    }
    if (G.player.inv.Count(I_BOBBYPIN) <= 0) {
        Notify("Non hai forcine per scassinare.", Col(255, 180, 120));
        PlaySfx(SFX_UI_BACK);
        return false;
    }
    LockpickStart((int)(&it - &G.inters[0]));
    return false;
}

static void BeginTransition(Interactable& it) {
    G.pendingZone = it.targetZone;
    G.pendingPos = it.targetPos;
    G.pendingYaw = it.targetYaw;
    G.fade = 0.001f;
    PlaySfx(SFX_DOOR);
}

static void Climb(Interactable& it) {
    Player& P = G.player;
    Vector3 p = it.targetPos;
    p.y = GroundHeight(P.zone, V3(p.x, p.y + 0.6f, p.z), 0.8f);
    P.pos = p;
    P.vel = V3(0, 0, 0);
    P.yaw = it.targetYaw;
    P.onGround = true;
    P.fallStartY = p.y;
    P.crouch = false;
    G.fade = -0.7f; // breve dissolvenza in entrata
    PlaySfx(SFX_CLIMB);
}

static void Repair(Interactable& it) {
    Player& P = G.player;
    if (it.used) {
        Notify("Funziona già a meraviglia.");
        return;
    }
    if (SkillValue(SK_REPAIR) < it.skillReq) {
        Notify(Fmt("Troppo complicato: serve Riparazione %d (hai %d).", it.skillReq, SkillValue(SK_REPAIR)), Col(255, 180, 120));
        PlaySfx(SFX_UI_BACK);
        return;
    }
    int parts = std::max(0, it.partsReq - (P.perks[PK_MECHANIC] ? 1 : 0));
    if (P.inv.Count(I_REPAIRKIT) <= 0) {
        Notify("Serve un kit di riparazione.", Col(255, 180, 120));
        PlaySfx(SFX_UI_BACK);
        return;
    }
    if (P.inv.Count(I_PARTS) < parts) {
        Notify(Fmt("Servono %d componenti meccanici (ne hai %d).", parts, P.inv.Count(I_PARTS)), Col(255, 180, 120));
        PlaySfx(SFX_UI_BACK);
        return;
    }
    TakeItem(I_REPAIRKIT, 1);
    if (parts > 0) TakeItem(I_PARTS, parts);
    it.used = true;
    P.repairs++;
    PlaySfx(SFX_REPAIR);
    for (int i = 0; i < 14; i++)
        SpawnParticle(it.zone, it.pos + V3(0, it.size.y * 0.6f, 0), V3(gRng.range(-2, 2), gRng.range(0.5f, 3), gRng.range(-2, 2)), 0.4f, 0.04f, Col(255, 220, 130), 9.8f, true);
    BigNotify("RIPARATO", it.name);
    GiveXP(40 + it.skillReq, "Riparazione");
    QuestsOnRepair(it.tag);
}

void Interact(int idx) {
    if (idx < 0 || idx >= (int)G.inters.size()) return;
    if (AdventureInteract(idx)) return;
    Interactable& it = G.inters[idx];
    switch (it.type) {
        case INT_CONTAINER:
            if (it.locked && !TryUnlock(it)) return;
            LootOpen(idx);
            break;
        case INT_ITEM:
            if (it.used) return;
            for (auto& s : it.inv.items) GiveItem(s.id, s.count);
            it.inv.Clear();
            it.used = true;
            PlaySfx(SFX_PICKUP);
            break;
        case INT_TRANSITION:
            if (it.tag == "vault_door_out" && G.flags["vault_opened"] == 0) {
                Notify("La porta del Rifugio è sigillata.");
                return;
            }
            BeginTransition(it);
            break;
        case INT_VAULTDOOR:
            if (!it.open) {
                if (QUESTS[Q_MAIN].stage < 20) {
                    Notify("Prima parla con il Sovrintendente.", Col(255, 220, 140));
                    return;
                }
                it.open = true;
                G.flags["vault_opened"] = 1;
                PlaySfx(SFX_VAULTDOOR);
            }
            break;
        case INT_GATE:
            if (it.open) return;
            if (G.player.inv.Count(I_KEYCARD) > 0 || G.flags["gate_hacked"]) {
                it.open = true;
                it.locked = false;
                if (it.collider >= 0) W.zones[it.zone].boxes[it.collider].enabled = false;
                PlaySfx(SFX_VAULTDOOR, 0.8f, 1.2f);
                Notify("Il cancello si apre.");
                if (QUESTS[Q_MAIN].stage >= 30 && QUESTS[Q_MAIN].stage < 50) SetQuestStage(Q_MAIN, 50);
            } else {
                Notify("Serve una tessera di ricerca Delta-9. Oppure si può ripristinare l'accesso dal terminale...", Col(255, 180, 120));
                PlaySfx(SFX_UI_BACK);
            }
            break;
        case INT_DOOR:
            if (it.open) return;
            if (it.locked && !TryUnlock(it)) return;
            it.open = true;
            if (it.collider >= 0) W.zones[it.zone].boxes[it.collider].enabled = false;
            PlaySfx(SFX_DOOR);
            break;
        case INT_TERMINAL:
            TerminalStart(idx);
            break;
        case INT_BED:
            if (HostilesNear(40)) {
                Notify("Non puoi riposare mentre qualcuno ti cerca.", Col(255, 180, 120));
                return;
            }
            SleepOpen();
            break;
        case INT_LADDER:
            if (HostilesNear(6)) {
                Notify("Non c'è tempo per arrampicarsi!", Col(255, 180, 120));
                return;
            }
            Climb(it);
            break;
        case INT_REPAIR:
            Repair(it);
            break;
        case INT_REACTOR:
            if (G.player.inv.Count(I_FUSIONCELL) > 0 && QUESTS[Q_MAIN].stage >= 60 && QUESTS[Q_MAIN].stage < 100) {
                TakeItem(I_FUSIONCELL, 1);
                G.flags["reactor_fixed"] = 1;
                PlaySfx(SFX_VAULTDOOR, 1.0f, 0.7f);
                CompleteQuest(Q_MAIN);
                EndingStart();
            } else if (G.flags["reactor_fixed"]) {
                Notify("Il reattore ronza stabile. Il Rifugio è salvo.");
            } else {
                Notify("Il nucleo del reattore è crepato. Serve una nuova cella di fusione.", Col(255, 200, 120));
            }
            break;
    }
}

void InteractablesUpdate(float dt) {
    for (auto& it : G.inters) {
        float target = it.open ? 1.0f : 0.0f;
        float speed = it.type == INT_VAULTDOOR ? 0.25f : (it.type == INT_GATE ? 0.4f : 1.5f);
        if (it.anim < target) it.anim = std::min(target, it.anim + dt * speed);
        if (it.anim > target) it.anim = std::max(target, it.anim - dt * speed);
        if (it.type == INT_VAULTDOOR && it.collider >= 0) W.zones[it.zone].boxes[it.collider].enabled = it.anim < 0.85f;
    }
}

// ---------------------------------------------------------------------------
//  Disegno
// ---------------------------------------------------------------------------
static void DrawContainer(const Interactable& it) {
    Matrix base = MatrixRotateY(it.yaw) * MatrixTranslate(it.pos.x, it.pos.y, it.pos.z);
    Vector3 s = it.size;
    auto Bx = [&](Vector3 c, Vector3 sz, Color col, float em = 0) { DrawPrim(PR_CUBE, MatTRS(c, V3(0, 0, 0), sz) * base, col, em); };
    switch (it.model) {
        case CM_CRATE: Bx(V3(0, s.y / 2, 0), s, Col(118, 90, 62)); Bx(V3(0, s.y / 2, 0), V3(s.x * 1.02f, 0.08f, s.z * 1.02f), Col(90, 68, 46)); break;
        case CM_AMMOBOX: Bx(V3(0, s.y / 2, 0), s, Col(80, 92, 60)); Bx(V3(0, s.y, 0), V3(0.15f, 0.04f, 0.08f), Col(60, 60, 60)); break;
        case CM_FIRSTAID:
            Bx(V3(0, s.y / 2, 0), s, Col(220, 220, 215));
            Bx(V3(0, s.y / 2, s.z / 2 + 0.005f), V3(0.18f, 0.06f, 0.01f), Col(200, 30, 30));
            Bx(V3(0, s.y / 2, s.z / 2 + 0.005f), V3(0.06f, 0.18f, 0.01f), Col(200, 30, 30));
            break;
        case CM_FRIDGE: Bx(V3(0, s.y / 2, 0), s, Col(200, 196, 180)); Bx(V3(0.3f, s.y * 0.6f, s.z / 2 + 0.02f), V3(0.04f, 0.3f, 0.04f), Col(150, 150, 150)); break;
        case CM_DESK:
            Bx(V3(0, s.y - 0.04f, 0), V3(s.x, 0.08f, s.z), Col(105, 110, 100));
            Bx(V3(-s.x / 2 + 0.25f, s.y / 2 - 0.04f, 0), V3(0.45f, s.y - 0.08f, s.z * 0.95f), Col(95, 100, 92));
            Bx(V3(s.x / 2 - 0.05f, s.y / 2, 0), V3(0.06f, s.y, s.z * 0.95f), Col(95, 100, 92));
            break;
        case CM_LOCKER:
            Bx(V3(0, s.y / 2, 0), s, Col(90, 100, 110));
            for (int i = 0; i < 3; i++) Bx(V3(0, s.y * 0.8f - i * 0.05f, s.z / 2 + 0.005f), V3(s.x * 0.6f, 0.015f, 0.01f), Col(50, 55, 60));
            break;
        case CM_SAFE: Bx(V3(0, s.y / 2, 0), s, Col(70, 72, 78)); DrawPrim(PR_CYL, MatTRS(V3(0, s.y * 0.55f, s.z / 2 + 0.02f), V3(PI / 2, 0, 0), V3(0.18f, 0.04f, 0.18f)) * base, Col(160, 160, 160)); break;
        case CM_FOOTLOCKER: Bx(V3(0, s.y / 2, 0), s, Col(60, 80, 70)); Bx(V3(0, s.y, 0), V3(s.x * 1.02f, 0.05f, s.z * 1.02f), Col(140, 120, 60)); break;
        case CM_TOOLBOX: Bx(V3(0, s.y / 2, 0), s, Col(170, 40, 30)); Bx(V3(0, s.y + 0.04f, 0), V3(0.3f, 0.04f, 0.04f), Col(40, 40, 40)); break;
        case CM_TRASH: DrawPrim(PR_CYL, MatTRS(V3(0, s.y / 2, 0), V3(0, 0, 0), s) * base, Col(90, 95, 85)); break;
        case CM_NONE: {
            // zaino da viaggio abbandonato
            Bx(V3(0, 0.25f, 0), V3(0.5f, 0.5f, 0.3f), Col(110, 92, 60));
            Bx(V3(0, 0.52f, 0), V3(0.52f, 0.08f, 0.32f), Col(90, 74, 48));
            Bx(V3(0, 0.2f, 0.17f), V3(0.36f, 0.22f, 0.08f), Col(96, 80, 52));
            DrawPrim(PR_CYL, MatTRS(V3(0, 0.62f, 0), V3(0, 0, PI / 2), V3(0.14f, 0.5f, 0.14f)) * base, Col(70, 90, 60));
            Bx(V3(-0.15f, 0.3f, -0.16f), V3(0.05f, 0.45f, 0.02f), Col(60, 50, 36));
            Bx(V3(0.15f, 0.3f, -0.16f), V3(0.05f, 0.45f, 0.02f), Col(60, 50, 36));
            break;
        }
    }
}

static void DrawCogDoor(Vector3 c, float yaw, float open, float radius) {
    // porta a ingranaggio che si ritira e rotola di lato
    float slide = Clamp01((open - 0.25f) / 0.75f);
    float back = Clamp01(open / 0.25f);
    Vector3 dir = YawDir(yaw);
    Vector3 side = V3(cosf(yaw), 0, -sinf(yaw));
    Vector3 p = c - dir * (back * 1.0f) + side * (slide * radius * 2.15f);
    float rot = slide * 2.2f;
    Matrix m = MatrixRotateZ(rot) * MatrixRotateX(PI / 2) * MatrixRotateY(yaw) * MatrixTranslate(p.x, p.y, p.z);
    DrawPrim(PR_CYL, MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(radius * 2, 0.9f, radius * 2)) * m, Col(140, 142, 138));
    for (int i = 0; i < 10; i++) {
        float a = i / 10.0f * 2 * PI;
        DrawPrim(PR_CUBE, MatTRS(V3(cosf(a) * radius, 0, sinf(a) * radius), V3(0, -a, 0), V3(0.7f, 0.85f, 0.5f)) * m, Col(125, 127, 122));
    }
    DrawPrim(PR_CYL, MatTRS(V3(0, -0.46f, 0), V3(0, 0, 0), V3(radius * 1.2f, 0.05f, radius * 1.2f)) * m, Col(220, 180, 40));
    DrawPrim(PR_CYL, MatTRS(V3(0, 0.46f, 0), V3(0, 0, 0), V3(radius * 1.2f, 0.05f, radius * 1.2f)) * m, Col(220, 180, 40));
    DrawPrim(PR_CYL, MatTRS(V3(0, -0.5f, 0), V3(0, 0, 0), V3(radius * 0.5f, 0.06f, radius * 0.5f)) * m, Col(40, 70, 120));
    DrawPrim(PR_CYL, MatTRS(V3(0, 0.5f, 0), V3(0, 0, 0), V3(radius * 0.5f, 0.06f, radius * 0.5f)) * m, Col(40, 70, 120));
}

// macchinari guasti: lampeggiano in rosso e fanno scintille; riparati, si illuminano di verde
static void DrawMachine(const Interactable& it) {
    Matrix base = MatrixRotateY(it.yaw) * MatrixTranslate(it.pos.x, it.pos.y, it.pos.z);
    auto Bx = [&](Vector3 c, Vector3 sz, Color col, float em = 0) { DrawPrim(PR_CUBE, MatTRS(c, V3(0, 0, 0), sz) * base, col, em); };
    auto Cy = [&](Vector3 c, Vector3 rot, Vector3 sz, Color col, float em = 0) { DrawPrim(PR_CYL, MatTRS(c, rot, sz) * base, col, em); };
    bool ok = it.used;
    float blink = ok ? 0.9f : (fmodf(G.time, 1.0f) < 0.5f ? 1.0f : 0.15f);
    Color lamp = ok ? Col(90, 255, 120) : Col(255, 70, 50);
    if (it.tag == "pump") {
        // pompa a mano con serbatoio e abbeveratoio
        Cy(V3(0, 0.8f, 0), V3(0, 0, 0), V3(0.9f, 1.6f, 0.9f), Col(110, 120, 118));
        Cy(V3(0, 1.62f, 0), V3(0, 0, 0), V3(0.95f, 0.06f, 0.95f), Col(80, 86, 84));
        Bx(V3(0.62f, 1.1f, 0), V3(0.35f, 0.12f, 0.12f), Col(90, 95, 92));
        float lever = ok ? sinf(G.time * 2.5f) * 0.35f : 0.5f;
        DrawPrim(PR_CUBE, MatTRS(V3(0, 0.3f, 0), V3(0, 0, lever), V3(0.06f, 0.8f, 0.06f)) * MatrixTranslate(-0.6f, 1.6f, 0) * base, Col(70, 60, 50));
        Bx(V3(1.25f, 0.25f, 0), V3(0.9f, 0.5f, 0.6f), Col(100, 84, 62));
        Bx(V3(1.25f, 0.46f, 0), V3(0.8f, 0.04f, 0.5f), ok ? Col(90, 140, 170) : Col(70, 60, 50), ok ? 0.15f : 0);
        Bx(V3(0, 1.75f, 0.42f), V3(0.12f, 0.12f, 0.06f), lamp, blink);
        if (ok && gRng.chance(0.4f)) {
            Vector3 spout = Vector3Transform(V3(0.8f, 1.05f, 0), base);
            SpawnParticle(it.zone, spout, V3(gRng.range(-0.05f, 0.05f), -0.3f, gRng.range(-0.05f, 0.05f)), 0.5f, 0.05f, Col(150, 190, 220, 200), 9.8f, false);
        }
    } else if (it.tag == "transmitter") {
        // trasmettitore radio con quadranti
        Bx(V3(0, 0.6f, 0), V3(1.2f, 1.2f, 0.6f), Col(120, 118, 100));
        Bx(V3(0, 0.75f, 0.31f), V3(0.9f, 0.5f, 0.02f), Col(40, 44, 40));
        for (int i = 0; i < 3; i++) Cy(V3(-0.3f + i * 0.3f, 0.75f, 0.33f), V3(PI / 2, 0, 0), V3(0.14f, 0.02f, 0.14f), ok ? Col(220, 210, 140) : Col(120, 110, 90), ok ? 0.5f : 0);
        Bx(V3(0.42f, 1.3f, 0), V3(0.04f, 0.4f, 0.04f), Col(160, 160, 150));
        Bx(V3(-0.42f, 1.24f, 0.2f), V3(0.1f, 0.1f, 0.1f), lamp, blink);
        if (!ok && gRng.chance(0.03f)) {
            for (int i = 0; i < 5; i++) SpawnParticle(it.zone, Vector3Transform(V3(0, 1.1f, 0.3f), base), V3(gRng.range(-1.5f, 1.5f), gRng.range(0, 2), gRng.range(-1.5f, 1.5f)), 0.3f, 0.04f, Col(255, 210, 120), 9.8f, true);
            PlaySfx3D(SFX_SPARK, it.pos, 0.35f);
        }
    } else {
        // generatore
        Bx(V3(0, 0.5f, 0), V3(1.6f, 1.0f, 0.9f), Col(150, 120, 50));
        Cy(V3(0.3f, 0.5f, 0), V3(0, 0, PI / 2), V3(0.7f, 0.9f, 0.7f), Col(90, 90, 86));
        Cy(V3(-0.6f, 1.25f, 0), V3(0, 0, 0), V3(0.12f, 0.5f, 0.12f), Col(60, 60, 58));
        Bx(V3(-0.2f, 1.05f, 0.3f), V3(0.12f, 0.1f, 0.1f), lamp, blink);
        if (ok && gRng.chance(0.08f)) SpawnParticle(it.zone, Vector3Transform(V3(-0.6f, 1.55f, 0), base), V3(0, 1, 0), 1.5f, 0.15f, Col(80, 80, 80, 90), -0.1f, false, 0.5f);
    }
}

void InteractablesDraw(Camera3D cam) {
    int zone = G.player.zone;
    for (auto& it : G.inters) {
        if (it.zone != zone) continue;
        if (Vector3Distance(it.pos, cam.position) > 110) continue;
        switch (it.type) {
            case INT_CONTAINER: DrawContainer(it); break;
            case INT_ITEM: {
                if (it.used) break;
                int id = it.inv.Empty() ? -1 : it.inv.items[0].id;
                if (id == I_FUSIONCELL) {
                    float bob = sinf(G.time * 2) * 0.05f;
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.35f + bob, 0), V3(0, G.time, 0), V3(0.3f, 0.6f, 0.3f)), Col(120, 200, 255), 0.8f);
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.35f + bob, 0), V3(0, G.time, 0), V3(0.36f, 0.12f, 0.36f)), Col(150, 150, 150));
                } else if (id == I_ROTFOOD) {
                    // sacco di juta gonfio, con un ronzio di mosche
                    DrawPrim(PR_SPHERE, MatTRS(it.pos + V3(0, 0.3f, 0), V3(0, it.yaw, 0), V3(0.7f, 0.6f, 0.55f)), Col(120, 104, 70));
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.66f, 0), V3(0, 0, 0), V3(0.16f, 0.18f, 0.16f)), Col(100, 86, 58));
                    if (gRng.chance(0.15f)) SpawnParticle(it.zone, it.pos + V3(gRng.range(-0.3f, 0.3f), 0.7f, gRng.range(-0.3f, 0.3f)), V3(gRng.range(-0.3f, 0.3f), 0.2f, gRng.range(-0.3f, 0.3f)), 1.4f, 0.12f, Col(140, 150, 90, 60), -0.05f, false, 0.2f);
                } else if (id == I_RECORD) {
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.02f, 0), V3(0, 0, 0), V3(0.3f, 0.01f, 0.3f)), Col(20, 20, 22));
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.025f, 0), V3(0, 0, 0), V3(0.1f, 0.012f, 0.1f)), Col(200, 60, 50));
                } else if (id == I_PARTS || id == I_REPAIRKIT) {
                    DrawPrim(PR_CUBE, MatTRS(it.pos + V3(0, 0.1f, 0), V3(0, it.yaw, 0), V3(0.4f, 0.2f, 0.25f)), Col(170, 40, 30));
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0.25f, 0.06f, 0.1f), V3(PI / 2, 0, 0), V3(0.12f, 0.04f, 0.12f)), Col(150, 150, 140));
                } else DrawPrim(PR_CUBE, MatTRS(it.pos + V3(0, 0.12f, 0), V3(0, it.yaw, 0), V3(0.3f, 0.2f, 0.3f)), Col(160, 150, 120));
                break;
            }
            case INT_TERMINAL: {
                Matrix base = MatrixRotateY(it.yaw) * MatrixTranslate(it.pos.x, it.pos.y, it.pos.z);
                DrawPrim(PR_CUBE, MatTRS(V3(0, 0, 0), V3(0, 0, 0), V3(0.5f, 0.42f, 0.45f)) * base, Col(150, 140, 110));
                Color sc = it.lockedOut ? Col(255, 60, 40) : Col(60, 255, 120);
                DrawPrim(PR_CUBE, MatTRS(V3(0, 0.02f, 0.226f), V3(0, 0, 0), V3(0.36f, 0.28f, 0.01f)) * base, sc, 0.75f + 0.1f * sinf(G.time * 8));
                DrawPrim(PR_CUBE, MatTRS(V3(0, -0.25f, 0.2f), V3(0.3f, 0, 0), V3(0.45f, 0.05f, 0.2f)) * base, Col(120, 112, 90));
                break;
            }
            case INT_VAULTDOOR: DrawCogDoor(it.pos, it.yaw, it.anim, 3.25f); break;
            case INT_TRANSITION:
                if (it.tag == "vault_door_out") DrawCogDoor(it.pos + V3(0, 3.6f, 0.3f), PI, G.flags["vault_opened"] ? 1.0f : 0.0f, 3.25f);
                break;
            case INT_GATE: {
                float slide = it.anim * 8.2f;
                Vector3 c = it.pos + V3(0, 2.6f, -slide);
                DrawPrim(PR_CUBE, MatTRS(c, V3(0, 0, 0), V3(0.35f, 5.2f, 8.0f)), Col(90, 96, 84));
                for (int i = 0; i < 6; i++) DrawPrim(PR_CUBE, MatTRS(c + V3(-0.2f, 0, -3.5f + i * 1.4f), V3(0, 0, 0), V3(0.1f, 5.0f, 0.15f)), Col(70, 75, 66));
                for (int i = 0; i < 8; i++) DrawPrim(PR_CUBE, MatTRS(c + V3(-0.2f, 2.3f, -3.5f + i * 1.0f), V3(0, 0, 0), V3(0.12f, 0.4f, 0.45f)), i & 1 ? Col(220, 180, 40) : Col(30, 30, 30));
                break;
            }
            case INT_DOOR: {
                float up = it.anim * 2.9f;
                Vector3 c = it.pos + V3(0, up, 0);
                DrawPrim(PR_CUBE, MatTRS(c, V3(0, it.yaw, 0), V3(3.0f, 3.0f, 0.3f)), Col(100, 104, 98));
                DrawPrim(PR_CUBE, MatTRS(c + V3(0, 0.6f, 0), V3(0, it.yaw, 0), V3(2.6f, 0.25f, 0.35f)), Col(220, 180, 40));
                DrawPrim(PR_CUBE, MatTRS(c + V3(0, 0.0f, 0), V3(0, it.yaw, 0), V3(0.2f, 0.2f, 0.4f)), it.locked ? Col(255, 50, 40) : Col(60, 255, 100), 0.9f);
                break;
            }
            case INT_REPAIR: DrawMachine(it); break;
            case INT_REACTOR: {
                bool fixed = G.flags["reactor_fixed"] != 0;
                float pulse = fixed ? 0.75f + 0.1f * sinf(G.time * 3) : 0.3f + 0.3f * Clamp01(sinf(G.time * 7) * sinf(G.time * 2.3f) + 0.3f);
                Color core = fixed ? Col(110, 190, 255) : Col(255, 110, 50);
                DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.5f, 0), V3(0, 0, 0), V3(5.4f, 1.0f, 5.4f)), Col(80, 82, 86));
                DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 3.5f, 0), V3(0, 0, 0), V3(3.2f, 5.0f, 3.2f)), core, pulse);
                for (int i = 0; i < 6; i++) {
                    float a = i / 6.0f * 2 * PI;
                    DrawPrim(PR_CUBE, MatTRS(it.pos + V3(sinf(a) * 1.75f, 3.5f, cosf(a) * 1.75f), V3(0, a, 0), V3(0.3f, 5.2f, 0.3f)), Col(110, 112, 116));
                }
                for (int k = 0; k < 3; k++) DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 1.6f + k * 1.8f, 0), V3(0, 0, 0), V3(3.8f, 0.25f, 3.8f)), Col(120, 122, 126));
                DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 6.4f, 0), V3(0, 0, 0), V3(4.6f, 0.8f, 4.6f)), Col(90, 92, 96));
                if (!fixed && gRng.chance(0.05f)) {
                    for (int i = 0; i < 5; i++) SpawnParticle(it.zone, it.pos + V3(gRng.range(-1.6f, 1.6f), gRng.range(1, 6), gRng.range(-1.6f, 1.6f)), V3(gRng.range(-2, 2), gRng.range(0, 2), gRng.range(-2, 2)), 0.3f, 0.05f, Col(255, 200, 120), 9.8f, true);
                    PlaySfx3D(SFX_SPARK, it.pos, 0.4f);
                }
                break;
            }
            default: break;
        }
    }
}
