// Entità: creature, guardiani e personaggi. Nessuno combatte: i guardiani vedono, sentono,
// inseguono e scacciano il giocatore; le esche e i rumori li distraggono.
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
    e.scanBase = yaw;
    e.anim = gRng.range(0, 6);
    e.strafe = gRng.chance(0.5f) ? 1.f : -1.f;
    e.aiTimer = gRng.range(0, 4);
    switch (k) {
        case EK_HUMAN:
            e.name = "Abitante";
            e.speed = 5.0f; e.detect = 26; e.territory = 40;
            break;
        case EK_ROACH:
            e.name = "Radscarafaggio"; e.photoKey = "roach";
            e.speed = 5.0f; e.detect = 14; e.territory = 22; e.wanderRadius = 6;
            e.fac = FAC_CREATURE; e.hostile = true; e.animal = true;
            break;
        case EK_MOLERAT:
            e.name = "Ratto talpa"; e.photoKey = "molerat";
            e.speed = 4.8f; e.detect = 18; e.territory = 24; e.wanderRadius = 8;
            e.fac = FAC_CREATURE; e.hostile = true; e.animal = true;
            break;
        case EK_DOG:
            e.name = "Cane selvatico"; e.photoKey = "dog";
            e.speed = 6.9f; e.detect = 24; e.territory = 26; e.wanderRadius = 12;
            e.fac = FAC_CREATURE; e.hostile = true; e.animal = true;
            break;
        case EK_GHOUL:
            e.name = "Ghoul sonnambulo"; e.photoKey = "ghoul";
            e.speed = 3.2f; e.detect = 12; e.territory = 28; e.wanderRadius = 10;
            e.fac = FAC_CREATURE; e.hostile = true;
            e.look.outfit = OF_GHOUL;
            e.look.skin = Col(122, 106, 76);
            e.look.hairStyle = 0;
            break;
        case EK_MUTANT:
            e.name = "Mutante"; e.photoKey = "mutant";
            e.speed = 4.4f; e.detect = 28; e.territory = 30; e.scale = 1.35f;
            e.fac = FAC_MUTANT; e.hostile = true; e.wanderRadius = 8;
            e.look.outfit = OF_MUTANT;
            e.look.skin = Col(122, 134, 72);
            e.look.hairStyle = 0;
            break;
        case EK_ROBOT:
            e.name = "Sentinella"; e.photoKey = "robot";
            e.speed = 3.6f; e.detect = 26; e.territory = 45;
            e.fac = FAC_ROBOT; e.hostile = true; e.wanderRadius = 5;
            break;
        case EK_CAMERA:
            e.name = "Telecamera di sicurezza"; e.photoKey = "camera";
            e.speed = 0; e.detect = 24; e.territory = 0;
            e.fac = FAC_ROBOT; e.hostile = true; e.wanderRadius = 0;
            break;
        case EK_DEATHCLAW:
            e.name = "Squarciatore"; e.photoKey = "deathclaw";
            e.speed = 7.4f; e.detect = 30; e.territory = 34; e.asleep = true;
            e.fac = FAC_CREATURE; e.hostile = true; e.animal = true; e.wanderRadius = 0;
            break;
        default: break;
    }
    return e;
}

float EntityReach(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return 1.2f;
        case EK_MOLERAT: case EK_DOG: case EK_GHOUL: return 1.6f;
        case EK_MUTANT: return 2.1f;
        case EK_ROBOT: return 1.8f * e.scale;
        case EK_CAMERA: return 0;
        case EK_DEATHCLAW: return 3.0f;
        default: return 1.6f * e.scale;
    }
}

float EntityRadius(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return 0.35f;
        case EK_MOLERAT: return 0.5f;
        case EK_DOG: return 0.45f;
        case EK_DEATHCLAW: return 1.0f;
        case EK_CAMERA: return 0.4f;
        case EK_ROBOT: return 0.5f * e.scale;
        default: return 0.35f * e.scale;
    }
}

static float EntityHeight(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return 0.4f;
        case EK_MOLERAT: return 0.9f;
        case EK_DOG: return 1.0f;
        case EK_DEATHCLAW: return e.asleep ? 1.4f : 3.0f;
        case EK_CAMERA: return 2.6f;
        case EK_ROBOT: return 2.1f * e.scale;
        default: return 1.8f * e.scale;
    }
}

Vector3 EntityCenter(const Entity& e) {
    if (e.kind == EK_CAMERA) return e.pos + V3(0, 2.4f, 0);
    return e.pos + V3(0, EntityHeight(e) * 0.55f, 0);
}

Entity* FindEntityByTag(const std::string& tag) {
    for (auto& e : G.ents)
        if (e.tag == tag) return &e;
    return nullptr;
}

const char* EntityState(const Entity& e) {
    if (e.off) return "Spento";
    if (e.stunT > 0) return e.kind == EK_ROBOT || e.kind == EK_CAMERA ? "In sovraccarico" : "Stordito";
    if (e.companion) return "Ti segue";
    if (!e.hostile) return "Tranquillo";
    if (e.asleep) return "Dorme";
    if (e.ai == AI_DISTRACT) return "Distratto dall'esca";
    if (e.ai == AI_SEARCH) return "Indaga su un rumore";
    if (e.aware) return "Ti insegue!";
    if (e.alert > 0.35f) return "Sospettoso";
    return "Ignaro";
}

const char* EntityHint(const Entity& e) {
    if (e.stunT > 0 && !e.off) return "Fuori combattimento: si riprende tra pochi secondi.";
    switch (e.kind) {
        case EK_ROACH: return "Veloce ma miope. Le esche lo distraggono.";
        case EK_MOLERAT: return "Difende la tana. Un'esca lo tiene occupato.";
        case EK_DOG: return "Corre molto. Adora la carne: lancia un'esca.";
        case EK_GHOUL: return "Lento e confuso: basta non farsi toccare.";
        case EK_MUTANT: return "Ci vede bene. Distrailo con un barattolo.";
        case EK_ROBOT: return "Si spegne dal terminale di sicurezza.";
        case EK_CAMERA: return "Evita il suo cono di luce.";
        case EK_DEATHCLAW: return "Ha il sonno leggero: niente corse vicino a lui!";
        default: return e.fac == FAC_RAIDER ? "Con un lasciapassare ti lasciano in pace." : "";
    }
}

// ---------------------------------------------------------------------------
//  Volumi per mirare con la fotocamera e osservare
// ---------------------------------------------------------------------------
struct HitSphere { Vector3 c; float r; };

