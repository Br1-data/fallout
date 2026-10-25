// Entità: combattimento originale, resistenza agli impulsi e stordimento permanente.
#include "game.h"

// ---------------------------------------------------------------------------
//  Creazione
// ---------------------------------------------------------------------------
Entity CreateEntity(EntKind k, int zone, Vector3 pos, float yaw) {
    Entity e;
    e.kind = k;
    e.zone = zone;
    e.pos = pos;
    e.home = pos;
    e.yaw = yaw;
    e.anim = gRng.range(0, 6);
    e.strafe = gRng.chance(0.5f) ? 1.f : -1.f;
    e.aiTimer = gRng.range(0, 4);
    switch (k) {
        case EK_HUMAN:
            e.name = "Abitante";
            e.maxHp = 60; e.speed = 5.2f; e.detect = 30; e.meleeDmg = 7; e.xp = 20;
            break;
        case EK_ROACH:
            e.name = "Radscarafaggio";
            e.maxHp = 14; e.speed = 5.5f; e.detect = 16; e.meleeDmg = 4; e.xp = 5;
            e.fac = FAC_CREATURE; e.hostile = true; e.wanderRadius = 6;
            break;
        case EK_MOLERAT:
            e.name = "Ratto talpa";
            e.maxHp = 38; e.dr = 2; e.speed = 5.0f; e.detect = 20; e.meleeDmg = 9; e.xp = 10;
            e.fac = FAC_CREATURE; e.hostile = true; e.wanderRadius = 8;
            e.inv.Add(I_MEAT);
            break;
        case EK_DOG:
            e.name = "Cane selvatico";
            e.maxHp = 32; e.speed = 7.5f; e.detect = 28; e.meleeDmg = 8; e.xp = 9;
            e.fac = FAC_CREATURE; e.hostile = true; e.wanderRadius = 12;
            break;
        case EK_GHOUL:
            e.name = "Ghoul selvaggio";
            e.maxHp = 70; e.dr = 4; e.speed = 6.6f; e.detect = 24; e.meleeDmg = 11; e.xp = 15;
            e.fac = FAC_CREATURE; e.hostile = true; e.wanderRadius = 10;
            e.look.outfit = OF_GHOUL;
            e.look.skin = Col(122, 106, 76);
            e.look.hairStyle = 0;
            if (gRng.chance(0.4f)) e.inv.Add(I_CAPS, gRng.irange(2, 12));
            break;
        case EK_MUTANT:
            e.name = "Mutante";
            e.maxHp = 190; e.dr = 12; e.speed = 4.6f; e.detect = 35; e.meleeDmg = 26; e.xp = 45; e.scale = 1.35f;
            e.fac = FAC_MUTANT; e.hostile = true; e.accuracy = 0.45f; e.wanderRadius = 8;
            e.look.outfit = OF_MUTANT;
            e.look.skin = Col(122, 134, 72);
            e.look.hairStyle = 0;
            e.inv.Add(I_CAPS, gRng.irange(5, 30));
            break;
        case EK_ROBOT:
            e.name = "Sentinella";
            e.maxHp = 130; e.dr = 22; e.speed = 3.4f; e.detect = 32; e.xp = 35;
            e.fac = FAC_ROBOT; e.hostile = true; e.weapon = I_LASERPISTOL; e.accuracy = 0.55f; e.wanderRadius = 5;
            e.inv.Add(I_AMMO_CELL, gRng.irange(8, 20));
            if (gRng.chance(0.5f)) e.inv.Add(I_FUSE);
            break;
        case EK_TURRET:
            e.name = "Torretta automatica";
            e.maxHp = 85; e.dr = 18; e.speed = 0; e.detect = 34; e.xp = 20;
            e.fac = FAC_ROBOT; e.hostile = true; e.weapon = I_ASSAULT; e.accuracy = 0.4f; e.wanderRadius = 0;
            e.inv.Add(I_AMMO_556, gRng.irange(10, 25));
            break;
        case EK_DEATHCLAW:
            e.name = "Squarciatore";
            e.maxHp = 650; e.dr = 20; e.speed = 8.6f; e.detect = 40; e.meleeDmg = 48; e.xp = 300;
            e.fac = FAC_CREATURE; e.hostile = true; e.wanderRadius = 10;
            break;
        default: break;
    }
    e.hp = e.maxHp;
    return e;
}

float EntityReach(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return 1.2f;
        case EK_MOLERAT: return 1.6f;
        case EK_DOG: return 1.6f;
        case EK_GHOUL: return 1.8f;
        case EK_MUTANT: return 2.3f * e.scale / 1.35f;
        case EK_DEATHCLAW: return 3.3f;
        default: return 1.9f * e.scale;
    }
}

float EntityRadius(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return 0.35f;
        case EK_MOLERAT: return 0.5f;
        case EK_DOG: return 0.45f;
        case EK_DEATHCLAW: return 1.0f;
        case EK_TURRET: return 0.55f;
        case EK_ROBOT: return 0.5f * e.scale;
        default: return 0.35f * e.scale;
    }
}

static float EntityHeight(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return 0.4f;
        case EK_MOLERAT: return 0.9f;
        case EK_DOG: return 1.0f;
        case EK_DEATHCLAW: return 3.0f;
        case EK_TURRET: return 1.5f;
        case EK_ROBOT: return 2.1f * e.scale;
        default: return 1.8f * e.scale;
    }
}

static bool IsHumanoid(const Entity& e) { return e.kind == EK_HUMAN || e.kind == EK_GHOUL || e.kind == EK_MUTANT; }

static bool UsesRanged(const Entity& e) {
    if (e.weapon < 0) return false;
    return !WEAPONS[ITEMS[e.weapon].weapon].melee;
}

Entity* FindEntityByTag(const std::string& tag) {
    for (auto& e : G.ents)
        if (e.tag == tag) return &e;
    return nullptr;
}

// ---------------------------------------------------------------------------
//  Volumi di collisione per i colpi
// ---------------------------------------------------------------------------
struct HitSphere { Vector3 c; float r; int part; };

static int HitSpheres(const Entity& e, HitSphere* out) {
    int n = 0;
    auto A = [&](float x, float y, float z, float r, int part) { out[n++] = {V3(x, y, z), r, part}; };
    switch (e.kind) {
        case EK_ROACH:
            A(0, 0.16f, -0.05f, 0.3f, 0); A(0, 0.14f, 0.33f, 0.13f, 1); A(0, 0.08f, -0.3f, 0.2f, 2);
            break;
        case EK_MOLERAT:
            A(0, 0.42f, -0.1f, 0.45f, 0); A(0, 0.45f, 0.55f, 0.24f, 1); A(0, 0.15f, 0, 0.3f, 2);
            break;
        case EK_DOG:
            A(0, 0.6f, 0.1f, 0.3f, 0); A(0, 0.6f, -0.3f, 0.28f, 0); A(0, 0.9f, 0.55f, 0.18f, 1); A(0, 0.28f, 0, 0.26f, 2);
            break;
        case EK_ROBOT:
            A(0, 1.25f, 0, 0.45f, 0); A(0, 1.85f, 0, 0.3f, 1); A(0, 0.5f, 0, 0.35f, 2);
            break;
        case EK_TURRET:
            A(0, 0.6f, 0, 0.45f, 0); A(0, 1.2f, 0.1f, 0.38f, 1); A(0, 0.2f, 0, 0.4f, 2);
            break;
        case EK_DEATHCLAW:
            A(0, 1.8f, 0.1f, 0.75f, 0); A(0, 2.0f, 0.65f, 0.6f, 0); A(0, 2.5f, 1.2f, 0.38f, 1); A(0, 0.8f, 0, 0.55f, 2); A(0, 1.3f, -0.6f, 0.5f, 0);
            break;
        default: {
            float hz = e.kind == EK_GHOUL ? 0.12f : 0.0f;
            A(0, 1.62f, hz, 0.15f, 1); A(0, 1.27f, hz * 0.5f, 0.26f, 0); A(0, 0.98f, 0, 0.22f, 0); A(0, 0.62f, 0, 0.19f, 2); A(0, 0.25f, 0, 0.18f, 2);
            break;
        }
    }
    float s = e.kind == EK_DEATHCLAW || e.kind == EK_ROBOT || e.kind == EK_TURRET ? e.scale : e.scale;
    Matrix m = MatrixScale(s, s, s) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    for (int i = 0; i < n; i++) {
        out[i].c = Vector3Transform(out[i].c, m);
        out[i].r *= s;
    }
    return n;
}

Vector3 EntityPartPos(const Entity& e, int part) {
    HitSphere hs[8]{};
    int n = HitSpheres(e, hs);
    for (int i = 0; i < n; i++)
        if (hs[i].part == part) return hs[i].c;
    return e.pos + V3(0, 1, 0);
}

