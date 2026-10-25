// Giocatore: statistiche, movimento, attrezzi (fotocamera, esche, barattoli), furtività e visuale.
#include "game.h"
#include "net.h"
#include "platform.h"

// ---------------------------------------------------------------------------
//  Statistiche
// ---------------------------------------------------------------------------
int Spec(int st) {
    int v = G.player.special[st];
    if (G.player.teaTimer > 0 && (st == ST_INT || st == ST_PER)) v += 2;
    return std::min(v, 12);
}

int SkillValue(Skill s) {
    const Player& P = G.player;
    int v = 2 + Spec(SKILL_GOV[s]) * 2 + (Spec(ST_LCK) + 1) / 2;
    if (P.tagged[s]) v += 15;
    v += P.skillPts[s];
    const ItemDef* wear = P.armor >= 0 ? &ITEMS[P.armor] : nullptr;
    if (s == SK_SNEAK) {
        if (P.perks[PK_SOFTSTEP]) v += 15;
        if (wear) v += wear->sneak;
    }
    if (s == SK_REPAIR && P.perks[PK_MECHANIC]) v += 15;
    if (s == SK_SPEECH && P.perks[PK_SILVERTONGUE]) v += 15;
    if (s == SK_BARTER && P.perks[PK_MERCHANT]) v += 15;
    if ((s == SK_SPEECH || s == SK_BARTER) && wear) v += wear->talk;
    return Clamp(v, 0, 100);
}

float MaxHP() {
    const Player& P = G.player;
    return 80.0f + P.special[ST_END] * 15.0f + (P.level - 1) * 10.0f + (P.perks[PK_LIFEGIVER] ? 30.0f : 0.0f);
}
float HPCap() { return std::max(1.0f, MaxHP() * (1.0f - Clamp01(G.player.rads / 1000.0f))); }
float MaxAP() { return 65.0f + G.player.special[ST_AGI] * 3.0f + (G.player.perks[PK_ACTIONBOY] ? 25.0f : 0.0f); }
float CarryMax() {
    const Player& P = G.player;
    float c = 150.0f + P.special[ST_STR] * 10.0f + (P.perks[PK_STRONGBACK] ? 50.0f : 0.0f);
    if (P.armor >= 0) c += ITEMS[P.armor].carry;
    return c;
}
float PlayerRadRes() {
    const Player& P = G.player;
    float r = P.special[ST_END] * 0.02f + SkillValue(SK_SURVIVAL) / 400.0f;
    if (P.armor >= 0) r += ITEMS[P.armor].radRes;
    if (P.perks[PK_RADRES]) r += 0.25f;
    if (P.radXTimer > 0) r += 0.5f;
    return Clamp(r, 0.0f, 0.9f);
}
int XPForLevel(int lvl) { return 75 * (lvl - 1) * lvl; }
int ArmorOutfit() { return G.player.armor >= 0 ? ITEMS[G.player.armor].look : OF_SETTLER; }

float PlayerSpeedMul() {
    float m = 1.0f;
    if (G.player.perks[PK_EXPLORER]) m *= 1.1f;
    if (G.player.inv.Weight() > CarryMax()) m *= 0.5f;
    return m;
}

bool HasPhoto(const std::string& key) {
    for (auto& p : G.player.photos)
        if (p == key) return true;
    return false;
}

void GiveXP(int amount, const char* reason) {
    Player& P = G.player;
    P.xp += amount;
    Notify(reason ? Fmt("+%d PE  %s", amount, reason) : Fmt("+%d PE", amount), Col(200, 255, 160));
    while (P.level < 20 && P.xp >= XPForLevel(P.level + 1)) {
        P.level++;
        P.pendingLevels++;
        P.hp = HPCap();
        BigNotify(Fmt("LIVELLO %d", P.level), "Apri il Pip-Boy (TAB) per scegliere abilità e talento");
        PlaySfx(SFX_LEVELUP);
    }
}

int ToolItem(int tool) {
    switch (tool) {
        case TOOL_CAMERA: return I_CAMERA;
        case TOOL_BAIT: return I_BAIT;
        case TOOL_NOISE: return I_NOISE;
        case TOOL_BLASTER: return I_BLASTER;
        default: return -1;
    }
}

const char* ToolName(int tool) {
    int it = ToolItem(tool);
    return it >= 0 ? ITEMS[it].name : "Mani libere";
}

void GiveItem(int id, int count, bool notify) {
    G.player.inv.Add(id, count);
    if (notify) {
        if (count > 1) Notify(Fmt("Ricevuto: %s (%d)", ITEMS[id].name, count));
        else Notify(Fmt("Ricevuto: %s", ITEMS[id].name));
    }
    if (id == I_CAPS) G.player.capsEarned += count;
    QuestsOnPickup(id);
}

void TakeItem(int id, int count) {
    G.player.inv.Remove(id, count);
    if (id == G.player.armor && G.player.inv.Count(id) == 0) G.player.armor = -1;
    int ti = ToolItem(G.player.tool);
    if (ti >= 0 && G.player.inv.Count(ti) == 0) G.player.tool = TOOL_HANDS;
}