static int HitSpheres(const Entity& e, HitSphere* out) {
    int n = 0;
    auto A = [&](float x, float y, float z, float r) { out[n++] = {V3(x, y, z), r}; };
    switch (e.kind) {
        case EK_ROACH: A(0, 0.16f, 0, 0.35f); break;
        case EK_MOLERAT: A(0, 0.42f, 0, 0.5f); A(0, 0.45f, 0.55f, 0.26f); break;
        case EK_DOG: A(0, 0.6f, 0, 0.4f); A(0, 0.9f, 0.55f, 0.2f); break;
        case EK_ROBOT: A(0, 1.25f, 0, 0.5f); A(0, 1.85f, 0, 0.32f); A(0, 0.5f, 0, 0.35f); break;
        case EK_CAMERA: A(0, 2.4f, 0.1f, 0.35f); A(0, 1.2f, 0, 0.15f); break;
        case EK_DEATHCLAW:
            if (e.asleep) { A(0, 0.8f, 0, 1.0f); A(0, 0.8f, 1.2f, 0.6f); }
            else { A(0, 1.8f, 0.1f, 0.8f); A(0, 2.5f, 1.2f, 0.4f); A(0, 0.8f, 0, 0.6f); }
            break;
        default: A(0, 1.55f, 0, 0.2f); A(0, 1.15f, 0, 0.3f); A(0, 0.5f, 0, 0.25f); break;
    }
    Matrix m = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    for (int i = 0; i < n; i++) {
        out[i].c = Vector3Transform(out[i].c, m);
        out[i].r *= e.scale;
    }
    return n;
}