const char* EntityPartName(const Entity& e, int part) {
    if (e.kind == EK_ROBOT || e.kind == EK_TURRET) return part == 1 ? "Sensori" : (part == 2 ? "Base" : "Telaio");
    if (IsHumanoid(e)) return part == 1 ? "Testa" : (part == 2 ? "Gambe" : "Torso");
    return part == 1 ? "Testa" : (part == 2 ? "Zampe" : "Corpo");
}

bool RaycastEntities(int zone, Vector3 o, Vector3 dir, float maxT, RayHit& hit, int ignore) {
    bool found = false;
    float best = maxT;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        const Entity& e = G.ents[i];
        if (e.stunned || e.zone != zone || i == ignore) continue;
        // test grossolano
        Vector3 oc = e.pos + V3(0, 1, 0) - o;
        float tc = Vector3DotProduct(oc, dir);
        if (tc < -3 || tc > best + 3) continue;
        float d2 = Vector3LengthSqr(oc) - tc * tc;
        if (d2 > 16 * e.scale * e.scale) continue;
        HitSphere hs[8]{};
        int n = HitSpheres(e, hs);
        for (int k = 0; k < n; k++) {
            Vector3 L = hs[k].c - o;
            float t = Vector3DotProduct(L, dir);
            float dd = Vector3LengthSqr(L) - t * t;
            float r2 = hs[k].r * hs[k].r;
            if (dd > r2) continue;
            float th = sqrtf(r2 - dd);
            float t0 = t - th;
            if (t0 < 0) t0 = t + th;
            if (t0 < 0 || t0 >= best) continue;
            best = t0;
            found = true;
            hit.hit = true;
            hit.t = t0;
            hit.point = o + dir * t0;
            hit.normal = Vector3Normalize(hit.point - hs[k].c);
            hit.entity = i;
            hit.part = hs[k].part;
            hit.metal = e.kind == EK_ROBOT || e.kind == EK_TURRET;
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
//  Danni
// ---------------------------------------------------------------------------
static SfxId VoiceOf(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return SFX_ROACH;
        case EK_MOLERAT: return SFX_GROWL;
        case EK_DOG: return SFX_DOG;
        case EK_GHOUL: return SFX_GHOUL;
        case EK_MUTANT: return SFX_MUTANT;
        case EK_ROBOT: case EK_TURRET: return SFX_ROBOT;
        case EK_DEATHCLAW: return SFX_ROAR;
        default: return SFX_HURT;
    }
}

void MakeEntityHostile(Entity& e) {
    if (e.hostile || e.stunned) return;
    e.hostile = true;
    e.aware = true;
    e.ai = AI_COMBAT;
    e.lastSeen = G.player.pos;
    if (e.fac == FAC_TOWN || e.fac == FAC_VAULT) {
        G.player.karma -= 15;
        Notify("Hai attaccato un innocente. Karma diminuito.", Col(255, 120, 100));
        for (auto& o : G.ents)
            if (!o.stunned && o.zone == e.zone && o.fac == e.fac && Vector3Distance(o.pos, e.pos) < 70 && (o.dlg == "guard" || o.dlg == "vault_guard")) {
                o.hostile = true;
                o.aware = true;
                o.ai = AI_COMBAT;
                o.lastSeen = G.player.pos;
            }
    }
}

void AlertNearby(int zone, Vector3 p, float radius) {
    for (auto& e : G.ents) {
        if (e.stunned || e.zone != zone || !e.hostile || e.aware) continue;
        if (Vector3Distance(e.pos, p) < radius) {
            e.aware = true;
            e.ai = AI_COMBAT;
            e.lastSeen = G.player.pos;
            e.alert = 1;
        }
    }
}

void ApplyStunDamage(Entity& e, float dmg, int part, Vector3 dir, bool byPlayer, bool crit) {
    if (e.stunned) return;
    float mult = 1.0f;
    if (part == 1) mult = (e.kind == EK_ROBOT || e.kind == EK_TURRET) ? 1.5f : 2.0f;
    else if (part == 2) mult = 0.8f;
    float final_ = dmg * mult * 40.0f / (40.0f + e.dr);
    e.hp -= final_;
    e.hitFlash = 0.12f;
    if (part == 2 && (e.kind != EK_TURRET)) {
        e.cripple += final_ / e.maxHp;
        if (e.cripple > 0.35f && e.cripple - final_ / e.maxHp <= 0.35f && byPlayer) Notify(std::string(e.name) + ": movimento rallentato dall'impulso!", Col(110, 235, 255));
    }
    Vector3 hp = EntityPartPos(e, part);
    SpawnStunSparks(e.zone, hp, dir, Col(110, 235, 255), crit ? 14 : 7);
    if (byPlayer) {
        if (!e.hostile) MakeEntityHostile(e);
        e.aware = true;
        e.ai = AI_COMBAT;
        e.lastSeen = G.player.pos;
        AlertNearby(e.zone, e.pos, 18);
    }
    if (e.voiceCd <= 0 && e.hp > 0) {
        PlaySfx3D(VoiceOf(e), e.pos, 0.8f);
        e.voiceCd = 1.2f;
    }
    if (e.hp <= 0) {
        e.hp = 0;
        e.stunned = true;
        e.stunAnimT = 0;
        e.vel = V3(0, 0, 0);
        e.moveAmt = 0;
        e.aware = false;
        e.alert = 0;
        e.ai = AI_IDLE;
        e.attackAnim = 0;
        e.attackCd = 0;
        e.burst = 0;
        e.reloadT = 0;
        PlaySfx3D(SFX_LASER, e.pos, 0.65f, 0);
        SpawnDynLight(e.zone, hp, Col(90, 225, 255), 6, 0.3f);
        if (byPlayer) PlayerIncapacitatedEntity(e);
        else QuestsOnIncapacitate(e);
    }
}

// ---------------------------------------------------------------------------
//  Intelligenza artificiale
// ---------------------------------------------------------------------------
static bool MoveTowards(Entity& e, Vector3 target, float speed, float dt, bool face = true) {
    Vector3 to = target - e.pos;
    to.y = 0;
    float d = Vector3Length(to);
    if (d < 0.25f) { e.vel = V3(0, 0, 0); return true; }
    float baseYaw = atan2f(to.x, to.z);
    float r = EntityRadius(e), h = EntityHeight(e);
    float probeLen = std::max(1.0f, r * 2.5f);
    auto Free = [&](float a) { return !CircleBlocked(e.zone, e.pos + YawDir(a) * probeLen, r * 0.9f, h, 0.6f); };
    float chosen = baseYaw;
    if (e.detourT > 0) {
        // mantiene la deviazione scelta per aggirare l'ostacolo
        e.detourT -= dt;
        if (Free(baseYaw) && e.detourT < 0.8f) e.detourT = 0;
        else if (Free(e.detourYaw)) chosen = e.detourYaw;
        else e.detourT = 0;
    }
    if (e.detourT <= 0) {
        static const float offs[] = {0, 0.5f, 1.0f, 1.5f, 2.1f, 2.7f};
        bool ok = false;
        for (float o : offs) {
            for (int s = 0; s < (o == 0 ? 1 : 2); s++) {
                float a = baseYaw + (s == 0 ? o * e.strafe : -o * e.strafe);
                if (Free(a)) {
                    chosen = a;
                    ok = true;
                    if (o > 0.4f) { e.detourT = 1.6f; e.detourYaw = a; }
                    break;
                }
            }
            if (ok) break;
        }
        if (!ok) e.strafe = -e.strafe;
    }
    float spd = speed * (e.cripple > 0.35f ? 0.55f : 1.0f);
    e.vel = YawDir(chosen) * spd;
    e.pos = e.pos + e.vel * dt;
    if (face) e.yaw += AngleWrap(chosen - e.yaw) * Clamp01(dt * 8);
    return false;
}

static void FacePlayer(Entity& e, float dt) {
    float a = AngleTo(e.pos, G.player.pos);
    e.yaw += AngleWrap(a - e.yaw) * Clamp01(dt * (e.kind == EK_TURRET ? 4.0f : 8.0f));
}

static Vector3 EntityEye(const Entity& e) {
    float h = EntityHeight(e) * 0.85f;
    return e.pos + V3(0, h, 0);
}

static Vector3 Muzzle(const Entity& e) {
    if (e.kind == EK_TURRET) return e.pos + V3(0, 1.2f * e.scale, 0) + YawDir(e.yaw) * 0.9f;
    if (e.kind == EK_ROBOT) return e.pos + V3(0, 1.2f * e.scale, 0) + YawDir(e.yaw) * 0.8f * e.scale + V3(cosf(e.yaw), 0, -sinf(e.yaw)) * 0.45f * e.scale;
    return e.pos + V3(0, 1.38f * e.scale, 0) + YawDir(e.yaw) * 0.7f * e.scale + V3(cosf(e.yaw), 0, -sinf(e.yaw)) * 0.12f;
}

static void EntityShoot(Entity& e) {
    const WeaponDef& w = WEAPONS[ITEMS[e.weapon].weapon];
    Vector3 from = Muzzle(e);
    Vector3 target = PlayerChest();
    float d = Vector3Distance(from, target);
    float acc = e.accuracy * Clamp(1.0f - d / (w.range * 1.2f), 0.25f, 1.0f);
    if (G.player.crouch) acc *= 0.85f;
    if (Vector3Length(G.player.vel) > 6) acc *= 0.75f;
    if (G.state == GS_VATS) acc *= 0.5f;
    bool hit = gRng.chance(acc);
    Vector3 end = target;
    if (!hit) end = target + V3(gRng.range(-1.5f, 1.5f), gRng.range(-0.8f, 1.2f), gRng.range(-1.5f, 1.5f));
    Vector3 dir = Vector3Normalize(end - from);
    RayHit wh;
    float maxT = Vector3Distance(from, end) + (hit ? 0 : 30);
    if (RaycastWorld(e.zone, from, dir, maxT, wh)) {
        end = wh.point;
        if (hit && wh.t < d - 0.5f) hit = false;
        if (!hit) SpawnImpact(e.zone, wh.point, wh.normal, wh.metal);
    } else if (!hit) end = from + dir * maxT;
    SpawnTracer(e.zone, from, end, w.beam, 0.035f, 0.18f);
    SpawnDynLight(e.zone, from, w.beam, 6, 0.06f);
    PlaySfx3D(w.sfx, from, 0.9f);
    if (hit) {
        float dmg = w.damage * (w.pellets > 1 ? w.pellets * 0.55f * Clamp(1.2f - d / w.range, 0.2f, 1.0f) : 1.0f);
        float k = e.kind == EK_HUMAN || e.kind == EK_MUTANT ? 0.62f : 0.75f;
        PlayerTakeDamage(dmg * k * gRng.range(0.8f, 1.15f), from);
    }
}

static void HostileAI(Entity& e, float dt, float d, int idx) {
    Player& P = G.player;
    e.losTimer -= dt;
    if (e.losTimer <= 0) {
        e.losTimer = 0.2f + gRng.range(0, 0.15f);
        e.los = d < 140 && LineOfSight(e.zone, EntityEye(e), PlayerEye());
    }
    if (!e.aware) {
        float range = e.detect;
        if (P.crouch) range *= 0.45f;
        range *= 1.0f - SkillValue(SK_SNEAK) / 260.0f;
        if (P.sprint && !P.perks[PK_NINJA]) range *= 1.35f;
        if (R.night > 0.5f && !P.pipLight && W.zones[e.zone].outdoor) range *= 0.75f;
        float ang = fabsf(AngleWrap(AngleTo(e.pos, P.pos) - e.yaw));
        bool inFov = ang < 1.2f || d < 5;
        if (d < range && e.los && inFov) e.alert += dt * (1.7f - d / range) * 1.4f;
        else e.alert = std::max(0.0f, e.alert - dt * 0.25f);
        if (e.alert >= 1.0f) {
            e.aware = true;
            e.ai = AI_COMBAT;
            e.lastSeen = P.pos;
            PlaySfx3D(VoiceOf(e), e.pos, 1.0f);
            for (auto& o : G.ents)
                if (&o != &e && !o.stunned && o.hostile && o.zone == e.zone && o.fac == e.fac && Vector3Distance(o.pos, e.pos) < 25) {
                    o.aware = true;
                    o.ai = AI_COMBAT;
                    o.lastSeen = P.pos;
                }
        }
        // pattuglia o vagabondaggio
        if (e.kind == EK_TURRET) {
            e.yaw += dt * 0.4f * sinf(G.time * 0.3f + idx);
            return;
        }
        if (e.alert > 0.3f) {
            FacePlayer(e, dt * 0.5f);
            return;
        }
        if (!e.patrol.empty()) {
            if (MoveTowards(e, e.patrol[e.patrolIdx], e.speed * 0.35f, dt)) e.patrolIdx = (e.patrolIdx + 1) % e.patrol.size();
            return;
        }
        e.aiTimer -= dt;
        if (e.aiTimer <= 0) {
            e.aiTimer = gRng.range(3, 9);
            float a = gRng.range(0, 6.28f), r = gRng.range(0, e.wanderRadius);
            e.moveTarget = e.home + V3(sinf(a) * r, 0, cosf(a) * r);
            if (gRng.chance(0.3f)) e.moveTarget = e.pos;
        }
        MoveTowards(e, e.moveTarget, e.speed * 0.3f, dt);
        return;
    }

    // --- combattimento ---
    if (d > 110) {
        e.aware = false;
        e.alert = 0;
        e.ai = AI_RETURN;
        return;
    }
    if (e.los) { e.lastSeen = P.pos; e.aiTimer = 0; }
    else e.aiTimer += dt;
    if (e.aiTimer > 12) {
        e.aware = false;
        e.alert = 0.5f;
        return;
    }
    // fuga dei predoni con poca resistenza agli impulsi
    if (e.kind == EK_HUMAN && e.ai != AI_FLEE && e.hp < e.maxHp * 0.2f && e.tag != "boss" && gRng.chance(dt * 0.5f)) {
        e.ai = AI_FLEE;
        e.stateFlee = 4.0f;
    }
    if (e.ai == AI_FLEE) {
        e.stateFlee -= dt;
        Vector3 away = e.pos + Vector3Normalize(V3(e.pos.x - P.pos.x, 0, e.pos.z - P.pos.z)) * 10;
        MoveTowards(e, away, e.speed, dt);
        if (e.stateFlee <= 0) e.ai = AI_COMBAT;
        return;
    }
    e.attackCd -= dt;
    if (UsesRanged(e)) {
        const WeaponDef& w = WEAPONS[ITEMS[e.weapon].weapon];
        float prefMin = w.range > 200 ? 14 : 7, prefMax = w.range > 200 ? 38 : (w.pellets > 1 ? 10 : 22);
        if (e.kind == EK_TURRET) {
            if (e.los) FacePlayer(e, dt);
        } else if (!e.los) {
            MoveTowards(e, e.lastSeen, e.speed * 0.8f, dt);
        } else {
            e.strafeTimer -= dt;
            if (e.strafeTimer <= 0) { e.strafeTimer = gRng.range(1.2f, 3.0f); e.strafe = gRng.chance(0.5f) ? 1.f : -1.f; }
            Vector3 toP = Vector3Normalize(V3(P.pos.x - e.pos.x, 0, P.pos.z - e.pos.z));
            Vector3 side = V3(toP.z, 0, -toP.x) * e.strafe;
            Vector3 goal;
            if (d > prefMax) goal = e.pos + toP * 3 + side;
            else if (d < prefMin) goal = e.pos - toP * 3 + side;
            else goal = e.pos + side * 2.5f;
            MoveTowards(e, goal, e.speed * (d > prefMax ? 0.9f : 0.5f), dt, false);
            FacePlayer(e, dt);
        }
        e.aimBlend = std::min(1.0f, e.aimBlend + dt * 4);
        if (e.reloadT > 0) {
            e.reloadT -= dt;
            if (e.reloadT <= 0) e.mag = w.mag;
        } else if (e.los && e.attackCd <= 0 && d < w.range && fabsf(AngleWrap(AngleTo(e.pos, P.pos) - e.yaw)) < 0.35f) {
            if (e.mag <= 0 && w.mag > 0) e.mag = w.mag;
            EntityShoot(e);
            e.mag--;
            if (w.automatic) {
                e.burst += 1;
                if (e.burst >= gRng.irange(3, 6)) { e.burst = 0; e.attackCd = gRng.range(0.9f, 1.6f); }
                else e.attackCd = w.fireDelay * 1.3f;
            } else e.attackCd = w.fireDelay * 2.2f + gRng.range(0.35f, 1.0f);
            if (e.mag <= 0) e.reloadT = w.reload * 1.2f;
        }
    } else {
        float reach = EntityReach(e);
        if (e.attackAnim > 0) {
            float dur = e.kind == EK_DEATHCLAW ? 0.75f : (e.kind == EK_ROACH ? 0.35f : 0.55f);
            e.attackAnim += dt / dur;
            FacePlayer(e, dt);
            if (e.attackAnim >= 0.5f && !e.attackHit) {
                e.attackHit = true;
                if (d < reach * 1.25f + 0.3f) {
                    float dmg = e.meleeDmg;
                    if (e.weapon >= 0) dmg = WEAPONS[ITEMS[e.weapon].weapon].damage * (e.kind == EK_MUTANT ? 0.9f : 0.7f);
                    PlayerTakeDamage(dmg * gRng.range(0.8f, 1.2f), e.pos);
                    PlaySfx3D(SFX_PUNCH, P.pos, 0.8f);
                } else PlaySfx3D(SFX_SWING, e.pos, 0.6f);
            }
            if (e.attackAnim >= 1) e.attackAnim = 0;
        } else if (d > reach * 0.85f) {
            Vector3 goal = e.los ? P.pos : e.lastSeen;
            MoveTowards(e, goal, e.speed, dt);
        } else {
            FacePlayer(e, dt);
            e.vel = V3(0, 0, 0);
            if (e.attackCd <= 0) {
                e.attackAnim = 0.001f;
                e.attackHit = false;
                e.attackCd = e.kind == EK_DEATHCLAW ? 1.1f : (e.kind == EK_ROACH ? 0.8f : 1.2f);
                if (gRng.chance(0.4f)) PlaySfx3D(VoiceOf(e), e.pos, 0.7f);
            }
        }
    }
}

static void FriendlyAI(Entity& e, float dt, float d) {
    if (G.state == GS_DIALOGUE && G.dlg.npc == e.id) {
        FacePlayer(e, dt);
        e.vel = V3(0, 0, 0);
        return;
    }
    if (d < 3.0f) {
        FacePlayer(e, dt);
        e.vel = V3(0, 0, 0);
        return;
    }
    e.aiTimer -= dt;
    if (e.aiTimer <= 0) {
        e.aiTimer = gRng.range(4, 11);
        float a = gRng.range(0, 6.28f), r = gRng.range(0, e.wanderRadius);
        e.moveTarget = e.home + V3(sinf(a) * r, 0, cosf(a) * r);
        if (gRng.chance(0.35f)) e.moveTarget = e.pos;
    }
    if (DistXZ(e.pos, e.moveTarget) > 0.4f) MoveTowards(e, e.moveTarget, 1.4f, dt);
    else e.vel = V3(0, 0, 0);
}

void EntitiesUpdate(float dt) {
    Player& P = G.player;
    bool combat = false;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        Entity& e = G.ents[i];
        if (e.zone != P.zone) continue;
        if (e.hitFlash > 0) e.hitFlash -= dt;
        if (e.voiceCd > 0) e.voiceCd -= dt;
        if (e.stunned) {
            // Questa e' solo l'animazione della caduta, mai un conto alla rovescia.
            e.stunAnimT = std::min(1.0f, e.stunAnimT + dt);
            e.vel = V3(0, 0, 0);
            e.moveAmt = 0;
            continue;
        }
        float d = Vector3Distance(e.pos, P.pos);
        if (d > 170 && !e.aware) continue;
        Vector3 before = e.pos;
        if (e.hostile) {
            HostileAI(e, dt, d, i);
            if (e.aware) combat = true;
        } else FriendlyAI(e, dt, d);
        if (e.kind != EK_TURRET) {
            ResolveCollision(e.zone, e.pos, EntityRadius(e), EntityHeight(e), 0.6f);
            float gy = GroundHeight(e.zone, V3(e.pos.x, e.pos.y + 0.3f, e.pos.z), 0.6f);
            if (gy < e.pos.y - 0.05f) e.pos.y = std::max(gy, e.pos.y - 9.0f * dt);
            else e.pos.y = gy;
        }
        // separazione tra entità
        for (int j = i + 1; j < (int)G.ents.size(); j++) {
            Entity& o = G.ents[j];
            if (o.stunned || o.zone != e.zone || o.kind == EK_TURRET || e.kind == EK_TURRET) continue;
            float rr = EntityRadius(e) + EntityRadius(o);
            float dx = o.pos.x - e.pos.x, dz = o.pos.z - e.pos.z;
            float dd = dx * dx + dz * dz;
            if (dd < rr * rr && dd > 1e-6f) {
                float dl = sqrtf(dd), push = (rr - dl) * 0.5f;
                e.pos.x -= dx / dl * push; e.pos.z -= dz / dl * push;
                o.pos.x += dx / dl * push; o.pos.z += dz / dl * push;
            }
        }
        // separazione dal giocatore
        {
            float rr = EntityRadius(e) + 0.35f;
            float dx = e.pos.x - P.pos.x, dz = e.pos.z - P.pos.z;
            float dd = dx * dx + dz * dz;
            if (dd < rr * rr && dd > 1e-6f && e.kind != EK_TURRET) {
                float dl = sqrtf(dd);
                e.pos.x = P.pos.x + dx / dl * rr;
                e.pos.z = P.pos.z + dz / dl * rr;
            }
        }
        float moved = DistXZ(before, e.pos) / std::max(dt, 1e-4f);
        e.moveAmt = LerpF(e.moveAmt, Clamp01(moved / 4.0f), Clamp01(dt * 8));
        e.anim += moved * dt * (e.kind == EK_ROACH ? 5.0f : (e.kind == EK_DEATHCLAW ? 1.4f : 2.6f)) / std::max(e.scale, 0.5f);
        if (e.glow) SpawnDynLight(e.zone, e.pos + V3(0, 1.4f, 0), Col(110, 255, 70), 7, 0.05f);
        if (!(e.hostile && e.aware)) e.aimBlend = std::max(0.0f, e.aimBlend - dt * 2);
    }
    G.inCombat = combat;
}