Vector3 PlayerEye() { return G.player.pos + V3(0, G.player.eyeH, 0); }
Vector3 PlayerForward() {
    const Player& P = G.player;
    return V3(cosf(P.pitch) * sinf(P.yaw), sinf(P.pitch), cosf(P.pitch) * cosf(P.yaw));
}

bool HostilesNear(float range) {
    for (auto& e : G.ents)
        if (e.hostile && e.aware && !e.off && e.zone == G.player.zone && Vector3Distance(e.pos, G.player.pos) < range) return true;
    return false;
}

// ---------------------------------------------------------------------------
//  Attrezzi
// ---------------------------------------------------------------------------
void EquipTool(int tool) {
    Player& P = G.player;
    int it = ToolItem(tool);
    if (it >= 0 && P.inv.Count(it) <= 0) {
        Notify(Fmt("Non hai: %s", ITEMS[it].name), Col(255, 200, 140));
        return;
    }
    if (P.tool == tool) return;
    P.tool = tool;
    P.equipT = 0.35f;
    P.throwT = 0;
    PlaySfx(SFX_UI_MOVE, 0.6f, 0.8f);
}

void EquipArmor(int item) {
    Player& P = G.player;
    P.armor = (P.armor == item) ? -1 : item;
    PlaySfx(SFX_PICKUP, 0.6f, 0.8f);
}

static float ThrowPower() {
    return 11.0f + SkillValue(SK_ATHLETICS) * 0.06f + (G.player.perks[PK_THROWER] ? 4.0f : 0.0f);
}

static void ThrowTool(int tool) {
    Player& P = G.player;
    int it = ToolItem(tool);
    if (it < 0 || P.inv.Count(it) <= 0) {
        Notify(it >= 0 ? Fmt("Non hai: %s", ITEMS[it].name) : std::string("Niente da lanciare."), Col(255, 200, 140));
        return;
    }
    TakeItem(it, 1);
    Vector3 f = PlayerForward();
    if (P.thirdPerson) f = Vector3Normalize(G.cam.target - G.cam.position);
    ThrowObject(P.zone, PlayerEye() + f * 0.6f, f * ThrowPower() + V3(0, 3.0f, 0), tool);
    PlaySfx(SFX_SWING, 0.7f, 1.1f);
    P.throwT = 0.001f;
    P.useCd = 0.6f;
}

static float AngleToPoint(Vector3 eye, Vector3 f, Vector3 p, float* dist) {
    Vector3 to = p - eye;
    float d = Vector3Length(to);
    if (dist) *dist = d;
    if (d < 0.01f) return 0;
    return acosf(Clamp(Vector3DotProduct(to / d, f), -1.f, 1.f));
}

void TakePhoto() {
    Player& P = G.player;
    Vector3 eye = G.cam.position;
    Vector3 f = Vector3Normalize(G.cam.target - G.cam.position);
    float zoomed = P.aimT > 0.5f;
    float maxAng = (zoomed ? 5.0f : 9.0f) * DEG2RAD;
    float maxDist = zoomed ? 130.0f : 70.0f;
    P.flash = 0.25f;
    P.useCd = 0.8f;
    PlaySfx(SFX_SHUTTER);
    SpawnDynLight(P.zone, eye + f * 0.5f, Col(255, 255, 240), 10, 0.08f);
    std::string bestKey, bestName;
    float bestAng = maxAng;
    // creature e personaggi
    for (auto& e : G.ents) {
        if (e.zone != P.zone || e.photoKey.empty()) continue;
        float d;
        Vector3 c = EntityCenter(e);
        float a = AngleToPoint(eye, f, c, &d);
        float tol = maxAng + atanf(EntityRadius(e) / std::max(d, 1.0f));
        if (d > maxDist || a > tol || a - tol + maxAng > bestAng) continue;
        if (!LineOfSight(P.zone, eye, c)) continue;
        bestAng = a - tol + maxAng;
        bestKey = e.photoKey;
        int pi = PhotoIndex(e.photoKey);
        bestName = pi >= 0 ? PHOTO_SUBJECTS[pi].name : e.name;
        // il lampo può svegliare o insospettire chi è vicino
        if (d < 14 && e.hostile && !e.off) {
            if (e.asleep && d < 8) e.alert += 0.4f;
            else if (!e.asleep) e.alert += 0.35f;
        }
    }
    // luoghi notevoli
    for (auto& s : PHOTO_SUBJECTS) {
        if (!s.landmark || s.zone != P.zone) continue;
        float d;
        float a = AngleToPoint(eye, f, s.pos, &d);
        float tol = maxAng + 0.06f;
        if (d > 170 || a > tol || a - tol + maxAng > bestAng) continue;
        // il punto di riferimento può stare dentro l'edificio: basta vederne la superficie
        Vector3 seen = s.pos + Vector3Normalize(eye - s.pos) * std::min(3.5f, d * 0.5f);
        if (!LineOfSight(P.zone, eye, seen)) continue;
        bestAng = a - tol + maxAng;
        bestKey = s.key;
        bestName = s.name;
    }
    if (bestKey.empty()) {
        Notify("Nessun soggetto interessante nell'inquadratura.", Col(200, 220, 200));
        return;
    }
    bool expeditionPhoto = NetPhoto(bestKey);
    if (HasPhoto(bestKey)) {
        if (!expeditionPhoto) Notify(Fmt("Hai già una foto di: %s", bestName.c_str()), Col(200, 220, 200));
        return;
    }
    P.photos.push_back(bestKey);
    int pi = PhotoIndex(bestKey);
    int xp = pi >= 0 ? PHOTO_SUBJECTS[pi].xp : 20;
    if (P.perks[PK_PHOTOGRAPHER]) xp = xp * 3 / 2;
    BigNotify("NUOVA FOTO", bestName);
    GiveXP(xp, "Fotografia");
    PlaySfx(SFX_DISCOVER, 0.7f);
    QuestsOnPhoto(bestKey);
}