bool RaycastEntities(int zone, Vector3 o, Vector3 dir, float maxT, RayHit& hit, int ignore) {
    bool found = false;
    float best = maxT;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        const Entity& e = G.ents[i];
        if (e.zone != zone || i == ignore) continue;
        Vector3 oc = EntityCenter(e) - o;
        float tc = Vector3DotProduct(oc, dir);
        if (tc < -3 || tc > best + 3) continue;
        HitSphere hs[6]{};
        int n = HitSpheres(e, hs);
        for (int k = 0; k < n; k++) {
            Vector3 L = hs[k].c - o;
            float t = Vector3DotProduct(L, dir);
            float dd = Vector3LengthSqr(L) - t * t;
            float r2 = hs[k].r * hs[k].r;
            if (dd > r2) continue;
            float t0 = t - sqrtf(r2 - dd);
            if (t0 < 0 || t0 >= best) continue;
            best = t0;
            found = true;
            hit.hit = true;
            hit.t = t0;
            hit.point = o + dir * t0;
            hit.entity = i;
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
//  Distrazioni
// ---------------------------------------------------------------------------
static SfxId VoiceOf(const Entity& e) {
    switch (e.kind) {
        case EK_ROACH: return SFX_ROACH;
        case EK_MOLERAT: return SFX_GROWL;
        case EK_DOG: return e.companion ? SFX_BARK : SFX_DOG;
        case EK_GHOUL: return SFX_GHOUL;
        case EK_MUTANT: return SFX_MUTANT;
        case EK_ROBOT: case EK_CAMERA: return SFX_ROBOT;
        case EK_DEATHCLAW: return SFX_ROAR;
        default: return SFX_UI_BACK;
    }
}

void BaitAt(int zone, Vector3 p) {
    float range = 22.0f + SkillValue(SK_NATURE) * 0.1f;
    float dur = 12.0f + SkillValue(SK_NATURE) * 0.12f;
    if (G.player.perks[PK_ANIMALFRIEND]) dur *= 1.5f;
    bool any = false;
    for (auto& e : G.ents) {
        if (e.zone != zone || !e.animal || !e.hostile || e.asleep || e.companion) continue;
        if (Vector3Distance(e.pos, p) > range) continue;
        e.ai = AI_DISTRACT;
        e.distractPos = p;
        e.distractT = dur;
        e.aware = false;
        e.alert = 0;
        any = true;
    }
    if (any) {
        G.player.distractions++;
        Notify("Le bestie fiutano l'esca e corrono a mangiarla!", Col(200, 255, 170));
    }
}

void NoiseAt(int zone, Vector3 p, float radius, bool byPlayer) {
    bool any = false;
    for (auto& e : G.ents) {
        if (e.zone != zone || !e.hostile || e.off || e.kind == EK_CAMERA || e.companion) continue;
        if (Vector3Distance(e.pos, p) > radius) continue;
        if (e.asleep) {
            e.alert += 0.5f;
            continue;
        }
        if (e.animal && e.kind != EK_DOG) continue; // gli animali badano solo al cibo
        if (e.ai == AI_DISTRACT) continue;
        e.ai = AI_SEARCH;
        e.distractPos = p;
        e.distractT = 7.0f + gRng.range(0, 3);
        e.aware = false;
        e.alert = 0.45f;
        any = true;
    }
    if (any && byPlayer) {
        G.player.distractions++;
        Notify("Qualcuno va a controllare il rumore...", Col(200, 255, 170));
    }
}

void SecurityOff(int zone, bool includeBoss) {
    int n = 0;
    for (auto& e : G.ents)
        if (e.zone == zone && (e.kind == EK_ROBOT || e.kind == EK_CAMERA) && !e.off && (includeBoss || e.tag != "mk5")) {
            e.off = true;
            e.aware = false;
            e.ai = AI_IDLE;
            n++;
        }
    if (n > 0) {
        Notify(Fmt("Sistema di sicurezza disattivato: %d dispositivi spenti.", n), Col(150, 255, 200));
        PlaySfx(SFX_ROBOT, 0.7f, 0.6f);
    }
}

// ---------------------------------------------------------------------------
//  Movimento
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
    e.vel = YawDir(chosen) * speed;
    e.pos = e.pos + e.vel * dt;
    if (face) e.yaw += AngleWrap(chosen - e.yaw) * Clamp01(dt * 8);
    return false;
}

static void FacePoint(Entity& e, Vector3 p, float dt) {
    float a = AngleTo(e.pos, p);
    e.yaw += AngleWrap(a - e.yaw) * Clamp01(dt * 6.0f);
}

static Vector3 EntityEye(const Entity& e) {
    if (e.kind == EK_CAMERA) return e.pos + V3(0, 2.35f, 0) + YawDir(e.yaw) * 0.4f;
    return e.pos + V3(0, EntityHeight(e) * 0.85f, 0);
}

// ---------------------------------------------------------------------------
//  Intelligenza artificiale
// ---------------------------------------------------------------------------
static float DetectionRange(const Entity& e) {
    const Player& P = G.player;
    if (P.stealthT > 0 || P.catchImmune > 0) return 0;
    float r = e.detect;
    if (P.crouch) r *= 0.45f;
    r *= 1.0f - SkillValue(SK_SNEAK) / 220.0f;
    if (e.animal) {
        r *= 1.0f - SkillValue(SK_NATURE) / 300.0f;
        if (P.perks[PK_ANIMALFRIEND]) r *= 0.6f;
    }
    if (W.zones[e.zone].outdoor && R.night > 0.5f) {
        if (P.pipLight) r *= 1.25f;
        else r *= P.perks[PK_SHADOW] ? 0.55f : 0.72f;
    }
    if (e.kind == EK_GHOUL) r *= 1.0f;
    return r;
}

static void Alarm(Entity& src) {
    PlaySfx3D(SFX_ALARM, src.pos, 1.0f, 0);
    for (auto& o : G.ents)
        if (&o != &src && o.zone == src.zone && o.kind == EK_ROBOT && !o.off && Vector3Distance(o.pos, src.pos) < 55) {
            o.aware = true;
            o.ai = AI_CHASE;
            o.lastSeen = G.player.pos;
            o.lostT = 0;
        }
}

static void BecomeAware(Entity& e) {
    e.aware = true;
    e.ai = AI_CHASE;
    e.lastSeen = G.player.pos;
    e.lostT = 0;
    if (e.voiceCd <= 0) {
        PlaySfx3D(VoiceOf(e), e.pos, 1.0f);
        e.voiceCd = 2;
    }
    if (e.kind == EK_CAMERA) {
        Alarm(e);
        Notify("ALLARME! Una telecamera ti ha visto!", Col(255, 120, 100));
        return;
    }
    for (auto& o : G.ents)
        if (&o != &e && o.hostile && !o.off && !o.asleep && o.zone == e.zone && o.fac == e.fac && Vector3Distance(o.pos, e.pos) < 20)
            o.alert = std::max(o.alert, 0.8f);
}

static void GuardAI(Entity& e, float dt, float d) {
    Player& P = G.player;
    e.losTimer -= dt;
    if (e.losTimer <= 0) {
        e.losTimer = 0.2f + gRng.range(0, 0.15f);
        e.los = d < std::max(e.detect * 1.6f, 30.0f) && LineOfSight(e.zone, EntityEye(e), PlayerEye());
    }
    // distratti da un'esca o da un rumore
    if (e.ai == AI_DISTRACT || e.ai == AI_SEARCH) {
        e.distractT -= dt;
        bool there = MoveTowards(e, e.distractPos, e.ai == AI_DISTRACT ? e.speed * 0.8f : e.speed * 0.45f, dt) || DistXZ(e.pos, e.distractPos) < 1.5f;
        if (there) {
            e.vel = V3(0, 0, 0);
            e.actionAnim += dt;
            if (e.ai == AI_SEARCH) e.yaw += sinf(G.time * 1.5f + e.id) * dt * 1.5f;
            else if (gRng.chance(dt * 0.7f)) PlaySfx3D(SFX_SNIFF, e.pos, 0.5f);
        }
        // chi indaga può comunque notarti se gli passi davanti
        if (e.ai == AI_SEARCH && e.los && d < DetectionRange(e) * 0.6f) {
            e.alert += dt * 1.5f;
            if (e.alert >= 1) BecomeAware(e);
        }
        if (e.distractT <= 0 && e.ai != AI_CHASE) {
            e.ai = AI_RETURN;
            e.actionAnim = 0;
        }
        return;
    }
    // lo Squarciatore dorme
    if (e.asleep) {
        float hearing = P.noise * (1.0f - SkillValue(SK_SNEAK) / 300.0f);
        if (P.stealthT <= 0 && P.catchImmune <= 0) {
            if (hearing > 0 && d < hearing * 0.75f) e.alert += dt * 1.4f;
            if (!P.crouch && d < 5.0f && e.los) e.alert += dt * 0.8f;
        }
        e.alert = std::max(0.0f, e.alert - dt * 0.08f);
        if (e.voiceCd <= 0) {
            PlaySfx3D(SFX_SNORE, e.pos, 0.8f, 0.05f);
            e.voiceCd = 3.2f;
        }
        if (e.alert >= 1) {
            e.asleep = false;
            BecomeAware(e);
            Notify("Lo Squarciatore si è svegliato!", Col(255, 140, 100));
        }
        return;
    }
    if (!e.aware) {
        float range = DetectionRange(e);
        float fov = e.kind == EK_CAMERA ? 0.4f : 1.15f;
        float ang = fabsf(AngleWrap(AngleTo(e.pos, P.pos) - e.yaw));
        bool inFov = ang < fov || (d < 3.5f && e.kind != EK_CAMERA);
        bool seen = d < range && e.los && inFov;
        float hearing = P.noise * (1.0f - SkillValue(SK_SNEAK) / 300.0f);
        bool heard = e.kind != EK_CAMERA && P.catchImmune <= 0 && P.stealthT <= 0 && hearing > 0 && d < hearing;
        if (seen) e.alert += dt * (1.7f - d / std::max(range, 1.0f)) * (e.kind == EK_CAMERA ? 2.2f : 1.4f);
        else if (heard) e.alert += dt * 1.2f;
        else e.alert = std::max(0.0f, e.alert - dt * 0.22f);
        if (seen || heard) e.lastSeen = P.pos;
        if (e.alert >= 1.0f) {
            BecomeAware(e);
            return;
        }
        if (e.kind == EK_CAMERA) {
            e.yaw = e.scanBase + sinf(G.time * 0.45f + e.id) * 0.9f;
            if (e.alert > 0.35f) FacePoint(e, P.pos, dt * 0.3f);
            return;
        }
        if (e.alert > 0.35f) {
            // sospettoso: si gira e fa qualche passo verso il punto sospetto
            FacePoint(e, e.lastSeen, dt);
            if (DistXZ(e.pos, e.lastSeen) > 3) MoveTowards(e, e.lastSeen, e.speed * 0.25f, dt, false);
            return;
        }
        if (e.ai == AI_RETURN) {
            if (MoveTowards(e, e.home, e.speed * 0.45f, dt) || DistXZ(e.pos, e.home) < 1.0f) e.ai = AI_IDLE;
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

    // --- inseguimento ---
    G.chased = true;
    if (e.los && P.stealthT <= 0) { e.lastSeen = P.pos; e.lostT = 0; }
    else e.lostT += dt * (P.stealthT > 0 ? 3.0f : 1.0f);
    bool tooFar = e.territory > 0 && DistXZ(e.pos, e.home) > e.territory + 6;
    bool lost = e.lostT > (e.kind == EK_CAMERA ? 1.5f : 5.0f);
    if (tooFar || lost || P.catchImmune > 0) {
        e.aware = false;
        e.alert = lost ? 0.3f : 0.0f;
        e.ai = AI_RETURN;
        if (tooFar && d < 40 && e.voiceCd <= 0) {
            PlaySfx3D(VoiceOf(e), e.pos, 0.6f);
            e.voiceCd = 3;
        }
        return;
    }
    if (e.kind == EK_CAMERA) {
        FacePoint(e, P.pos, dt);
        return;
    }
    MoveTowards(e, e.los ? P.pos : e.lastSeen, e.speed, dt);
    if (d < EntityReach(e) + 0.35f) CatchPlayer(e);
}

static void FriendlyAI(Entity& e, float dt, float d) {
    if (G.state == GS_DIALOGUE && G.dlg.npc == e.id) {
        FacePoint(e, G.player.pos, dt);
        e.vel = V3(0, 0, 0);
        return;
    }
    if (d < 3.0f) {
        FacePoint(e, G.player.pos, dt);
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
    if (DistXZ(e.pos, e.moveTarget) > 0.4f) MoveTowards(e, e.moveTarget, e.kind == EK_DOG ? 2.0f : 1.4f, dt);
    else e.vel = V3(0, 0, 0);
}

// il cane Ruggine: segue il giocatore e lo avvisa dei pericoli
static void CompanionAI(Entity& e, float dt) {
    Player& P = G.player;
    if (e.zone != P.zone) {
        e.zone = P.zone;
        e.pos = P.pos - YawDir(P.yaw) * 2.0f;
    }
    float d = DistXZ(e.pos, P.pos);
    if (d > 45) {
        e.pos = P.pos - YawDir(P.yaw) * 2.5f;
        e.pos.y = GroundHeight(e.zone, e.pos + V3(0, 1, 0), 1.0f);
        d = 2.5f;
    }
    if (d > 3.5f) MoveTowards(e, P.pos, d > 8 ? 7.5f : 4.5f, dt);
    else {
        e.vel = V3(0, 0, 0);
        FacePoint(e, P.pos, dt * 0.5f);
    }
    if (e.voiceCd <= 0) {
        for (auto& o : G.ents) {
            if (o.zone != e.zone || !o.hostile || o.off || o.aware || o.asleep) continue;
            float od = Vector3Distance(o.pos, P.pos);
            if (od < 22 && od > 6) {
                PlaySfx3D(SFX_BARK, e.pos, 0.8f);
                Notify(Fmt("Ruggine ringhia piano: c'è %s là davanti.", o.kind == EK_HUMAN ? "qualcuno" : "qualcosa"), Col(255, 220, 160));
                e.voiceCd = 20;
                break;
            }
        }
    }
}

void EntitiesUpdate(float dt) {
    Player& P = G.player;
    G.chased = false;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        Entity& e = G.ents[i];
        if (e.stunT > 0) {
            e.stunT = std::max(0.0f, e.stunT - dt);
            e.vel = V3(0, 0, 0);
            e.moveAmt = 0;
            if (e.stunT > 0) continue;
        }
        if (e.companion) {
            if (e.voiceCd > 0) e.voiceCd -= dt;
            Vector3 before = e.pos;
            CompanionAI(e, dt);
            ResolveCollision(e.zone, e.pos, EntityRadius(e), EntityHeight(e), 0.6f);
            e.pos.y = GroundHeight(e.zone, V3(e.pos.x, e.pos.y + 0.3f, e.pos.z), 0.6f);
            float moved = DistXZ(before, e.pos) / std::max(dt, 1e-4f);
            e.moveAmt = LerpF(e.moveAmt, Clamp01(moved / 4.0f), Clamp01(dt * 8));
            e.anim += moved * dt * 2.6f;
            continue;
        }
        if (e.zone != P.zone) continue;
        if (e.voiceCd > 0) e.voiceCd -= dt;
        if (e.off) {
            e.moveAmt = LerpF(e.moveAmt, 0, Clamp01(dt * 4));
            continue;
        }
        float d = Vector3Distance(e.pos, P.pos);
        if (d > 170 && !e.aware) continue;
        Vector3 before = e.pos;
        if (e.hostile) GuardAI(e, dt, d);
        else FriendlyAI(e, dt, d);
        if (e.kind != EK_CAMERA) {
            ResolveCollision(e.zone, e.pos, EntityRadius(e), EntityHeight(e), 0.6f);
            float gy = GroundHeight(e.zone, V3(e.pos.x, e.pos.y + 0.3f, e.pos.z), 0.6f);
            if (gy < e.pos.y - 0.05f) e.pos.y = std::max(gy, e.pos.y - 9.0f * dt);
            else e.pos.y = gy;
        }
        // separazione tra entità
        for (int j = i + 1; j < (int)G.ents.size(); j++) {
            Entity& o = G.ents[j];
            if (o.zone != e.zone || o.kind == EK_CAMERA || e.kind == EK_CAMERA) continue;
            float rr = EntityRadius(e) + EntityRadius(o);
            float dx = o.pos.x - e.pos.x, dz = o.pos.z - e.pos.z;
            float dd = dx * dx + dz * dz;
            if (dd < rr * rr && dd > 1e-6f) {
                float dl = sqrtf(dd), push = (rr - dl) * 0.5f;
                e.pos.x -= dx / dl * push; e.pos.z -= dz / dl * push;
                o.pos.x += dx / dl * push; o.pos.z += dz / dl * push;
            }
        }
        // i personaggi non si sovrappongono al giocatore
        if (e.kind != EK_CAMERA) {
            float rr = EntityRadius(e) + 0.35f;
            float dx = e.pos.x - P.pos.x, dz = e.pos.z - P.pos.z;
            float dd = dx * dx + dz * dz;
            if (dd < rr * rr && dd > 1e-6f) {
                float dl = sqrtf(dd);
                e.pos.x = P.pos.x + dx / dl * rr;
                e.pos.z = P.pos.z + dz / dl * rr;
            }
        }
        float moved = DistXZ(before, e.pos) / std::max(dt, 1e-4f);
        e.moveAmt = LerpF(e.moveAmt, Clamp01(moved / 4.0f), Clamp01(dt * 8));
        e.anim += moved * dt * (e.kind == EK_ROACH ? 5.0f : (e.kind == EK_DEATHCLAW ? 1.4f : 2.6f)) / std::max(e.scale, 0.5f);
        if (e.glow) SpawnDynLight(e.zone, e.pos + V3(0, 1.4f, 0), Col(110, 255, 70), 7, 0.05f);
    }
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

void DrawToolModel(const Matrix& m, int tool, float flash) {
    switch (tool) {
        case TOOL_CAMERA:
            P(PR_CUBE, m, V3(0, 0, 0), V3(0.16f, 0.1f, 0.07f), Col(50, 48, 46));
            P(PR_CUBE, m, V3(0, 0.055f, -0.005f), V3(0.07f, 0.03f, 0.05f), Col(170, 170, 165));
            DrawPrim(PR_CYL, MatTRS(V3(0, -0.005f, 0.05f), V3(PI / 2, 0, 0), V3(0.06f, 0.05f, 0.06f)) * m, Col(30, 30, 32));
            DrawPrim(PR_CYL, MatTRS(V3(0, -0.005f, 0.077f), V3(PI / 2, 0, 0), V3(0.045f, 0.006f, 0.045f)) * m, Col(120, 160, 200), 0.3f);
            P(PR_CUBE, m, V3(0.05f, 0.035f, 0.036f), V3(0.035f, 0.02f, 0.006f), Col(240, 240, 230), flash > 0 ? 1.0f : 0.15f);
            break;
        case TOOL_BAIT:
            P(PR_SPHERE, m, V3(0, 0.01f, 0.03f), V3(0.1f, 0.08f, 0.11f), Col(130, 60, 45));
            P(PR_CUBE, m, V3(0, 0.05f, 0.03f), V3(0.03f, 0.03f, 0.08f), Col(170, 150, 110));
            break;
        case TOOL_NOISE:
            DrawPrim(PR_CYL, MatTRS(V3(0, 0.02f, 0.03f), V3(0, 0, 0), V3(0.07f, 0.11f, 0.07f)) * m, Col(150, 150, 140));
            DrawPrim(PR_CYL, MatTRS(V3(0, 0.03f, 0.03f), V3(0, 0, 0), V3(0.072f, 0.04f, 0.072f)) * m, Col(190, 60, 50));
            break;
        case TOOL_BLASTER: {
            // Impugnatura inclinata, corpo in metallo e bobina energetica visibile.
            P(PR_CUBE, m, V3(0, -0.055f, -0.025f), V3(0.055f, 0.15f, 0.075f), Col(38, 42, 47), 0, V3(-0.22f, 0, 0));
            P(PR_CUBE, m, V3(0, 0.025f, 0.055f), V3(0.105f, 0.085f, 0.25f), Col(65, 76, 85));
            P(PR_CUBE, m, V3(0, 0.071f, 0.025f), V3(0.045f, 0.025f, 0.13f), Col(145, 159, 168));
            DrawPrim(PR_CYL, MatTRS(V3(0, 0.025f, 0.205f), V3(PI / 2, 0, 0), V3(0.055f, 0.11f, 0.055f)) * m, Col(34, 45, 51));
            DrawPrim(PR_CYL, MatTRS(V3(0, 0.025f, 0.264f), V3(PI / 2, 0, 0), V3(0.035f, 0.012f, 0.035f)) * m, Col(100, 255, 235), 1.0f);
            P(PR_CUBE, m, V3(0.057f, 0.025f, 0.035f), V3(0.013f, 0.047f, 0.11f), Col(65, 230, 180), 0.8f);
            for (int i = 0; i < 3; i++)
                P(PR_CUBE, m, V3(-0.055f, 0.025f, 0.015f + i * 0.035f), V3(0.012f, 0.05f, 0.012f), Col(145, 174, 178));
            if (flash > 0)
                P(PR_SPHERE, m, V3(0, 0.025f, 0.29f), V3(0.09f, 0.09f, 0.15f), Col(170, 255, 240), 1.0f);
            break;
        }
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
        case OF_POWER: s.torso = s.upper = s.lower = s.pants = Col(118, 120, 108); s.plates = true; s.extra = Col(95, 98, 88); s.bulk = 1.4f; s.boots = Col(80, 82, 76); break;
        case OF_RADSUIT: s.torso = s.upper = s.lower = s.pants = Col(205, 185, 70); s.boots = Col(50, 50, 50); break;
        case OF_BARMAN: s.torso = Col(30, 30, 34); s.upper = s.lower = Col(220, 215, 205); s.pants = Col(40, 40, 44); break;
        case OF_DARK: s.torso = s.upper = s.lower = Col(78, 72, 64); s.pants = Col(60, 56, 50); s.coat = true; s.extra = Col(60, 56, 50); break;
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

// hold: 0 braccia rilassate, 1 attrezzo davanti al viso (fotocamera); action: lancio
void DrawHumanoid(Vector3 pos, float yaw, const Look& look, float anim, float moveAmt, float hold, float action, float lieT, int tool, float flash, float scale, bool glow, float crouch) {
    OutfitStyle st = Style(look);
    float em = glow ? 0.45f : 0.0f;
    float dk = Clamp01(lieT);
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
        if (look.outfit == OF_DARK) P(PR_SPHERE, torso, V3(0, 0.62f, -0.06f), V3(0.3f, 0.2f, 0.26f), st.extra, em);
    }
    if (look.outfit == OF_TRADER) P(PR_CUBE, torso, V3(0, 0.1f, 0.12f * b), V3(0.3f, 0.5f, 0.02f), st.extra, em);
    if (look.outfit == OF_BARMAN) P(PR_CUBE, torso, V3(0, 0.3f, 0.005f), V3(tw * 0.55f, 0.42f, 0.235f * b), Col(220, 215, 205), em);
    // testa
    Vector3 head = V3(0, 0.69f + (b - 1) * 0.08f, 0.02f + hunch * 0.1f);
    DrawLimb(torso, V3(0, 0.5f, 0), head, 0.05f * b, look.skin, em);
    if (look.outfit == OF_POWER) {
        P(PR_CUBE, torso, head, V3(0.32f, 0.34f, 0.34f), st.torso, em);
        P(PR_CUBE, torso, head + V3(0, 0.03f, 0.17f), V3(0.24f, 0.05f, 0.02f), Col(255, 200, 80), 0.9f);
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
    Vector3 handR{};
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sh = V3(s * (tw * 0.5f + 0.04f * b), 0.47f, 0);
        float as = (s < 0 ? -sw : sw) * 0.8f;
        Vector3 d1 = Vector3Normalize(V3(s * 0.12f, -cosf(as), sinf(as)));
        Vector3 d2 = Vector3Normalize(V3(s * 0.05f, -cosf(as + 0.35f), sinf(as + 0.35f)));
        if (hold > 0) {
            // tiene l'attrezzo davanti al viso
            Vector3 a1 = Vector3Normalize(V3(-s * 0.1f, -0.15f, 0.98f));
            Vector3 a2 = Vector3Normalize(V3(-s * 0.55f, 0.75f, 0.35f));
            d1 = Vector3Normalize(Vector3Lerp(d1, a1, hold));
            d2 = Vector3Normalize(Vector3Lerp(d2, a2, hold));
        }
        if (s > 0 && action > 0) {
            Vector3 up = Vector3Normalize(V3(0.35f, 0.75f, -0.35f)), fwd = Vector3Normalize(V3(-0.05f, 0.3f, 0.95f));
            float k1 = action < 0.35f ? action / 0.35f : 1.0f, k2 = action < 0.35f ? 0 : (action - 0.35f) / 0.65f;
            Vector3 tgt = Vector3Lerp(up, fwd, k2);
            d1 = Vector3Normalize(Vector3Lerp(d1, tgt, k1));
            d2 = Vector3Normalize(Vector3Lerp(d2, tgt, k1));
        }
        Vector3 elbow = sh + d1 * 0.3f, hand = elbow + d2 * 0.28f;
        DrawLimb(torso, sh, elbow, 0.058f * b, st.upper, em);
        DrawLimb(torso, elbow, hand, 0.05f * b, st.bareArms ? look.skin : st.lower, em);
        P(PR_SPHERE, torso, hand, V3(0.1f * b, 0.1f * b, 0.11f * b), look.outfit == OF_POWER ? st.extra : look.skin, em);
        if (s > 0) handR = hand;
    }
    if (tool != TOOL_HANDS) {
        Vector3 at = hold > 0.5f ? V3(0, 0.62f, 0.33f) : handR;
        Matrix tm = MatrixScale(1.6f, 1.6f, 1.6f) * Basis(V3(0, 0, 1), at) * torso;
        DrawToolModel(tm, tool, flash);
    }
}

static void DrawRoach(const Entity& e, float em) {
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color body = Col(96, 58, 38), shell = Col(72, 42, 28);
    float eat = e.ai == AI_DISTRACT && e.moveAmt < 0.2f ? fabsf(sinf(G.time * 9)) * 0.03f : 0;
    P(PR_SPHERE, root, V3(0, 0.17f, 0), V3(0.38f, 0.17f, 0.64f), body, em);
    P(PR_SPHERE, root, V3(0, 0.22f, -0.04f), V3(0.36f, 0.13f, 0.6f), shell, em);
    P(PR_SPHERE, root, V3(0, 0.14f - eat, 0.34f), V3(0.18f, 0.12f, 0.16f), body, em);
    for (int s = -1; s <= 1; s += 2) {
        float sway = sinf(G.time * 6 + s) * 0.1f;
        DrawLimb(root, V3(s * 0.04f, 0.17f, 0.4f), V3(s * 0.18f + sway, 0.3f, 0.85f), 0.008f, shell);
        for (int i = 0; i < 3; i++) {
            float z = 0.15f - i * 0.15f;
            float swing = sinf(e.anim * 2 + i * 2.1f + (s > 0 ? PI : 0)) * 0.13f * e.moveAmt;
            Vector3 b0 = V3(s * 0.12f, 0.13f, z), k = V3(s * 0.33f, 0.2f, z + swing), f = V3(s * 0.44f, 0.0f, z + swing * 1.6f);
            DrawLimb(root, b0, k, 0.018f, body, em);
            DrawLimb(root, k, f, 0.014f, body, em);
        }
    }
}

static void DrawQuadruped(const Entity& e, float em) {
    bool rat = e.kind == EK_MOLERAT;
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    float eat = e.ai == AI_DISTRACT && e.moveAmt < 0.2f ? 0.18f + fabsf(sinf(G.time * 6)) * 0.06f : 0;
    if (rat) {
        Color skin = Col(152, 112, 96), dark = Col(120, 88, 74);
        P(PR_SPHERE, root, V3(0, 0.42f, 0), V3(0.62f, 0.48f, 1.05f), skin, em);
        P(PR_SPHERE, root, V3(0, 0.45f - eat, 0.5f), V3(0.42f, 0.34f, 0.44f), dark, em);
        for (int s = -1; s <= 1; s += 2) {
            P(PR_CUBE, root, V3(s * 0.04f, 0.33f - eat, 0.74f), V3(0.04f, 0.13f, 0.03f), Col(235, 225, 190));
            P(PR_SPHERE, root, V3(s * 0.12f, 0.52f - eat, 0.66f), V3(0.04f, 0.04f, 0.04f), Col(20, 20, 20));
        }
        for (int i = 0; i < 4; i++) {
            float sx = (i & 1) ? 0.22f : -0.22f, sz = (i & 2) ? 0.3f : -0.3f;
            float sw = sinf(e.anim * 2 + ((i == 0 || i == 3) ? 0 : PI)) * 0.15f * e.moveAmt;
            DrawLimb(root, V3(sx, 0.32f, sz), V3(sx * 1.2f, 0.0f, sz + sw), 0.06f, dark, em);
        }
        DrawLimb(root, V3(0, 0.35f, -0.5f), V3(sinf(G.time * 3) * 0.2f, 0.15f, -1.05f), 0.025f, skin, em);
    } else {
        Color fur = e.companion || !e.hostile ? Col(150, 100, 60) : Col(110, 92, 72), dark = ColShade(fur, 0.72f);
        P(PR_SPHERE, root, V3(0, 0.62f, -0.05f), V3(0.34f, 0.36f, 0.9f), fur, em);
        P(PR_SPHERE, root, V3(0, 0.66f, 0.27f), V3(0.36f, 0.4f, 0.42f), fur, em);
        float hb = eat;
        DrawLimb(root, V3(0, 0.75f, 0.35f), V3(0, 0.9f - hb, 0.52f + hb * 0.5f), 0.09f, fur, em);
        P(PR_CUBE, root, V3(0, 0.92f - hb, 0.56f + hb * 0.5f), V3(0.2f, 0.2f, 0.26f), fur, em);
        P(PR_CUBE, root, V3(0, 0.87f - hb, 0.74f + hb * 0.5f), V3(0.11f, 0.1f, 0.18f), dark, em);
        for (int s = -1; s <= 1; s += 2) {
            P(PR_CONE, root, V3(s * 0.07f, 1.07f - hb, 0.52f + hb * 0.5f), V3(0.06f, 0.12f, 0.04f), dark, em);
            P(PR_CUBE, root, V3(s * 0.06f, 0.96f - hb, 0.69f + hb * 0.5f), V3(0.03f, 0.03f, 0.01f), Col(30, 20, 15), 0.3f);
        }
        if (e.companion || !e.hostile) P(PR_CYL, root, V3(0, 0.84f, 0.46f), V3(0.2f, 0.05f, 0.2f), Col(200, 40, 40), 0, V3(0.5f, 0, 0));
        for (int i = 0; i < 4; i++) {
            float sx = (i & 1) ? 0.13f : -0.13f, sz = (i & 2) ? 0.3f : -0.33f;
            float sw = sinf(e.anim * 1.6f + ((i == 0 || i == 3) ? 0 : PI)) * 0.4f * e.moveAmt;
            Vector3 hip = V3(sx, 0.55f, sz), knee = hip + V3(0, -0.28f, sw * 0.28f), foot = knee + V3(0, -0.27f, sw * 0.1f - 0.04f);
            DrawLimb(root, hip, knee, 0.05f, fur, em);
            DrawLimb(root, knee, foot, 0.04f, dark, em);
        }
        float wag = sinf(G.time * (e.companion ? 14.0f : 8.0f)) * (e.companion ? 0.35f : 0.2f);
        DrawLimb(root, V3(0, 0.7f, -0.42f), V3(wag, e.companion ? 1.0f : 0.85f, -0.75f), 0.03f, fur, em);
    }
}

static void DrawRobot(const Entity& e, float em) {
    float slump = e.off ? 0.35f : 0.0f;
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color body = e.tag == "mk5" ? Col(150, 70, 50) : Col(112, 118, 96), dark = Col(60, 62, 58), metal = Col(140, 142, 140);
    float sw = sinf(e.anim) * 0.4f * e.moveAmt;
    for (int s = -1; s <= 1; s += 2) {
        float a = s < 0 ? sw : -sw;
        Vector3 hip = V3(s * 0.24f, 0.85f, 0);
        Vector3 foot = hip + V3(0, -0.8f * cosf(a), 0.8f * sinf(a));
        DrawLimb(root, hip, foot, 0.11f, dark, em);
        P(PR_CUBE, root, foot + V3(0, 0.05f, 0.07f), V3(0.24f, 0.12f, 0.36f), metal, em);
    }
    Matrix upper = MatrixRotateX(slump) * MatrixTranslate(0, 0.85f, 0) * root;
    P(PR_CYL, upper, V3(0, 0.4f, 0), V3(0.85f, 0.85f, 0.7f), body, em);
    P(PR_CUBE, upper, V3(0, 0.45f, 0.3f), V3(0.5f, 0.45f, 0.12f), dark, em);
    for (int i = 0; i < 3; i++) P(PR_CUBE, upper, V3(-0.15f + i * 0.15f, 0.53f, 0.37f), V3(0.08f, 0.05f, 0.02f), e.off ? dark : (i == 1 ? Col(255, 200, 60) : Col(80, 255, 120)), e.off ? 0 : 0.9f);
    P(PR_SPHERE, upper, V3(0, 1.0f, 0), V3(0.55f, 0.45f, 0.55f), metal, em);
    Color eye = e.off ? Col(30, 30, 30) : (e.aware ? Col(255, 50, 40) : (e.alert > 0.35f ? Col(255, 200, 60) : Col(80, 200, 255)));
    P(PR_CUBE, upper, V3(0, 1.03f, 0.23f), V3(0.36f, 0.08f, 0.05f), eye, e.off ? 0.0f : 1.0f);
    DrawLimb(upper, V3(0, 1.2f, 0), V3(0.1f, 1.5f, -0.05f), 0.015f, dark);
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sh = V3(s * 0.48f, 0.7f, 0);
        Vector3 el = sh + V3(s * 0.05f, -0.4f, 0.1f + (e.aware ? 0.25f : 0));
        Vector3 hand = el + V3(0, -0.3f, 0.2f + (e.aware ? 0.2f : 0));
        DrawLimb(upper, sh, el, 0.08f, dark, em);
        DrawLimb(upper, el, hand, 0.07f, metal, em);
        P(PR_SPHERE, upper, hand, V3(0.14f, 0.14f, 0.14f), dark, em);
    }
}

static void DrawSecurityCamera(const Entity& e, float em) {
    Matrix base = MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color metal = Col(110, 112, 104), dark = Col(55, 57, 54);
    P(PR_CYL, base, V3(0, 1.1f, 0), V3(0.16f, 2.2f, 0.16f), dark, em);
    P(PR_CYL, base, V3(0, 0.05f, 0), V3(0.6f, 0.1f, 0.6f), dark, em);
    Matrix head = MatrixRotateX(e.off ? 0.6f : 0.25f) * MatrixRotateY(e.yaw) * MatrixTranslate(0, 2.35f, 0) * base;
    P(PR_CUBE, head, V3(0, 0, 0.12f), V3(0.3f, 0.26f, 0.55f), metal, em);
    DrawPrim(PR_CYL, MatTRS(V3(0, 0, 0.42f), V3(PI / 2, 0, 0), V3(0.18f, 0.08f, 0.18f)) * head, Col(30, 30, 32));
    Color lc = e.off ? Col(30, 30, 30) : (e.aware ? Col(255, 40, 30) : (e.alert > 0.35f ? Col(255, 200, 60) : Col(80, 255, 120)));
    P(PR_SPHERE, head, V3(0.1f, 0.12f, 0.3f), V3(0.06f, 0.06f, 0.06f), lc, e.off ? 0 : 1.0f);
}

static void DrawDeathclaw(const Entity& e, float em) {
    // quando dorme è sdraiato su un fianco e respira lentamente
    float lie = e.asleep ? 1.0f : 0.0f;
    float breathe = sinf(G.time * (e.asleep ? 1.1f : 2.0f)) * (e.asleep ? 0.05f : 0.02f);
    Matrix root = MatrixScale(e.scale, e.scale, e.scale) * MatrixRotateZ(lie * PI * 0.45f) * MatrixTranslate(0, lie * 0.65f, 0) * MatrixRotateY(e.yaw) * MatrixTranslate(e.pos.x, e.pos.y, e.pos.z);
    Color skin = Col(72, 62, 50), belly = Col(110, 96, 74), bone = Col(220, 210, 185);
    float sw = sinf(e.anim) * 0.5f * e.moveAmt;
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
    P(PR_SPHERE, root, V3(0, 1.75f + breathe, 0.15f), V3(1.15f, 1.05f + breathe, 1.45f), skin, em, V3(-0.35f, 0, 0));
    P(PR_SPHERE, root, V3(0, 1.95f + breathe, 0.65f), V3(1.05f, 0.95f, 0.85f), skin, em);
    P(PR_SPHERE, root, V3(0, 1.62f, 0.75f), V3(0.8f, 0.7f, 0.6f), belly, em);
    for (int i = 0; i < 4; i++) P(PR_CONE, root, V3(0, 2.4f - i * 0.15f, 0.4f - i * 0.35f), V3(0.12f, 0.28f, 0.12f), bone, em, V3(-0.6f, 0, 0));
    Vector3 hd = V3(0, 2.45f, 1.15f);
    P(PR_SPHERE, root, hd, V3(0.52f, 0.46f, 0.78f), skin, em);
    P(PR_SPHERE, root, hd + V3(0, -0.15f, 0.12f), V3(0.42f, 0.2f, 0.62f), belly, em);
    for (int s = -1; s <= 1; s += 2) {
        DrawPrim(PR_CONE, MatAlignY(hd + V3(s * 0.18f, 0.18f, -0.05f), hd + V3(s * 0.38f, 0.65f, -0.35f), 0.07f) * root, bone, em);
        if (!e.asleep) P(PR_SPHERE, root, hd + V3(s * 0.17f, 0.08f, 0.28f), V3(0.07f, 0.06f, 0.06f), Col(255, 200, 60), 0.8f);
    }
    for (int s = -1; s <= 1; s += 2) {
        Vector3 sh = V3(s * 0.6f, 2.2f, 0.7f);
        Vector3 el = sh + V3(s * 0.25f, -0.6f, 0.25f);
        Vector3 hand = el + V3(-s * 0.1f, -0.5f, 0.4f);
        DrawLimb(root, sh, el, 0.14f, skin, em);
        DrawLimb(root, el, hand, 0.11f, skin, em);
        Vector3 dir = Vector3Normalize(hand - el);
        for (int c = -1; c <= 1; c++) {
            Vector3 off = V3(c * 0.07f, 0, 0);
            DrawPrim(PR_CONE, MatAlignY(hand + off, hand + off + dir * 0.4f + V3(0, -0.1f, 0), 0.04f) * root, bone, em);
        }
    }
    Vector3 t0 = V3(0, 1.5f, -0.6f);
    for (int i = 0; i < 3; i++) {
        float ww = sinf(G.time * (e.asleep ? 0.6f : 2.5f) + i) * 0.25f * (i + 1);
        Vector3 t1 = t0 + V3(ww, -0.3f, -0.75f);
        DrawLimb(root, t0, t1, 0.2f - i * 0.05f, skin, em);
        t0 = t1;
    }
}

void DrawEntity(const Entity& e) {
    float em = 0;
    if (e.glow) em = 0.35f + 0.1f * sinf(G.time * 4);
    switch (e.kind) {
        case EK_ROACH: DrawRoach(e, em); break;
        case EK_MOLERAT: case EK_DOG: DrawQuadruped(e, em); break;
        case EK_ROBOT: DrawRobot(e, em); break;
        case EK_CAMERA: DrawSecurityCamera(e, em); break;
        case EK_DEATHCLAW: DrawDeathclaw(e, em); break;
        default: {
            Look look = e.look;
            if (e.glow) look.skin = Col(150, 200, 90);
            float hold = 0;
            if (e.kind == EK_GHOUL) hold = 0.35f; // braccia protese da sonnambulo
            DrawHumanoid(e.pos, e.yaw, look, e.anim, e.moveAmt, hold, 0, 0, TOOL_HANDS, 0, e.scale, e.glow, 0);
            break;
        }
    }
    if (e.stunT > 0 && !e.off) {
        for (int i = 0; i < 3; i++) {
            float angle = G.time * 3 + i * (2 * PI / 3);
            Vector3 spark = e.pos + V3(cosf(angle) * 0.32f, EntityHeight(e) + 0.18f + sinf(angle * 2) * 0.06f, sinf(angle) * 0.32f);
            P(PR_SPHERE, MatrixIdentity(), spark, V3(0.09f, 0.09f, 0.09f), Col(110, 235, 255), 1);
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

// ombre, coni visivi delle telecamere e delle sentinelle, "Z" di chi dorme
void EntitiesDrawShadows(Camera3D cam) {
    for (auto& e : G.ents) {
        if (e.zone != G.player.zone || e.kind == EK_CAMERA) continue;
        if (Vector3Distance(e.pos, cam.position) > 50) continue;
        float r = EntityRadius(e) * 1.2f;
        DrawCylinder(e.pos + V3(0, 0.03f, 0), r, r, 0.01f, 10, Color{0, 0, 0, 70});
    }
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    for (auto& e : G.ents) {
        if (e.zone != G.player.zone || e.off) continue;
        if (e.kind != EK_CAMERA && e.kind != EK_ROBOT) continue;
        if (Vector3Distance(e.pos, cam.position) > 80) continue;
        Color c = e.aware ? Col(255, 60, 40) : (e.alert > 0.35f ? Col(255, 200, 60) : Col(255, 240, 160));
        float len = e.kind == EK_CAMERA ? 14.0f : 9.0f;
        float pitchDown = e.kind == EK_CAMERA ? 0.22f : 0.12f;
        Vector3 eye = EntityEye(e);
        Vector3 dir = Vector3Normalize(YawDir(e.yaw) + V3(0, -pitchDown, 0));
        float radius = len * tanf(e.kind == EK_CAMERA ? 0.4f : 0.6f);
        DrawCylinderEx(eye, eye + dir * len, 0.05f, radius, 12, ColAlpha(c, 0.07f));
    }
    rlEnableDepthMask();
    EndBlendMode();
}