// ---------------------------------------------------------------------------
//  Modelli
// ---------------------------------------------------------------------------
static Matrix Basis(Vector3 f, Vector3 p) {
    Vector3 z = Vector3Normalize(f);
    Vector3 x = Vector3CrossProduct(V3(0, 1, 0), z);
    if (Vector3Length(x) < 1e-4f) x = V3(1, 0, 0);
    x = Vector3Normalize(x);
    Vector3 y = Vector3CrossProduct(z, x);
    return Matrix{x.x, y.x, z.x, p.x, x.y, y.y, z.y, p.y, x.z, y.z, z.z, p.z, 0, 0, 0, 1};
}

static void P(int prim, const Matrix& parent, Vector3 c, Vector3 s, Color col, float em = 0, Vector3 rot = V3(0, 0, 0)) {
    DrawPrim(prim, MatTRS(c, rot, s) * parent, col, em);
}

void DrawWeaponModel(const Matrix& m, int wm, float flash, float spin) {
    Color dark = Col(48, 48, 52), metal = Col(110, 112, 118), wood = Col(110, 70, 40), silver = Col(170, 172, 178);
    // Anche gli identificativi storici hanno soltanto l'aspetto di emettitori laser.
    if (wm == WM_PISTOL) wm = WM_LASERPISTOL;
    else if (wm == WM_RIFLE || wm == WM_SHOTGUN || wm == WM_ASSAULT) wm = WM_LASERRIFLE;
    switch (wm) {
        case WM_KNIFE: case WM_PIPE: case WM_BAT: case WM_SLEDGE: {
            float length = wm == WM_KNIFE ? 0.28f : (wm == WM_PIPE ? 0.55f : (wm == WM_BAT ? 0.72f : 0.85f));
            Color energy = wm == WM_BAT ? Col(100, 255, 150) : (wm == WM_SLEDGE ? Col(190, 130, 255) : Col(90, 230, 255));
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0, -0.12f), V3(0, 0, 0.13f), 0.027f) * m, dark);
            P(PR_CUBE, m, V3(0, 0, 0.10f), V3(0.075f, 0.055f, 0.05f), silver);
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0, 0.14f), V3(0, 0, length), 0.026f) * m, energy, 1.0f);
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0, 0.14f), V3(0, 0, length), 0.011f) * m, Col(220, 255, 255), 1.0f);
            break;
        }
        case WM_PISTOL:
            P(PR_CUBE, m, V3(0, -0.04f, -0.01f), V3(0.032f, 0.11f, 0.05f), Col(70, 50, 40), 0, V3(-0.25f, 0, 0));
            P(PR_CUBE, m, V3(0, 0.02f, 0.05f), V3(0.036f, 0.045f, 0.2f), dark);
            break;
        case WM_RIFLE:
            P(PR_CUBE, m, V3(0, -0.02f, -0.18f), V3(0.045f, 0.09f, 0.32f), wood);
            P(PR_CUBE, m, V3(0, 0.02f, 0.12f), V3(0.05f, 0.06f, 0.36f), wood);
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0.04f, 0.1f), V3(0, 0.04f, 0.75f), 0.013f) * m, dark);
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0.1f, 0.0f), V3(0, 0.1f, 0.3f), 0.022f) * m, dark);
            break;
        case WM_SHOTGUN:
            P(PR_CUBE, m, V3(0, -0.02f, -0.12f), V3(0.045f, 0.08f, 0.22f), wood);
            P(PR_CUBE, m, V3(0, 0.02f, 0.05f), V3(0.06f, 0.06f, 0.14f), metal);
            for (int s = -1; s <= 1; s += 2) DrawPrim(PR_CYL, MatAlignY(V3(s * 0.017f, 0.035f, 0.05f), V3(s * 0.017f, 0.035f, 0.45f), 0.017f) * m, dark);
            break;
        case WM_ASSAULT:
            P(PR_CUBE, m, V3(0, 0.02f, 0.08f), V3(0.05f, 0.08f, 0.38f), Col(60, 64, 56));
            P(PR_CUBE, m, V3(0, -0.07f, 0.12f), V3(0.03f, 0.13f, 0.05f), dark, 0, V3(0.25f, 0, 0));
            P(PR_CUBE, m, V3(0, -0.04f, -0.02f), V3(0.03f, 0.09f, 0.045f), dark, 0, V3(-0.25f, 0, 0));
            P(PR_CUBE, m, V3(0, 0.0f, -0.2f), V3(0.04f, 0.07f, 0.2f), Col(60, 64, 56));
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0.04f, 0.25f), V3(0, 0.04f, 0.55f), 0.012f) * m, dark);
            P(PR_CUBE, m, V3(0, 0.08f, 0.05f), V3(0.012f, 0.03f, 0.2f), dark);
            break;
        case WM_LASERPISTOL:
            P(PR_CUBE, m, V3(0, -0.04f, -0.01f), V3(0.03f, 0.1f, 0.05f), Col(60, 55, 50), 0, V3(-0.2f, 0, 0));
            P(PR_CUBE, m, V3(0, 0.025f, 0.06f), V3(0.045f, 0.06f, 0.22f), silver);
            P(PR_CUBE, m, V3(0, 0.025f, 0.18f), V3(0.03f, 0.03f, 0.04f), Col(255, 60, 40), 0.6f + flash);
            break;
        case WM_LASERRIFLE:
            P(PR_CUBE, m, V3(0, -0.02f, -0.2f), V3(0.045f, 0.08f, 0.25f), Col(70, 60, 50));
            P(PR_CUBE, m, V3(0, 0.02f, 0.15f), V3(0.06f, 0.08f, 0.5f), silver);
            for (int i = 0; i < 4; i++) DrawPrim(PR_CYL, MatTRS(V3(0, 0.02f, 0.05f + i * 0.08f), V3(PI / 2, 0, 0), V3(0.08f, 0.02f, 0.08f)) * m, Col(255, 70, 40), 0.5f + flash);
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0.02f, 0.4f), V3(0, 0.02f, 0.62f), 0.015f) * m, dark);
            break;
        case WM_PLASMA:
            P(PR_CUBE, m, V3(0, -0.02f, -0.2f), V3(0.045f, 0.08f, 0.25f), Col(60, 70, 60));
            P(PR_CUBE, m, V3(0, 0.02f, 0.12f), V3(0.07f, 0.09f, 0.42f), Col(90, 110, 95));
            DrawPrim(PR_CYL, MatTRS(V3(0, 0.1f, 0.1f), V3(PI / 2, 0, 0), V3(0.07f, 0.16f, 0.07f)) * m, Col(80, 255, 90), 0.7f + flash);
            DrawPrim(PR_CYL, MatAlignY(V3(0, 0.02f, 0.3f), V3(0, 0.02f, 0.55f), 0.025f) * m, dark);
            break;
        case WM_MINIGUN:
            P(PR_CUBE, m, V3(0, 0, 0), V3(0.16f, 0.16f, 0.3f), Col(70, 72, 70));
            for (int i = 0; i < 6; i++) {
                float a = spin + i * PI / 3;
                Vector3 o = V3(cosf(a) * 0.045f, sinf(a) * 0.045f, 0);
                DrawPrim(PR_CYL, MatAlignY(o + V3(0, 0, 0.1f), o + V3(0, 0, 0.75f), 0.013f) * m, dark);
                P(PR_SPHERE, m, o + V3(0, 0, 0.76f), V3(0.023f, 0.023f, 0.023f), Col(90, 230, 255), 0.8f + flash);
            }
            DrawPrim(PR_CYL, MatTRS(V3(0, 0, 0.6f), V3(PI / 2, 0, 0), V3(0.15f, 0.04f, 0.15f)) * m, metal);
            break;
        case WM_GRENADE:
            P(PR_SPHERE, m, V3(0, 0, 0), V3(0.07f, 0.09f, 0.07f), silver);
            P(PR_CUBE, m, V3(0, 0, 0.05f), V3(0.025f, 0.06f, 0.02f), Col(90, 230, 255), 0.9f);
            break;
        default: break;
    }
}