void FireBlaster() {
    Player& P = G.player;
    if (P.inv.Count(I_BLASTER) == 0 || P.useCd > 0) return;
    if (P.ap < 8) { Notify("Blaster scarico: attendi la ricarica dei PA."); return; }
    P.ap -= 8;
    P.useCd = 0.45f;
    P.flash = 0.16f;
    Vector3 origin = G.cam.position;
    Vector3 direction = Vector3Normalize(G.cam.target - origin);
    float distance = 95;
    RayHit worldHit{}, entityHit{};
    if (RaycastWorld(P.zone, origin, direction, distance, worldHit)) distance = worldHit.t;
    bool hitEntity = RaycastEntities(P.zone, origin, direction, distance, entityHit);
    if (hitEntity) distance = entityHit.t;
    Vector3 impact = origin + direction * distance;
    if (!LaserMissionHit(origin, direction, distance, impact) && hitEntity) {
        Entity& e = G.ents[entityHit.entity];
        if (!e.off) {
            e.stunT = 6;
            e.vel = V3(0, 0, 0);
            e.moveAmt = 0;
            e.aware = false;
            e.alert = 0;
            if (!e.companion) e.ai = AI_RETURN;
            bool machine = e.kind == EK_ROBOT || e.kind == EK_CAMERA;
            Notify(e.name + (machine ? ": sovraccarico per 6 secondi!" : ": stordito per 6 secondi!"), Col(110, 235, 255));
        }
    }
    Vector3 right = Vector3Normalize(Vector3CrossProduct(direction, V3(0, 1, 0)));
    SpawnLaser(P.zone, origin + right * 0.2f - V3(0, 0.15f, 0) + direction * 0.5f, impact);
    PlaySfx(SFX_LASER, 0.7f);
}

static void UseTool() {
    Player& P = G.player;
    switch (P.tool) {
        case TOOL_CAMERA: TakePhoto(); break;
        case TOOL_BAIT: ThrowTool(TOOL_BAIT); break;
        case TOOL_NOISE: ThrowTool(TOOL_NOISE); break;
        case TOOL_BLASTER: FireBlaster(); break;
        default: break;
    }
}

void UseItem(int item) {
    Player& P = G.player;
    const ItemDef& d = ITEMS[item];
    if (d.type == IT_TOOL) { EquipTool(d.tool); return; }
    if (d.type == IT_ARMOR) { EquipArmor(item); return; }
    if (d.type != IT_AID) {
        if (item == I_SCOUTNOTE) BigNotify("Biglietto dell'esploratore", "Il deposito è sul bordo orientale del Cratere Verde.");
        else if (item == I_MAINTCODE) BigNotify("Codice di manutenzione", "PROMETEO-0451: spegne la Sentinella Mk.V dal terminale del laboratorio.");
        return;
    }
    if (P.inv.Count(item) <= 0) return;
    P.inv.Remove(item, 1);
    float med = 0.6f + SkillValue(SK_MEDICINE) / 100.0f * 0.8f;
    if (P.perks[PK_MEDIC]) med *= 1.5f;
    float surv = 1.0f + SkillValue(SK_SURVIVAL) / 100.0f;
    if (item == I_MEDKIT) {
        P.healOverTime += d.heal * med;
        P.medkitsUsed++;
        PlaySfx(SFX_STIM);
    } else if (item == I_RADAWAY) {
        P.rads = std::max(0.0f, P.rads + d.rads * med);
        PlaySfx(SFX_STIM, 0.8f, 0.8f);
        Notify("Radiazioni ridotte.");
    } else if (item == I_RADX) {
        P.radXTimer = 180;
        PlaySfx(SFX_EAT, 0.6f, 1.4f);
        Notify("Resistenza alle radiazioni aumentata.");
    } else if (item == I_STEALTHBOY) {
        P.stealthT = d.stealth * (P.perks[PK_SHADOW] ? 2.0f : 1.0f);
        PlaySfx(SFX_STEALTH);
        Notify("Mantello ottico attivo: sei quasi invisibile!", Col(150, 220, 255));
        for (auto& e : G.ents)
            if (e.zone == P.zone && e.ai == AI_CHASE) { e.aware = false; e.alert = 0.3f; e.ai = AI_RETURN; }
    } else if (item == I_COFFEE) {
        P.coffeeTimer = 60;
        P.ap = std::min(MaxAP(), P.ap + d.ap);
        PlaySfx(SFX_EAT, 0.7f, 1.2f);
        Notify("Caffè d'anteguerra: energia a mille!");
    } else if (item == I_HERBTEA) {
        P.teaTimer = 120;
        PlaySfx(SFX_EAT, 0.6f, 1.5f);
        Notify("Tisana del deserto: +2 Intelligenza e Percezione.");
    } else {
        P.healOverTime += d.heal * surv;
        float r = d.rads * (P.perks[PK_LEADBELLY] ? 0.5f : 1.0f);
        P.rads += r * (1.0f - PlayerRadRes() * 0.5f);
        if (d.ap > 0) P.ap = std::min(MaxAP(), P.ap + d.ap);
        PlaySfx(SFX_EAT);
    }
}

// ---------------------------------------------------------------------------
//  Pericoli: radiazioni, cadute, essere scacciati
// ---------------------------------------------------------------------------
void PlayerHurt(float dmg, bool radiation) {
    Player& P = G.player;
    if (G.state == GS_FAINT || G.state == GS_ENDING) return;
    P.hp -= dmg;
    P.hurtFlash = std::min(1.0f, P.hurtFlash + dmg / 22.0f + 0.15f);
    if (!radiation) PlaySfx(SFX_HURT, 0.7f, gRng.range(0.9f, 1.1f));
    if (P.hp <= 0) {
        P.hp = 0;
        PlayerFaint();
    }
}

void PlayerFaint() {
    PlaySfx(SFX_FAINT);
    SetGameState(GS_FAINT);
}

static const char* CatchMessage(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return "I radscarafaggi ti sciamano intorno: scappi a gambe levate!";
        case EK_MOLERAT: return "Il ratto talpa difende la sua tana: meglio girare al largo!";
        case EK_DOG: return "I cani abbaiano e ti rincorrono finché non ti allontani!";
        case EK_GHOUL: return "Un ghoul ti afferra la manica: ti divincoli e scappi!";
        case EK_MUTANT: return "Il mutante ti prende per il colletto e ti accompagna fuori brontolando.";
        case EK_ROBOT: case EK_CAMERA: return "SENTINELLA: \"Intruso individuato. Scorta all'uscita in corso.\"";
        case EK_DEATHCLAW: return "Lo Squarciatore si sveglia con un ruggito: corri via più veloce che puoi!";
        default:
            if (e.fac == FAC_RAIDER) return "I Teschi ti accompagnano fuori dal covo: \"E non tornare senza permesso!\"";
            return "Ti hanno scoperto e cacciato via!";
    }
}

void CatchPlayer(Entity& e) {
    Player& P = G.player;
    if (P.catchImmune > 0 || G.fade != 0) return;
    P.timesCaught++;
    P.catchImmune = 6;
    PlaySfx(SFX_CAUGHT);
    BigNotify("SCOPERTO!", CatchMessage(e));
    // punto sicuro dove ritrovarsi
    int zone = P.zone;
    Vector3 dest = P.pos;
    float yaw = P.yaw;
    if (zone == ZONE_BASE) {
        zone = ZONE_WASTE;
        dest = V3(186, 0, 222);
        yaw = PI / 2;
    } else if (W.zones[zone].outdoor) {
        // all'ingresso del luogo più vicino, se è fuori dal territorio di chi ti ha scoperto...
        float minDist = std::min(e.territory, 30.0f) + 8;
        int best = -1;
        float bd = 1e9f;
        for (int i = 0; i < (int)W.locations.size(); i++) {
            float d = DistXZ(W.locations[i].pos, e.home);
            if (d < W.locations[i].radius + 60 && d < bd && DistXZ(W.locations[i].travelPos, e.home) > minDist) { bd = d; best = i; }
        }
        if (best >= 0) {
            dest = W.locations[best].travelPos;
            yaw = AngleTo(dest, W.locations[best].pos);
        } else {
            // ...altrimenti appena oltre il confine del territorio, dalla parte da cui sei arrivato
            Vector3 away = V3(P.pos.x - e.home.x, 0, P.pos.z - e.home.z);
            if (Vector3Length(away) < 0.1f) away = V3(1, 0, 0);
            float base = atan2f(away.x, away.z);
            for (int k = 0; k < 12; k++) {
                float a = base + (k / 2 + 1) * 0.35f * ((k & 1) ? 1.f : -1.f) * (k > 0 ? 1.f : 0.f);
                Vector3 dir = V3(sinf(a), 0, cosf(a));
                Vector3 p = e.home + dir * (e.territory + 18);
                p.y = TerrainHeight(p.x, p.z);
                dest = p;
                yaw = atan2f(dir.x, dir.z);
                if (!CircleBlocked(zone, V3(p.x, p.y + 0.1f, p.z), 0.4f, 1.8f, 0.6f)) break;
            }
        }
    }
    if (e.fac == FAC_RAIDER && P.inv.Count(I_CAPS) >= 10) {
        TakeItem(I_CAPS, 10);
        Notify("Ti hanno \"multato\" di 10 tappi.", Col(255, 210, 150));
    }
    G.pendingZone = zone;
    G.pendingPos = dest;
    G.pendingYaw = yaw;
    G.fade = 0.001f;
    G.timeOfDay += 0.5f;
    for (auto& o : G.ents)
        if (o.zone == e.zone && (o.ai == AI_CHASE || o.ai == AI_SEARCH)) {
            o.aware = false;
            o.alert = 0;
            o.ai = AI_RETURN;
        }
}