struct OutfitStyle { Color torso, pants, upper, lower, boots, extra; bool coat, plates, stripe, bareArms; float bulk; };

static OutfitStyle Style(const Look& L) {
    OutfitStyle s{L.shirt, L.pants, L.shirt, L.skin, Col(50, 40, 32), L.shirt, false, false, false, false, 1.0f};
    switch (L.outfit) {
        case OF_VAULT: s.torso = s.pants = s.upper = s.lower = Col(42, 72, 132); s.extra = Col(225, 185, 45); s.stripe = true; s.boots = Col(40, 40, 45); break;
        case OF_RAIDER: s.torso = Col(72, 56, 44); s.pants = Col(62, 52, 42); s.upper = L.skin; s.lower = L.skin; s.plates = true; s.extra = Col(90, 88, 84); s.bareArms = true; break;
        case OF_SHERIFF: s.torso = Col(112, 82, 56); s.pants = Col(70, 60, 50); s.upper = s.lower = Col(112, 82, 56); s.coat = true; s.extra = Col(220, 190, 60); break;
        case OF_DOCTOR: s.torso = s.upper = s.lower = Col(222, 222, 216); s.pants = Col(60, 60, 70); s.coat = true; s.extra = Col(200, 40, 40); break;
        case OF_GUARD: s.torso = Col(112, 112, 106); s.pants = Col(62, 70, 54); s.upper = Col(100, 100, 96); s.lower = Col(80, 76, 66); s.plates = true; s.extra = Col(130, 130, 125); break;
        case OF_TRADER: s.torso = Col(170, 150, 110); s.upper = s.lower = Col(170, 150, 110); s.pants = Col(80, 66, 50); s.extra = Col(110, 75, 45); break;
        case OF_GHOUL: s.torso = Col(62, 56, 46); s.pants = Col(56, 50, 42); s.upper = Col(62, 56, 46); s.lower = L.skin; break;
        case OF_MUTANT: s.torso = L.skin; s.pants = Col(92, 72, 52); s.upper = s.lower = L.skin; s.plates = true; s.extra = Col(80, 70, 60); s.bulk = 1.35f; s.bareArms = true; break;
        case OF_LEATHER: s.torso = Col(96, 70, 48); s.upper = s.lower = Col(96, 70, 48); s.pants = Col(72, 60, 48); break;
        case OF_COMBAT: s.torso = Col(84, 92, 68); s.upper = s.lower = Col(84, 92, 68); s.pants = Col(76, 82, 62); s.plates = true; s.extra = Col(70, 76, 58); break;
        case OF_POWER: s.torso = s.upper = s.lower = s.pants = Col(118, 120, 108); s.plates = true; s.extra = Col(95, 98, 88); s.bulk = 1.6f; s.boots = Col(80, 82, 76); break;
        case OF_RADSUIT: s.torso = s.upper = s.lower = s.pants = Col(205, 185, 70); s.boots = Col(50, 50, 50); break;
        case OF_BARMAN: s.torso = Col(30, 30, 34); s.upper = s.lower = Col(220, 215, 205); s.pants = Col(40, 40, 44); break;
        default: break;
    }
    return s;
}