// ---------------------------------------------------------------------------
//  Aggiornamento per frame
// ---------------------------------------------------------------------------
static void FindInteraction() {
    Player& P = G.player;
    G.lookEnt = -1;
    G.lookInter = -1;
    Vector3 eye = PlayerEye(), f = PlayerForward();
    if (P.thirdPerson) f = Vector3Normalize(G.cam.target - G.cam.position);
    float best = 0.38f;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        Entity& e = G.ents[i];
        if (e.zone != P.zone || e.hostile || e.stunT > 0 || e.dlg.empty()) continue;
        Vector3 c = EntityCenter(e);
        Vector3 to = c - eye;
        float d = Vector3Length(to);
        if (d > 4.0f + EntityRadius(e)) continue;
        float ang = acosf(Clamp(Vector3DotProduct(to / d, f), -1.f, 1.f));
        if (d < 1.2f) ang *= 0.5f;
        if (ang < best) { best = ang; G.lookEnt = i; }
    }
    for (int i = 0; i < (int)G.inters.size(); i++) {
        Interactable& it = G.inters[i];
        if (it.zone != P.zone || (it.type == INT_ITEM && it.used)) continue;
        if (it.type == INT_REPAIR && it.used) continue;
        Vector3 c = it.pos + (it.type == INT_CONTAINER ? V3(0, it.size.y * 0.5f, 0) : V3(0, 0, 0));
        Vector3 to = c - eye;
        float d = Vector3Length(to);
        float range = std::max(2.6f, std::max(it.size.x, it.size.y) * 0.65f);
        if (d > range) continue;
        float ang = acosf(Clamp(Vector3DotProduct(to / d, f), -1.f, 1.f));
        float tol = (it.type == INT_TRANSITION || it.type == INT_VAULTDOOR || it.type == INT_GATE || it.type == INT_REACTOR || it.type == INT_LADDER) ? 0.75f : 0.45f;
        if (ang < tol && ang * 0.8f < best) { best = ang * 0.8f; G.lookInter = i; G.lookEnt = -1; }
    }
}

static void CycleTool(int dir) {
    std::vector<int> list = {TOOL_HANDS};
    for (int t = TOOL_CAMERA; t < TOOL_COUNT; t++)
        if (G.player.inv.Count(ToolItem(t)) > 0) list.push_back(t);
    if (list.size() <= 1) return;
    int cur = 0;
    for (int i = 0; i < (int)list.size(); i++)
        if (list[i] == G.player.tool) cur = i;
    EquipTool(list[(cur + dir + (int)list.size()) % (int)list.size()]);
}

void PlayerUpdate(float dt) {
    Player& P = G.player;
    P.playTime += dt;
    // --- visuale ---
    Vector2 md = LookDelta();
    if (G.mouseSkip > 0) { G.mouseSkip--; md = V2(0, 0); }
    float sens = 0.0022f * G.settings.sensitivity * (P.aimT > 0.5f ? 0.45f : 1.0f);
    P.yaw -= md.x * sens;
    P.pitch -= md.y * sens * (G.settings.invertY ? -1.f : 1.f);
    P.pitch = Clamp(P.pitch, -1.5f, 1.5f);
    P.yaw = AngleWrap(P.yaw);
    if (IsKeyPressed(KEY_V)) P.thirdPerson = !P.thirdPerson;
    if (IsKeyPressed(KEY_L)) {
        P.pipLight = !P.pipLight;
        PlaySfx(SFX_UI_MOVE);
    }
    // nel browser Ctrl+W chiuderebbe la scheda: solo C
    if (IsKeyPressed(KEY_C) || (!DUSTFALL_WEB && IsKeyPressed(KEY_LEFT_CONTROL))) P.crouch = !P.crouch;

    // --- movimento ---
    Vector3 fwd = YawDir(P.yaw), right = V3(-cosf(P.yaw), 0, sinf(P.yaw));
    Vector3 wish = V3(0, 0, 0);
    if (IsKeyDown(KEY_W)) wish = wish + fwd;
    if (IsKeyDown(KEY_S)) wish = wish - fwd;
    if (IsKeyDown(KEY_D)) wish = wish + right;
    if (IsKeyDown(KEY_A)) wish = wish - right;
    bool moving = Vector3Length(wish) > 0.1f;
    if (moving) wish = Vector3Normalize(wish);
    bool zooming = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && P.tool == TOOL_CAMERA;
    P.aimT = LerpF(P.aimT, zooming ? 1.0f : 0.0f, Clamp01(dt * 12));
    bool encumbered = P.inv.Weight() > CarryMax();
    P.sprint = IsKeyDown(KEY_LEFT_SHIFT) && moving && P.ap > 1 && !encumbered && !zooming && IsKeyDown(KEY_W);
    if (P.sprint && P.crouch) P.crouch = false;
    float speed = P.crouch ? 2.2f : (P.sprint ? 7.2f + SkillValue(SK_ATHLETICS) * 0.01f : 4.4f);
    if (zooming) speed = std::min(speed, 2.4f);
    speed *= PlayerSpeedMul();
    float apRegen = (7.0f + Spec(ST_AGI) * 0.7f) * (P.coffeeTimer > 0 ? 2.0f : 1.0f);
    if (P.sprint) P.ap = std::max(0.0f, P.ap - (P.perks[PK_RUNNER] ? 5.5f : 11.0f) * (1.0f - SkillValue(SK_ATHLETICS) / 300.0f) * dt);
    else P.ap = std::min(MaxAP(), P.ap + apRegen * dt);
    Vector3 target = wish * speed;
    float accel = P.onGround ? 14.0f : 2.0f;
    P.vel.x = LerpF(P.vel.x, target.x, Clamp01(dt * accel));
    P.vel.z = LerpF(P.vel.z, target.z, Clamp01(dt * accel));
    float noise = 0;
    if (IsKeyPressed(KEY_SPACE) && P.onGround) {
        P.vel.y = 5.4f + SkillValue(SK_ATHLETICS) * 0.012f;
        P.onGround = false;
        P.crouch = false;
        PlaySfx(SFX_JUMP, 0.5f);
        noise = std::max(noise, 7.0f);
    }
    P.vel.y -= 18.0f * dt;
    Vector3 np = P.pos + P.vel * dt;
    float height = P.crouch ? 1.15f : 1.75f;
    ResolveCollision(P.zone, np, 0.35f, height, 0.5f);
    float g = GroundHeight(P.zone, np, 0.5f);
    bool wasGround = P.onGround;
    if (np.y <= g + 0.02f || (wasGround && P.vel.y <= 0 && np.y - g < 0.45f)) {
        if (!wasGround) {
            float fall = P.fallStartY - g;
            if (fall > 1.2f) {
                PlaySfx(SFX_LAND, Clamp01(fall / 4));
                noise = std::max(noise, 9.0f);
            }
            float safe = 5.5f + SkillValue(SK_ATHLETICS) * 0.02f;
            if (fall > safe) PlayerHurt((fall - safe) * 12.0f, false);
        }
        np.y = g;
        P.vel.y = 0;
        P.onGround = true;
        P.fallStartY = g;
    } else {
        if (P.onGround) P.fallStartY = np.y;
        P.onGround = false;
        P.fallStartY = std::max(P.fallStartY, np.y);
    }
    float hsMove = DistXZ(P.pos, np);
    P.walked += hsMove;
    P.pos = np;
    P.eyeH = LerpF(P.eyeH, P.crouch ? 1.05f : 1.62f, Clamp01(dt * 10));

    float hs = Vector3Length(V3(P.vel.x, 0, P.vel.z));
    if (P.onGround && hs > 0.5f) {
        P.bob += hs * dt * 1.9f;
        P.stepDist += hs * dt;
        if (P.stepDist > (P.sprint ? 2.6f : 2.1f)) {
            P.stepDist = 0;
            PlaySfx((SfxId)(SFX_STEP1 + gRng.irange(0, 3)), P.crouch ? 0.15f : (P.sprint ? 0.5f : 0.32f), gRng.range(0.9f, 1.1f));
        }
        if (P.sprint) noise = std::max(noise, P.perks[PK_SOFTSTEP] ? 8.0f : 14.0f);
        else if (!P.crouch) noise = std::max(noise, 4.0f);
    }
    if (P.armor == I_EXOSUIT && hs > 0.5f) noise = std::max(noise, 10.0f);
    if (P.stealthT > 0) noise *= 0.3f;
    P.noise = noise;

    // --- attrezzi ---
    if (P.useCd > 0) P.useCd -= dt;
    if (P.equipT > 0) P.equipT -= dt;
    if (P.flash > 0) P.flash -= dt;
    if (P.throwT > 0) {
        P.throwT += dt / 0.45f;
        if (P.throwT >= 1) P.throwT = 0;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && P.useCd <= 0 && P.equipT <= 0) UseTool();
    if (IsKeyPressed(KEY_G) && P.useCd <= 0) ThrowTool(TOOL_NOISE);
    if (IsKeyPressed(KEY_B) && P.useCd <= 0) ThrowTool(TOOL_BAIT);
    float wheel = GetMouseWheelMove();
    if (wheel > 0.5f) CycleTool(1);
    else if (wheel < -0.5f) CycleTool(-1);
    for (int k = 0; k < TOOL_COUNT; k++)
        if (IsKeyPressed(KEY_ONE + k)) EquipTool(k);
    if (IsKeyPressed(KEY_H)) {
        if (P.inv.Count(I_MEDKIT) > 0) UseItem(I_MEDKIT);
        else Notify("Non hai kit medici.");
    }
    if (IsKeyPressed(KEY_F)) {
        if (P.stealthT > 0) Notify("Il mantello ottico è già attivo.");
        else if (P.inv.Count(I_STEALTHBOY) > 0) UseItem(I_STEALTHBOY);
        else Notify("Non hai mantelli ottici.");
    }

    // --- interazione ---
    FindInteraction();
    if (IsKeyPressed(KEY_E)) {
        if (LaserMissionInteract()) {}
        else if (G.lookInter >= 0) Interact(G.lookInter);
        else if (G.lookEnt >= 0) DialogueStart(G.lookEnt);
    }

    // --- radiazioni, cure, effetti ---
    float rate = RadiationAt(P.zone, P.pos) * (1.0f - PlayerRadRes());
    G.radRate = rate;
    if (rate > 0.01f) {
        float before = P.rads;
        P.rads += rate * dt;
        for (int th : {200, 400, 600, 800})
            if (before < th && P.rads >= th) Notify(Fmt("Avvelenamento da radiazioni: %d rad!", th), Col(255, 230, 80));
        G.geigerAcc += dt * (2.0f + rate * 2.5f);
        if (G.geigerAcc > 1.0f && gRng.chance(0.6f)) {
            PlaySfx(SFX_GEIGER, Clamp(0.3f + rate * 0.05f, 0.3f, 0.9f), gRng.range(0.8f, 1.3f));
            G.geigerAcc = gRng.range(0, 0.6f);
        }
    }
    if (P.rads >= 1000) {
        P.rads = 999;
        P.hp = 0;
        PlayerFaint();
        return;
    }
    if (P.hp > HPCap()) P.hp = HPCap();
    if (P.healOverTime > 0) {
        float a = std::min(P.healOverTime, 35.0f * dt);
        P.hp += a;
        P.healOverTime -= a;
    }
    if (P.perks[PK_REGEN]) P.hp += 1.5f * dt;
    P.hp = std::min(P.hp, HPCap());
    if (P.radXTimer > 0) P.radXTimer -= dt;
    if (P.teaTimer > 0) P.teaTimer -= dt;
    if (P.coffeeTimer > 0) P.coffeeTimer -= dt;
    if (P.catchImmune > 0) P.catchImmune -= dt;
    if (P.stealthT > 0) {
        P.stealthT -= dt;
        if (P.stealthT <= 0) {
            Notify("Il mantello ottico si è spento.", Col(150, 220, 255));
            PlaySfx(SFX_STEALTH, 0.6f, 0.7f);
        }
    }
    if (P.hurtFlash > 0) P.hurtFlash = std::max(0.0f, P.hurtFlash - dt * 1.3f);
    if (P.pipLight) SpawnDynLight(P.zone, PlayerEye() + PlayerForward() * 0.5f, Col(160, 255, 170), 14, 0.03f);
}