Color OutfitColor(int outfit) {
    Look l;
    l.outfit = outfit;
    return Style(l).upper;
}

static void Leg(Vector3 hip, float a, float k, Vector3& knee, Vector3& foot, float len) {
    knee = hip + V3(0, -cosf(a), sinf(a)) * len;
    foot = knee + V3(0, -cosf(a - k), sinf(a - k)) * len;
}

void DrawHumanoid(Vector3 pos, float yaw, const Look& look, float anim, float moveAmt, float aim, float attack, float stunAnimT, int wm, float flash, float scale, bool glow, float crouch) {
    OutfitStyle st = Style(look);
    float em = glow ? 0.45f : 0.0f;
    float dk = Clamp01(stunAnimT / 0.7f);
    dk = 1 - (1 - dk) * (1 - dk);
    Matrix root = MatrixScale(scale, scale, scale) * MatrixRotateX(-dk * PI * 0.5f) * MatrixTranslate(0, dk * 0.12f * scale, 0) * MatrixRotateY(yaw) * MatrixTranslate(pos.x, pos.y, pos.z);
    float b = st.bulk;
    float ph = anim;
    float sw = sinf(ph) * 0.6f * moveAmt;
    float hipY = 0.93f - crouch * 0.38f - fabsf(sinf(ph)) * 0.04f * moveAmt;
    float hunch = look.outfit == OF_GHOUL ? 0.35f : (look.outfit == OF_MUTANT ? 0.12f : 0.0f);
    hunch += moveAmt * 0.08f + crouch * 0.25f;
    // gambe
    float hipW = 0.1f * b;
    for (int s = -1; s <= 1; s += 2) {
        float a = (s < 0 ? sw : -sw) + crouch * 1.0f;
        float k = std::max(0.0f, (s < 0 ? -cosf(ph) : cosf(ph))) * 0.8f * moveAmt + 0.05f + crouch * 1.9f;
        Vector3 hip = V3(s * hipW, hipY, 0), knee, foot;
        Leg(hip, a, k, knee, foot, 0.465f);
        DrawLimb(root, hip, knee, 0.078f * b, st.pants, em);
        DrawLimb(root, knee, foot, 0.066f * b, st.pants, em);
        P(PR_CUBE, root, foot + V3(0, 0.035f, 0.05f), V3(0.12f * b, 0.08f, 0.25f), st.boots, em);
    }
    // busto
    Matrix torso = MatrixRotateX(hunch) * MatrixTranslate(0, hipY, 0) * root;
    float tw = (look.female ? 0.36f : 0.41f) * b;
    P(PR_CUBE, torso, V3(0, 0.04f, 0), V3(0.34f * b, 0.2f, 0.21f * b), st.pants, em);
    P(PR_CUBE, torso, V3(0, 0.3f, 0), V3(tw, 0.42f, 0.23f * b), st.torso, em);
    if (st.stripe) {
        P(PR_CUBE, torso, V3(0, 0.3f, 0.118f * b), V3(0.06f, 0.42f, 0.01f), st.extra, em);
        P(PR_CUBE, torso, V3(0, 0.1f, 0), V3(0.35f * b, 0.05f, 0.235f * b), st.extra, em);
    }
    if (st.plates) {
        P(PR_CUBE, torso, V3(0, 0.33f, 0.04f), V3(tw * 0.9f, 0.3f, 0.2f * b), st.extra, em);
        for (int s = -1; s <= 1; s += 2) P(PR_CUBE, torso, V3(s * (tw * 0.5f + 0.02f), 0.5f, 0), V3(0.16f * b, 0.08f, 0.22f * b), st.extra, em, V3(0, 0, s * 0.3f));
    }
    if (st.coat) {
        P(PR_CUBE, torso, V3(0, -0.2f, 0), V3(tw * 1.02f, 0.5f, 0.25f * b), st.torso, em);
        if (look.outfit == OF_SHERIFF) P(PR_CUBE, torso, V3(-0.1f, 0.38f, 0.12f * b), V3(0.06f, 0.06f, 0.01f), st.extra, 0.2f);
        if (look.outfit == OF_DOCTOR) P(PR_CUBE, torso, V3(-0.1f, 0.4f, 0.12f * b), V3(0.05f, 0.05f, 0.01f), st.extra);
    }
    if (look.outfit == OF_TRADER) P(PR_CUBE, torso, V3(0, 0.1f, 0.12f * b), V3(0.3f, 0.5f, 0.02f), st.extra, em);
    if (look.outfit == OF_BARMAN) {
        P(PR_CUBE, torso, V3(0, 0.3f, 0.005f), V3(tw * 0.55f, 0.42f, 0.235f * b), Col(220, 215, 205), em);
    }
    // testa
    Vector3 head = V3(0, 0.69f + (b - 1) * 0.08f, 0.02f + hunch * 0.1f);
    DrawLimb(torso, V3(0, 0.5f, 0), head, 0.05f * b, look.skin, em);
    if (look.outfit == OF_POWER) {
        P(PR_CUBE, torso, head, V3(0.32f, 0.34f, 0.34f), st.torso, em);
        P(PR_CUBE, torso, head + V3(0, 0.03f, 0.17f), V3(0.24f, 0.05f, 0.02f), Col(255, 200, 80), 0.9f);
        P(PR_CYL, torso, head + V3(0, -0.12f, 0.16f), V3(0.1f, 0.08f, 0.1f), st.extra, em, V3(PI / 2, 0, 0));
    } else if (look.outfit == OF_RADSUIT) {
        P(PR_SPHERE, torso, head, V3(0.29f, 0.32f, 0.3f), st.torso, em);
        P(PR_CUBE, torso, head + V3(0, 0.02f, 0.13f), V3(0.18f, 0.1f, 0.03f), Col(60, 70, 60), 0.2f);
    } else {
        P(PR_SPHERE, torso, head, V3(0.22f, 0.26f, 0.24f), look.skin, em);
        Color eye = glow ? Col(150, 255, 90) : Col(25, 22, 20);
        for (int s = -1; s <= 1; s += 2) P(PR_CUBE, torso, head + V3(s * 0.045f, 0.02f, 0.112f), V3(0.035f, 0.022f, 0.01f), eye, glow ? 1.0f : 0.0f);
        P(PR_CUBE, torso, head + V3(0, -0.02f, 0.12f), V3(0.03f, 0.05f, 0.03f), ColShade(look.skin, 0.9f), em);
        switch (look.hairStyle) {
            case 1: P(PR_SPHERE, torso, head + V3(0, 0.05f, -0.015f), V3(0.235f, 0.18f, 0.25f), look.hair); break;
            case 2:
                P(PR_SPHERE, torso, head + V3(0, 0.05f, -0.015f), V3(0.24f, 0.19f, 0.255f), look.hair);
                P(PR_CUBE, torso, head + V3(0, -0.09f, -0.09f), V3(0.22f, 0.26f, 0.08f), look.hair);
                break;
            case 3: P(PR_CUBE, torso, head + V3(0, 0.13f, -0.01f), V3(0.045f, 0.1f, 0.26f), look.hair); break;
            default: break;
        }
        switch (look.hat) {
            case 1:
                P(PR_CYL, torso, head + V3(0, 0.1f, 0), V3(0.52f, 0.02f, 0.52f), Col(90, 66, 44));
                P(PR_CYL, torso, head + V3(0, 0.17f, 0), V3(0.25f, 0.14f, 0.27f), Col(90, 66, 44));
                break;
            case 2: P(PR_SPHERE, torso, head + V3(0, 0.07f, 0), V3(0.28f, 0.2f, 0.3f), Col(80, 90, 70)); break;
            case 3: P(PR_CYL, torso, head + V3(0, 0.07f, 0), V3(0.24f, 0.06f, 0.26f), Col(160, 40, 35)); break;
            default: break;
        }
    }
    // braccia
    bool twoHanded = (wm == WM_RIFLE || wm == WM_SHOTGUN || wm == WM_ASSAULT || wm == WM_LASERRIFLE || wm == WM_PLASMA || wm == WM_MINIGUN || wm == WM_PISTOL || wm == WM_LASERPISTOL);
    bool meleeW = wm == WM_KNIFE || wm == WM_PIPE || wm == WM_BAT || wm == WM_SLEDGE || wm == WM_FIST || wm < 0;
    Vector3 handR{}, elbowR{};
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sh = V3(s * (tw * 0.5f + 0.04f * b), 0.47f, 0);
        float as = (s < 0 ? -sw : sw) * 0.8f;
        Vector3 d1 = Vector3Normalize(V3(s * 0.12f, -cosf(as), sinf(as)));
        Vector3 d2 = Vector3Normalize(V3(s * 0.05f, -cosf(as + 0.35f), sinf(as + 0.35f)));
        if (aim > 0 && !meleeW && (s > 0 || twoHanded)) {
            Vector3 a1 = s > 0 ? Vector3Normalize(V3(0.05f, -0.25f, 0.97f)) : Vector3Normalize(V3(0.1f, -0.35f, 0.93f));
            Vector3 a2 = s > 0 ? Vector3Normalize(V3(-0.25f, 0.05f, 1.0f)) : Vector3Normalize(V3(0.55f, 0.05f, 0.85f));
            d1 = Vector3Normalize(Vector3Lerp(d1, a1, aim));
            d2 = Vector3Normalize(Vector3Lerp(d2, a2, aim));
        }
        if (s > 0 && attack > 0) {
            Vector3 up = Vector3Normalize(V3(0.35f, 0.75f, -0.2f)), down = Vector3Normalize(V3(-0.1f, -0.35f, 0.95f));
            float t = attack < 0.45f ? attack / 0.45f : 1.0f - (attack - 0.45f) / 0.55f;
            float k = attack < 0.45f ? t : 1.0f;
            Vector3 target = attack < 0.45f ? up : Vector3Lerp(down, d1, 1 - t);
            d1 = Vector3Normalize(Vector3Lerp(d1, target, k));
            d2 = Vector3Normalize(Vector3Lerp(d2, attack < 0.45f ? up : down, k));
        }
        if (s < 0 && attack > 0 && wm == WM_FIST && look.outfit != OF_MUTANT) {
            // pugni alternati non necessari: il braccio sinistro resta in guardia
            d1 = Vector3Normalize(V3(-0.1f, -0.4f, 0.9f));
            d2 = Vector3Normalize(V3(0.3f, 0.6f, 0.5f));
        }
        Vector3 elbow = sh + d1 * 0.3f, hand = elbow + d2 * 0.28f;
        DrawLimb(torso, sh, elbow, 0.058f * b, st.upper, em);
        DrawLimb(torso, elbow, hand, 0.05f * b, st.bareArms ? look.skin : st.lower, em);
        P(PR_SPHERE, torso, hand, V3(0.1f * b, 0.1f * b, 0.11f * b), look.outfit == OF_POWER ? st.extra : look.skin, em);
        if (s > 0) { handR = hand; elbowR = elbow; }
    }
    if (wm >= 0 && wm != WM_FIST) {
        Vector3 fwd = meleeW ? Vector3Normalize(Vector3Lerp(handR - elbowR, V3(0, 0.6f, 0.4f), 0.5f)) : Vector3Normalize(Vector3Lerp(V3(0, -0.6f, 0.8f), V3(0, 0, 1), aim));
        Matrix wmM = MatrixScale(1.0f / std::max(b, 1.0f) * b, b, b) * Basis(fwd, handR) * torso;
        DrawWeaponModel(wmM, wm, flash, (float)GetTime() * 20 * flash);
    }
}

static void DrawRoach(const Entity& e, float em) {
    float dk = Clamp01(e.stunAnimT / 0.4f);
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateZ(dk * PI) * MatrixTranslate(0, dk * 0.3f, 0) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color body = Col(96, 58, 38), shell = Col(72, 42, 28);
    P(PR_SPHERE, root, V3(0, 0.17f, 0), V3(0.38f, 0.17f, 0.64f), body, em);
    P(PR_SPHERE, root, V3(0, 0.22f, -0.04f), V3(0.36f, 0.13f, 0.6f), shell, em);
    P(PR_SPHERE, root, V3(0, 0.14f, 0.34f), V3(0.18f, 0.12f, 0.16f), body, em);
    for (int s = -1; s <= 1; s += 2) {
        float sway = sinf(G.time * 6 + s) * 0.1f;
        DrawLimb(root, V3(s * 0.04f, 0.17f, 0.4f), V3(s * 0.18f + sway, 0.3f, 0.85f), 0.008f, shell);
        for (int i = 0; i < 3; i++) {
            float z = 0.15f - i * 0.15f;
            float swing = sinf(e.anim * 2 + i * 2.1f + (s > 0 ? PI : 0)) * 0.13f * e.moveAmt;
            float curl = dk * 0.25f;
            Vector3 b0 = V3(s * 0.12f, 0.13f, z), k = V3(s * (0.33f - curl), 0.2f + curl, z + swing), f = V3(s * (0.44f - curl * 2), 0.0f + curl * 2, z + swing * 1.6f);
            DrawLimb(root, b0, k, 0.018f, body, em);
            DrawLimb(root, k, f, 0.014f, body, em);
        }
    }
}