// ---------------------------------------------------------------------------
//  Camera
// ---------------------------------------------------------------------------
void UpdateCamera3D() {
    Player& P = G.player;
    Vector3 f = PlayerForward();
    Vector3 eye = PlayerEye();
    float hs = Vector3Length(V3(P.vel.x, 0, P.vel.z));
    float bobAmt = P.onGround ? Clamp01(hs / 6.0f) : 0;
    Vector3 right = Vector3Normalize(Vector3CrossProduct(f, V3(0, 1, 0)));
    float fov = G.settings.fov;
    fov = LerpF(fov, fov / 2.6f, P.aimT);
    if (P.thirdPerson) {
        Vector3 desired = eye - f * 3.2f + right * 0.7f + V3(0, 0.35f, 0);
        Vector3 dir = desired - eye;
        float len = Vector3Length(dir);
        RayHit h;
        if (RaycastWorld(P.zone, eye, dir / len, len, h)) desired = eye + dir / len * std::max(0.4f, h.t - 0.3f);
        G.cam.position = desired;
        G.cam.target = desired + f;
    } else {
        Vector3 bob = right * (sinf(P.bob) * 0.035f * bobAmt) + V3(0, fabsf(cosf(P.bob)) * 0.05f * bobAmt, 0);
        G.cam.position = eye + bob;
        G.cam.target = G.cam.position + f;
    }
    G.cam.up = V3(0, 1, 0);
    G.cam.fovy = fov;
    G.cam.projection = CAMERA_PERSPECTIVE;
}