static void DrawQuadruped(const Entity& e, float em) {
    bool rat = e.kind == EK_MOLERAT;
    float dk = Clamp01(e.stunAnimT / 0.5f);
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateZ(dk * PI * 0.5f) * MatrixTranslate(0, dk * (rat ? 0.3f : 0.2f), 0) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    float bite = e.attackAnim > 0 ? sinf(e.attackAnim * PI) : 0;
    if (rat) {
        Color skin = Col(152, 112, 96), dark = Col(120, 88, 74);
        P(PR_SPHERE, root, V3(0, 0.42f, 0), V3(0.62f, 0.48f, 1.05f), skin, em);
        P(PR_SPHERE, root, V3(0, 0.45f, 0.5f + bite * 0.15f), V3(0.42f, 0.34f, 0.44f), dark, em);
        for (int s = -1; s <= 1; s += 2) {
            P(PR_CUBE, root, V3(s * 0.04f, 0.33f, 0.74f + bite * 0.15f), V3(0.04f, 0.13f, 0.03f), Col(235, 225, 190));
            P(PR_SPHERE, root, V3(s * 0.12f, 0.52f, 0.66f + bite * 0.15f), V3(0.04f, 0.04f, 0.04f), Col(20, 20, 20));
        }
        for (int i = 0; i < 4; i++) {
            float sx = (i & 1) ? 0.22f : -0.22f, sz = (i & 2) ? 0.3f : -0.3f;
            float sw = sinf(e.anim * 2 + ((i == 0 || i == 3) ? 0 : PI)) * 0.15f * e.moveAmt;
            DrawLimb(root, V3(sx, 0.32f, sz), V3(sx * 1.2f, 0.0f, sz + sw), 0.06f, dark, em);
        }
        DrawLimb(root, V3(0, 0.35f, -0.5f), V3(sinf(G.time * 3) * 0.2f, 0.15f, -1.05f), 0.025f, skin, em);
    } else {
        Color fur = Col(110, 92, 72), dark = Col(80, 66, 52);
        P(PR_SPHERE, root, V3(0, 0.62f, -0.05f), V3(0.34f, 0.36f, 0.9f), fur, em);
        P(PR_SPHERE, root, V3(0, 0.66f, 0.27f), V3(0.36f, 0.4f, 0.42f), fur, em);
        float hb = bite * 0.12f;
        DrawLimb(root, V3(0, 0.75f, 0.35f), V3(0, 0.9f - hb, 0.52f + hb), 0.09f, fur, em);
        P(PR_CUBE, root, V3(0, 0.92f - hb, 0.56f + hb), V3(0.2f, 0.2f, 0.26f), fur, em);
        P(PR_CUBE, root, V3(0, 0.87f - hb, 0.74f + hb), V3(0.11f, 0.1f, 0.18f), dark, em);
        for (int s = -1; s <= 1; s += 2) {
            P(PR_CONE, root, V3(s * 0.07f, 1.07f - hb, 0.52f + hb), V3(0.06f, 0.12f, 0.04f), dark, em);
            P(PR_CUBE, root, V3(s * 0.06f, 0.96f - hb, 0.69f + hb), V3(0.03f, 0.03f, 0.01f), Col(30, 20, 15), 0.3f);
        }
        for (int i = 0; i < 4; i++) {
            float sx = (i & 1) ? 0.13f : -0.13f, sz = (i & 2) ? 0.3f : -0.33f;
            float sw = sinf(e.anim * 1.6f + ((i == 0 || i == 3) ? 0 : PI)) * 0.4f * e.moveAmt;
            Vector3 hip = V3(sx, 0.55f, sz), knee = hip + V3(0, -0.28f, sw * 0.28f), foot = knee + V3(0, -0.27f, sw * 0.1f - 0.04f);
            DrawLimb(root, hip, knee, 0.05f, fur, em);
            DrawLimb(root, knee, foot, 0.04f, dark, em);
        }
        float wag = sinf(G.time * 8) * 0.2f;
        DrawLimb(root, V3(0, 0.7f, -0.42f), V3(wag, 0.85f, -0.75f), 0.03f, fur, em);
    }
}

static void DrawRobot(const Entity& e, float em) {
    float dk = Clamp01(e.stunAnimT / 0.7f);
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateX(-dk * PI * 0.5f) * MatrixTranslate(0, dk * 0.4f, 0) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color body = e.tag == "mk5" ? Col(150, 70, 50) : Col(112, 118, 96), dark = Col(60, 62, 58), metal = Col(140, 142, 140);
    float sw = sinf(e.anim) * 0.4f * e.moveAmt;
    for (int s = -1; s <= 1; s += 2) {
        float a = s < 0 ? sw : -sw;
        Vector3 hip = V3(s * 0.24f, 0.85f, 0);
        Vector3 foot = hip + V3(0, -0.8f * cosf(a), 0.8f * sinf(a));
        DrawLimb(root, hip, foot, 0.11f, dark, em);
        P(PR_CUBE, root, foot + V3(0, 0.05f, 0.07f), V3(0.24f, 0.12f, 0.36f), metal, em);
    }
    P(PR_CYL, root, V3(0, 1.25f, 0), V3(0.85f, 0.85f, 0.7f), body, em);
    P(PR_CUBE, root, V3(0, 1.3f, 0.3f), V3(0.5f, 0.45f, 0.12f), dark, em);
    for (int i = 0; i < 3; i++) P(PR_CUBE, root, V3(-0.15f + i * 0.15f, 1.38f, 0.37f), V3(0.08f, 0.05f, 0.02f), i == 1 ? Col(255, 200, 60) : Col(80, 255, 120), 0.9f);
    P(PR_SPHERE, root, V3(0, 1.85f, 0), V3(0.55f, 0.45f, 0.55f), metal, em);
    Color eye = e.aware ? Col(255, 50, 40) : Col(80, 200, 255);
    if (!e.stunned) P(PR_CUBE, root, V3(0, 1.88f, 0.23f), V3(0.36f, 0.08f, 0.05f), eye, 1.0f);
    DrawLimb(root, V3(0, 2.05f, 0), V3(0.1f, 2.35f, -0.05f), 0.015f, dark);
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sh = V3(s * 0.48f, 1.55f, 0);
        Vector3 el = sh + Vector3Lerp(V3(s * 0.05f, -0.4f, 0.1f), V3(s * 0.02f, -0.15f, 0.35f), e.aimBlend);
        Vector3 hand = el + Vector3Lerp(V3(0, -0.35f, 0.1f), V3(-s * 0.05f, 0.02f, 0.4f), e.aimBlend);
        DrawLimb(root, sh, el, 0.08f, dark, em);
        DrawLimb(root, el, hand, 0.07f, metal, em);
        DrawPrim(PR_CYL, MatAlignY(hand, hand + Vector3Normalize(hand - el) * 0.15f, 0.06f) * root, Col(255, 60, 40), e.attackCd > 0.6f && e.aware ? 0.9f : 0.2f);
    }
}