// ---------------------------------------------------------------------------
//  Mani e attrezzo in prima persona (disegnati vicinissimi alla camera)
// ---------------------------------------------------------------------------
void DrawViewModel() {
    Player& P = G.player;
    if (P.thirdPerson || P.aimT > 0.6f) return;
    Vector3 f = Vector3Normalize(G.cam.target - G.cam.position);
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, V3(0, 1, 0)));
    Vector3 u = Vector3CrossProduct(r, f);
    Vector3 eye = G.cam.position;
    Matrix B = Matrix{r.x, u.x, f.x, eye.x, r.y, u.y, f.y, eye.y, r.z, u.z, f.z, eye.z, 0, 0, 0, 1};
    Matrix S = MatrixScale(0.1f, 0.1f, 0.1f) * B;
    float hs = Vector3Length(V3(P.vel.x, 0, P.vel.z));
    float bobAmt = P.onGround ? Clamp01(hs / 6.0f) : 0;
    Vector2 md = LookDelta();
    static Vector2 sway{0, 0};
    sway.x = LerpF(sway.x, Clamp(md.x * 0.0006f, -0.03f, 0.03f), Clamp01(GetFrameTime() * 8));
    sway.y = LerpF(sway.y, Clamp(md.y * 0.0006f, -0.03f, 0.03f), Clamp01(GetFrameTime() * 8));
    Vector3 bob = V3(sinf(P.bob) * 0.012f * bobAmt - sway.x, fabsf(cosf(P.bob)) * 0.01f * bobAmt + sway.y, 0);
    float lower = P.equipT > 0 ? P.equipT * 0.6f : 0;
    Color sleeve = OutfitColor(ArmorOutfit()), skin = Col(215, 170, 140);
    if (P.armor == I_EXOSUIT) skin = Col(95, 98, 88);
    if (P.sprint) lower += 0.05f;
    // mano destra con l'attrezzo
    Vector3 hand = V3(0.17f, -0.2f, 0.48f) + bob - V3(0, lower, 0);
    float pitch = 0, yawA = 0;
    if (P.throwT > 0) {
        float t = P.throwT;
        float k1 = t < 0.35f ? t / 0.35f : 1.0f, k2 = t < 0.35f ? 0 : sinf((t - 0.35f) / 0.65f * PI);
        hand = hand + V3(0.04f * k1 - 0.1f * k2, 0.14f * k1 - 0.06f * k2, -0.12f * k1 + 0.2f * k2);
        pitch = -0.9f * k1 + 1.2f * k2;
    }
    if (P.flash > 0.15f) hand.z -= 0.01f;
    float toolScale = 0.75f;
    if (P.tool == TOOL_CAMERA) {
        // la fotocamera si tiene con due mani, in basso a destra
        hand = V3(0.2f, -0.22f, 0.5f) + bob - V3(0, lower, 0);
        toolScale = 0.6f;
    }
    Matrix toolM = MatrixScale(toolScale, toolScale, toolScale) * MatrixRotateXYZ(V3(pitch, yawA, 0)) * MatrixTranslate(hand.x - (P.tool == TOOL_CAMERA ? 0.05f : 0), hand.y + 0.03f, hand.z);
    DrawLimb(S, V3(0.3f, -0.5f, 0.22f), hand - V3(0, 0.02f, 0.06f), 0.032f, sleeve);
    DrawPrim(PR_SPHERE, MatTRS(hand, V3(0, 0, 0), V3(0.07f, 0.065f, 0.085f)) * S, skin);
    if (P.tool != TOOL_HANDS) DrawToolModel(toolM * S, P.tool, P.flash > 0 ? 1.0f : 0.0f);
    // mano sinistra
    Vector3 lh = P.tool == TOOL_CAMERA ? V3(0.1f, -0.23f, 0.5f) + bob - V3(0, lower, 0) : V3(-0.18f, -0.25f, 0.45f) + bob - V3(0, lower, 0);
    DrawLimb(S, V3(-0.3f, -0.5f, 0.24f), lh - V3(0, 0.02f, 0.06f), 0.032f, sleeve);
    DrawPrim(PR_SPHERE, MatTRS(lh, V3(0, 0, 0), V3(0.07f, 0.065f, 0.085f)) * S, skin);
}

void DrawPlayerBody() {
    Player& P = G.player;
    if (!P.thirdPerson) return;
    Look look;
    look.outfit = ArmorOutfit();
    look.female = P.female;
    look.hairStyle = P.female ? 2 : 1;
    look.hair = Col(70, 50, 35);
    look.skin = Col(215, 170, 140);
    float hs = Vector3Length(V3(P.vel.x, 0, P.vel.z));
    float hold = P.tool == TOOL_CAMERA ? 1.0f : (P.tool != TOOL_HANDS ? 0.4f : 0.0f);
    float action = P.throwT;
    if (NetLocalWave() > 0 && action <= 0) {
        // saluto agli altri giocatori (tasto Z)
        hold = 0;
        action = 0.2f + 0.15f * sinf(G.time * 12.0f);
    }
    DrawHumanoid(P.pos, P.yaw, look, P.bob * 1.1f, Clamp01(hs / 4.5f), hold, action, 0, P.tool, P.flash > 0 ? 1 : 0, 1.0f, false, P.crouch ? 1.0f : 0.0f);
    if (P.stealthT > 0) {
        // alone azzurro del mantello ottico
        BeginBlendMode(BLEND_ADDITIVE);
        DrawSphere(P.pos + V3(0, 1, 0), 0.9f, ColAlpha(Col(80, 160, 255), 0.12f));
        EndBlendMode();
    }
}