static void DrawTurret(const Entity& e, float em) {
    Matrix base = MatrixScale(e.scale, e.scale, e.scale) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color metal = Col(110, 112, 104), dark = Col(55, 57, 54);
    P(PR_CYL, base, V3(0, 0.3f, 0), V3(0.9f, 0.6f, 0.9f), dark, em);
    P(PR_CYL, base, V3(0, 0.8f, 0), V3(0.3f, 0.5f, 0.3f), metal, em);
    float dk = Clamp01(e.stunAnimT / 0.5f);
    Matrix head = MatrixRotateX(dk * 0.6f) * MatrixRotateY(e.yaw) * MatrixTranslate(0, 1.2f, 0) * base;
    P(PR_CUBE, head, V3(0, 0, 0), V3(0.6f, 0.42f, 0.7f), metal, em);
    for (int s = -1; s <= 1; s += 2) DrawPrim(PR_CYL, MatAlignY(V3(s * 0.13f, 0, 0.3f), V3(s * 0.13f, 0, 0.9f), 0.045f) * head, dark, em);
    if (!e.stunned) P(PR_CUBE, head, V3(0, 0.12f, 0.36f), V3(0.2f, 0.06f, 0.02f), e.aware ? Col(255, 40, 30) : Col(80, 255, 120), 1.0f);
}

static void DrawDeathclaw(const Entity& e, float em) {
    float dk = Clamp01(e.stunAnimT / 0.9f);
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateZ(dk * PI * 0.5f) * MatrixTranslate(0, dk * 0.8f, 0) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color skin = Col(72, 62, 50), belly = Col(110, 96, 74), bone = Col(220, 210, 185);
    float sw = sinf(e.anim) * 0.5f * e.moveAmt;
    float breathe = sinf(G.time * 2) * 0.02f;
    for (int s = -1; s <= 1; s += 2) {
        float a = s < 0 ? sw : -sw;
        Vector3 hip = V3(s * 0.38f, 1.5f, -0.2f);
        Vector3 knee = hip + V3(s * 0.04f, -0.55f * cosf(a), 0.45f + 0.5f * sinf(a));
        Vector3 ankle = knee + V3(0, -0.62f, -0.35f + sinf(a) * 0.2f);
        Vector3 toe = ankle + V3(0, -0.3f, 0.4f);
        DrawLimb(root, hip, knee, 0.2f, skin, em);
        DrawLimb(root, knee, ankle, 0.13f, skin, em);
        DrawLimb(root, ankle, toe, 0.1f, skin, em);
        for (int c = -1; c <= 1; c++) DrawPrim(PR_CONE, MatAlignY(toe + V3(c * 0.08f, 0, 0), toe + V3(c * 0.1f, -0.05f, 0.22f), 0.035f) * root, bone, em);
    }
    P(PR_SPHERE, root, V3(0, 1.75f + breathe, 0.15f), V3(1.15f, 1.05f, 1.45f), skin, em, V3(-0.35f, 0, 0));
    P(PR_SPHERE, root, V3(0, 1.95f + breathe, 0.65f), V3(1.05f, 0.95f, 0.85f), skin, em);
    P(PR_SPHERE, root, V3(0, 1.62f, 0.75f), V3(0.8f, 0.7f, 0.6f), belly, em);
    for (int i = 0; i < 4; i++) P(PR_CONE, root, V3(0, 2.4f - i * 0.15f, 0.4f - i * 0.35f), V3(0.12f, 0.28f, 0.12f), bone, em, V3(-0.6f, 0, 0));
    // testa
    float atk = e.attackAnim > 0 ? sinf(e.attackAnim * PI) : 0;
    Vector3 hd = V3(0, 2.45f - atk * 0.2f, 1.15f + atk * 0.15f);
    P(PR_SPHERE, root, hd, V3(0.52f, 0.46f, 0.78f), skin, em);
    P(PR_SPHERE, root, hd + V3(0, -0.15f, 0.12f), V3(0.42f, 0.2f, 0.62f), belly, em);
    for (int s = -1; s <= 1; s += 2) {
        DrawPrim(PR_CONE, MatAlignY(hd + V3(s * 0.18f, 0.18f, -0.05f), hd + V3(s * 0.38f, 0.65f, -0.35f), 0.07f) * root, bone, em);
        P(PR_SPHERE, root, hd + V3(s * 0.17f, 0.08f, 0.28f), V3(0.07f, 0.06f, 0.06f), Col(255, 200, 60), 0.8f);
    }
    // braccia con artigli
    for (int s = -1; s <= 1; s += 2) {
        float raise = (s > 0 ? atk : atk * 0.7f);
        Vector3 sh = V3(s * 0.6f, 2.2f, 0.7f);
        Vector3 el = sh + Vector3Lerp(V3(s * 0.25f, -0.6f, 0.25f), V3(s * 0.3f, 0.5f, 0.2f), raise);
        Vector3 hand = el + Vector3Lerp(V3(-s * 0.1f, -0.5f, 0.4f), V3(-s * 0.15f, 0.2f, 0.7f), raise);
        DrawLimb(root, sh, el, 0.14f, skin, em);
        DrawLimb(root, el, hand, 0.11f, skin, em);
        Vector3 dir = Vector3Normalize(hand - el);
        for (int c = -1; c <= 1; c++) {
            Vector3 off = V3(c * 0.07f, 0, 0);
            DrawPrim(PR_CONE, MatAlignY(hand + off, hand + off + dir * 0.5f + V3(0, -0.12f, 0), 0.045f) * root, bone, em);
        }
    }
    // coda
    Vector3 t0 = V3(0, 1.5f, -0.6f);
    for (int i = 0; i < 3; i++) {
        float ww = sinf(G.time * 2.5f + i) * 0.25f * (i + 1);
        Vector3 t1 = t0 + V3(ww, -0.3f, -0.75f);
        DrawLimb(root, t0, t1, 0.2f - i * 0.05f, skin, em);
        t0 = t1;
    }
}

void DrawEntity(const Entity& e) {
    float em = e.hitFlash > 0 ? 0.5f : 0.0f;
    if (e.glow) em = std::max(em, 0.35f + 0.1f * sinf(G.time * 4));
    switch (e.kind) {
        case EK_ROACH: DrawRoach(e, em); break;
        case EK_MOLERAT: case EK_DOG: DrawQuadruped(e, em); break;
        case EK_ROBOT: DrawRobot(e, em); break;
        case EK_TURRET: DrawTurret(e, em); break;
        case EK_DEATHCLAW: DrawDeathclaw(e, em); break;
        default: {
            int wm = e.weapon >= 0 ? WEAPONS[ITEMS[e.weapon].weapon].model : WM_FIST;
            float aim = e.stunned ? 0 : e.aimBlend;
            float flash = 0;
            Look look = e.look;
            if (e.glow) look.skin = Col(150, 200, 90);
            Vector3 position = e.pos;
            if (e.stunned) position.y += 0.008f * sinf(G.time * 2.4f + e.id);
            DrawHumanoid(position, e.yaw, look, e.anim, e.stunned ? 0 : e.moveAmt, aim, e.attackAnim, e.stunned ? e.stunAnimT : 0, wm, flash, e.scale, e.glow, 0);
            break;
        }
    }
    if (e.stunned) {
        // Un piccolo segnale di stasi distingue l'avversario incosciente da un corpo senza vita.
        Vector3 center = e.pos + V3(0, e.kind == EK_TURRET ? 0.7f : 0.35f * e.scale, 0);
        for (int i = 0; i < 3; i++) {
            float angle = G.time * 1.8f + i * 2 * PI / 3;
            P(PR_SPHERE, MatrixIdentity(), center + V3(cosf(angle) * 0.3f, 0.1f + sinf(angle * 2) * 0.035f, sinf(angle) * 0.3f), V3(0.07f, 0.07f, 0.07f), Col(100, 235, 255), 1.0f);
        }
    }
}

void EntitiesDraw(Camera3D cam) {
    Vector3 fwd = Vector3Normalize(cam.target - cam.position);
    for (auto& e : G.ents) {
        if (e.zone != G.player.zone) continue;
        Vector3 to = e.pos - cam.position;
        float d = Vector3Length(to);
        if (d > 150) continue;
        if (d > 4 && Vector3DotProduct(to, fwd) < -3) continue;
        DrawEntity(e);
    }
}

void EntitiesDrawShadows(Camera3D cam) {
    for (auto& e : G.ents) {
        if (e.zone != G.player.zone || e.kind == EK_TURRET) continue;
        if (Vector3Distance(e.pos, cam.position) > 50) continue;
        float r = EntityRadius(e) * (e.stunned ? 1.8f : 1.2f);
        DrawCylinder(e.pos + V3(0, 0.03f, 0), r, r, 0.01f, 10, Color{0, 0, 0, 70});
    }
}
