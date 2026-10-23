// Effetti visivi, granate, S.P.A.T. (puntamento assistito) e oggetti interattivi.
#include "game.h"

static std::vector<Particle> sParts;
static std::vector<Tracer> sTracers;
static std::vector<Grenade> sGrenades;
static std::vector<DynLight> sDynLights;

// ---------------------------------------------------------------------------
//  Particelle
// ---------------------------------------------------------------------------
void SpawnParticle(int zone, Vector3 p, Vector3 v, float life, float size, Color c, float gravity, bool additive, float grow) {
    if (sParts.size() > 3000) return;
    Particle q;
    q.pos = p;
    q.vel = v;
    q.life = q.maxLife = life;
    q.size = size;
    q.grow = grow;
    q.col = c;
    q.gravity = gravity;
    q.additive = additive;
    q.zone = zone;
    sParts.push_back(q);
}

void SpawnBlood(int zone, Vector3 p, Vector3 dir, Color c, int n) {
    for (int i = 0; i < n; i++) {
        Vector3 v = dir * gRng.range(1, 4) + V3(gRng.range(-1.5f, 1.5f), gRng.range(0, 2.5f), gRng.range(-1.5f, 1.5f));
        SpawnParticle(zone, p, v, gRng.range(0.3f, 0.7f), gRng.range(0.06f, 0.14f), c, 9.8f, false);
    }
    SpawnParticle(zone, p, dir * 0.5f, 0.35f, 0.25f, ColAlpha(c, 0.6f), 0, false, 0.8f);
}

void SpawnImpact(int zone, Vector3 p, Vector3 n, bool metal) {
    if (metal) {
        for (int i = 0; i < 6; i++)
            SpawnParticle(zone, p, n * 2 + V3(gRng.range(-2.5f, 2.5f), gRng.range(0, 3), gRng.range(-2.5f, 2.5f)), 0.3f, 0.05f, Col(255, 210, 120), 9.8f, true);
    } else {
        for (int i = 0; i < 4; i++)
            SpawnParticle(zone, p, n * gRng.range(0.5f, 2) + V3(gRng.range(-0.6f, 0.6f), gRng.range(0.2f, 1.2f), gRng.range(-0.6f, 0.6f)), gRng.range(0.5f, 1.0f), gRng.range(0.12f, 0.25f), Col(150, 130, 105, 200), 1.5f, false, 0.8f);
    }
}

void SpawnTracer(int zone, Vector3 a, Vector3 b, Color c, float width, float life) {
    sTracers.push_back({a, b, life, life, c, width, zone});
}

void SpawnDynLight(int zone, Vector3 p, Color c, float range, float t) {
    if (sDynLights.size() > 32) return;
    sDynLights.push_back({p, c, range, t, zone});
}

void SpawnExplosion(int zone, Vector3 p, float radius, float dmg, bool byPlayer) {
    PlaySfx3D(SFX_EXPLOSION, p, 1.0f, 0.1f);
    SpawnDynLight(zone, p + V3(0, 1, 0), Col(255, 170, 80), radius * 4, 0.35f);
    for (int i = 0; i < 30; i++) {
        Vector3 v = V3(gRng.range(-1, 1), gRng.range(0.2f, 1.2f), gRng.range(-1, 1));
        v = Vector3Normalize(v) * gRng.range(2, 9);
        SpawnParticle(zone, p, v, gRng.range(0.3f, 0.7f), gRng.range(0.3f, 0.8f), Col(255, gRng.irange(120, 200), 60), 2, true, 1.5f);
    }
    for (int i = 0; i < 16; i++) {
        Vector3 v = V3(gRng.range(-2, 2), gRng.range(1, 4), gRng.range(-2, 2));
        SpawnParticle(zone, p, v, gRng.range(1.5f, 3.0f), gRng.range(0.6f, 1.2f), Col(60, 55, 50, 200), -0.5f, false, 1.2f);
    }
    for (int i = 0; i < 14; i++)
        SpawnParticle(zone, p, V3(gRng.range(-8, 8), gRng.range(3, 10), gRng.range(-8, 8)), gRng.range(0.5f, 1.2f), 0.06f, Col(255, 220, 140), 12, true);
    if (dmg <= 0) return;
    if (byPlayer && G.player.perks[PK_DEMOLITION]) dmg *= 1.5f;
    if (byPlayer) dmg *= 0.7f + SkillValue(SK_EXPLOSIVES) / 200.0f;
    for (auto& e : G.ents) {
        if (e.dead || e.zone != zone) continue;
        float d = Vector3Distance(e.pos + V3(0, 0.8f, 0), p);
        if (d > radius) continue;
        if (!LineOfSight(zone, p + V3(0, 0.3f, 0), e.pos + V3(0, 1, 0))) continue;
        DamageEntity(e, dmg * (1 - d / radius * 0.7f), 0, Vector3Normalize(e.pos - p), byPlayer);
    }
    if (G.player.zone == zone) {
        float d = Vector3Distance(PlayerChest(), p);
        if (d < radius && LineOfSight(zone, p + V3(0, 0.3f, 0), PlayerChest())) PlayerTakeDamage(dmg * 0.6f * (1 - d / radius * 0.7f), p);
    }
    if (byPlayer) AlertNearby(zone, p, 60);
}

void ThrowGrenade(int zone, Vector3 p, Vector3 v, bool player) {
    sGrenades.push_back({p, v, 2.4f, zone, player, WEAPONS[ITEMS[I_GRENADE].weapon].damage});
}

void EmittersUpdate(int zone, Vector3 camPos, float dt) {
    for (auto& em : W.zones[zone].emitters) {
        if (Vector3Distance(em.pos, camPos) > 90) continue;
        em.acc += em.rate * dt;
        while (em.acc > 1) {
            em.acc -= 1;
            switch (em.type) {
                case EM_FIRE:
                    SpawnParticle(zone, em.pos + V3(gRng.range(-0.2f, 0.2f), 0, gRng.range(-0.2f, 0.2f)) * em.size,
                                  V3(gRng.range(-0.2f, 0.2f), gRng.range(1.0f, 2.2f), gRng.range(-0.2f, 0.2f)), gRng.range(0.35f, 0.7f),
                                  gRng.range(0.25f, 0.45f) * em.size, Col(255, gRng.irange(90, 170), 40), -0.5f, true, -0.4f);
                    break;
                case EM_SMOKE:
                    SpawnParticle(zone, em.pos, V3(gRng.range(-0.3f, 0.3f), gRng.range(0.6f, 1.2f), gRng.range(-0.3f, 0.3f)), gRng.range(2.5f, 4.0f),
                                  gRng.range(0.3f, 0.5f) * em.size, Col(70, 66, 62, 110), -0.1f, false, 0.6f);
                    break;
                case EM_STEAM:
                    SpawnParticle(zone, em.pos, V3(gRng.range(-0.2f, 0.2f), gRng.range(1.0f, 2.0f), gRng.range(-0.2f, 0.2f)), 1.5f, 0.3f, Col(200, 200, 200, 90), -0.2f, false, 0.8f);
                    break;
                case EM_SPARKS:
                    SpawnParticle(zone, em.pos, V3(gRng.range(-2, 2), gRng.range(0, 3), gRng.range(-2, 2)), 0.4f, 0.04f, Col(255, 220, 120), 9.8f, true);
                    break;
                case EM_RADGLOW: {
                    float a = gRng.range(0, 6.28f), r = gRng.range(0, 6) * em.size;
                    SpawnParticle(zone, em.pos + V3(sinf(a) * r, 0.2f, cosf(a) * r), V3(0, gRng.range(0.2f, 0.6f), 0), gRng.range(1.5f, 3.0f), gRng.range(0.08f, 0.18f), Col(120, 255, 80), -0.05f, true);
                    break;
                }
                default: break;
            }
        }
    }
    // polvere nell'aria all'aperto
    if (W.zones[zone].outdoor && gRng.chance(dt * 25)) {
        Vector3 p = camPos + V3(gRng.range(-12, 12), gRng.range(-1, 4), gRng.range(-12, 12));
        SpawnParticle(zone, p, V3(gRng.range(0.5f, 1.5f), gRng.range(-0.1f, 0.1f), gRng.range(-0.2f, 0.4f)), gRng.range(3, 6), 0.025f, Col(230, 210, 170, 120), 0, false);
    }
}

void FxUpdate(float dt) {
    for (auto& p : sParts) {
        p.life -= dt;
        p.vel.y -= p.gravity * dt;
        p.pos = p.pos + p.vel * dt;
        p.size = std::max(0.01f, p.size + p.grow * dt);
        if (p.gravity > 5) {
            float g = GroundHeight(p.zone, p.pos, 0.2f);
            if (p.pos.y < g) { p.pos.y = g + 0.01f; p.vel = V3(0, 0, 0); }
        }
    }
    sParts.erase(std::remove_if(sParts.begin(), sParts.end(), [](const Particle& p) { return p.life <= 0; }), sParts.end());
    for (auto& t : sTracers) t.t -= dt;
    sTracers.erase(std::remove_if(sTracers.begin(), sTracers.end(), [](const Tracer& t) { return t.t <= 0; }), sTracers.end());
    for (auto& l : sDynLights) l.t -= dt;
    sDynLights.erase(std::remove_if(sDynLights.begin(), sDynLights.end(), [](const DynLight& l) { return l.t <= 0; }), sDynLights.end());
    for (auto& g : sGrenades) {
        g.fuse -= dt;
        g.vel.y -= 14 * dt;
        Vector3 np = g.pos + g.vel * dt;
        Vector3 d = np - g.pos;
        float len = Vector3Length(d);
        RayHit h;
        if (len > 1e-4f && RaycastWorld(g.zone, g.pos, d / len, len + 0.08f, h)) {
            Vector3 n = h.normal;
            g.vel = (g.vel - n * (2 * Vector3DotProduct(g.vel, n))) * 0.45f;
            np = h.point + n * 0.08f;
            if (Vector3Length(g.vel) > 1.5f) PlaySfx3D(SFX_BOUNCE, np, 0.5f);
        }
        g.pos = np;
        if (g.fuse <= 0) SpawnExplosion(g.zone, g.pos, 6.5f, g.dmg, g.player);
    }
    sGrenades.erase(std::remove_if(sGrenades.begin(), sGrenades.end(), [](const Grenade& g) { return g.fuse <= 0; }), sGrenades.end());
}

void FxCollectLights(int zone, std::vector<LightSrc>& out) {
    for (auto& l : sDynLights) {
        if (l.zone != zone) continue;
        LightSrc s;
        s.pos = l.pos;
        s.color = l.col;
        s.range = l.range;
        s.intensity = 1.6f;
        out.push_back(s);
    }
}

void FxDraw(Camera3D cam) {
    int zone = G.player.zone;
    // granate
    for (auto& g : sGrenades)
        if (g.zone == zone) DrawPrim(PR_SPHERE, MatTRS(g.pos, V3(G.time * 5, G.time * 3, 0), V3(0.12f, 0.15f, 0.12f)), Col(70, 80, 50));
    rlDrawRenderBatchActive();
    // traccianti
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& t : sTracers) {
        if (t.zone != zone) continue;
        float k = t.t / t.maxT;
        Color c = ColAlpha(t.col, k);
        DrawCylinderEx(t.a, t.b, t.width * (0.6f + k * 0.4f), t.width * (0.6f + k * 0.4f), 5, c);
        if (t.width > 0.02f) DrawCylinderEx(t.a, t.b, t.width * 2.5f, t.width * 2.5f, 6, ColAlpha(t.col, k * 0.25f));
    }
    // particelle additive
    for (auto& p : sParts) {
        if (p.zone != zone || !p.additive) continue;
        float k = Clamp01(p.life / p.maxLife);
        DrawBillboard(cam, R.softDot, p.pos, p.size, ColAlpha(p.col, k));
    }
    EndBlendMode();
    // particelle con trasparenza normale, ordinate per distanza
    static std::vector<int> idx;
    idx.clear();
    for (int i = 0; i < (int)sParts.size(); i++)
        if (sParts[i].zone == zone && !sParts[i].additive && Vector3Distance(sParts[i].pos, cam.position) < 90) idx.push_back(i);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return Vector3DistanceSqr(sParts[a].pos, cam.position) > Vector3DistanceSqr(sParts[b].pos, cam.position); });
    rlDisableDepthMask();
    for (int i : idx) {
        const Particle& p = sParts[i];
        float k = Clamp01(p.life / p.maxLife);
        Color c = p.col;
        float lv = std::max(R.lightLevel, 0.25f);
        c = ColShade(c, lv);
        c.a = (unsigned char)(p.col.a * std::min(1.0f, k * 1.5f));
        DrawBillboard(cam, R.softDot, p.pos, p.size, c);
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void FxClear() {
    sParts.clear();
    sTracers.clear();
    sGrenades.clear();
    sDynLights.clear();
}

// ---------------------------------------------------------------------------
//  S.P.A.T. - Sistema di Puntamento Assistito Tattico
// ---------------------------------------------------------------------------
float VatsChance(const Entity& e, int part) {
    const WeaponDef& w = CurrentWeapon();
    Vector3 eye = PlayerEye();
    float d = Vector3Distance(eye, EntityPartPos(e, part));
    float skill = SkillValue(w.skill);
    float range = w.melee ? w.range + EntityRadius(e) + 0.6f : w.range;
    if (w.melee) return d <= range ? 0.95f : 0.0f;
    float base = 0.35f + skill / 200.0f + Spec(ST_PER) * 0.02f;
    float distK = Clamp(1.0f - d / (range * 0.9f), 0.05f, 1.0f);
    float c = base * (0.35f + 0.65f * distK);
    float size = e.kind == EK_DEATHCLAW || e.kind == EK_MUTANT ? 1.15f : (e.kind == EK_ROACH ? 0.75f : 1.0f);
    float partK = part == 1 ? 0.55f : (part == 2 ? 0.8f : 1.0f);
    c *= partK * size;
    if (part == 1 && G.player.perks[PK_SNIPER]) c *= 1.25f;
    if (!LineOfSight(G.player.zone, eye, EntityPartPos(e, part))) c *= 0.2f;
    return Clamp(c, 0.02f, 0.95f);
}

void VatsEnter() {
    Player& P = G.player;
    const WeaponDef& w = CurrentWeapon();
    if (P.ap < w.apCost) {
        Notify("Punti Azione insufficienti per lo S.P.A.T.", Col(255, 160, 100));
        return;
    }
    if (!w.melee && P.mag <= 0 && P.inv.Count(w.ammo) <= 0) {
        Notify("Nessuna munizione.", Col(255, 160, 100));
        return;
    }
    G.vatsTargets.clear();
    Vector3 eye = PlayerEye(), f = PlayerForward();
    for (int i = 0; i < (int)G.ents.size(); i++) {
        Entity& e = G.ents[i];
        if (e.dead || e.zone != P.zone || !e.hostile) continue;
        Vector3 c = EntityPartPos(e, 0);
        Vector3 to = c - eye;
        float d = Vector3Length(to);
        float range = w.melee ? 12.0f : std::min(w.range, 110.0f);
        if (d > range) continue;
        if (Vector3DotProduct(to / d, f) < 0.45f) continue;
        if (!LineOfSight(P.zone, eye, c) && !LineOfSight(P.zone, eye, EntityPartPos(e, 1))) continue;
        VatsTarget t;
        t.ent = i;
        for (int p = 0; p < 3; p++) t.chance[p] = VatsChance(e, p);
        G.vatsTargets.push_back(t);
    }
    if (G.vatsTargets.empty()) {
        Notify("S.P.A.T.: nessun bersaglio.", Col(255, 160, 100));
        return;
    }
    std::sort(G.vatsTargets.begin(), G.vatsTargets.end(), [&](const VatsTarget& a, const VatsTarget& b) {
        return Vector3Distance(G.ents[a.ent].pos, P.pos) < Vector3Distance(G.ents[b.ent].pos, P.pos);
    });
    G.vatsQueue.clear();
    G.vatsSel = 0;
    G.vatsPart = 0;
    G.vatsExec = false;
    G.vatsT = 0;
    PlaySfx(SFX_VATS_IN);
    SetGameState(GS_VATS);
}

static int QueuedAP() {
    return (int)G.vatsQueue.size() * CurrentWeapon().apCost;
}

void VatsUpdate(float dt) {
    Player& P = G.player;
    const WeaponDef& w = CurrentWeapon();
    if (!G.vatsExec) {
        // fase di selezione: il tempo è fermo
        if (G.vatsTargets.empty()) { SetGameState(GS_PLAY); return; }
        int n = (int)G.vatsTargets.size();
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) || GetMouseWheelMove() > 0.5f) { G.vatsSel = (G.vatsSel - 1 + n) % n; PlaySfx(SFX_UI_MOVE); }
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) || GetMouseWheelMove() < -0.5f) { G.vatsSel = (G.vatsSel + 1) % n; PlaySfx(SFX_UI_MOVE); }
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) { G.vatsPart = (G.vatsPart + 2) % 3; PlaySfx(SFX_UI_MOVE); }
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) { G.vatsPart = (G.vatsPart + 1) % 3; PlaySfx(SFX_UI_MOVE); }
        Entity& tgt = G.ents[G.vatsTargets[G.vatsSel].ent];
        // mira la camera verso il bersaglio
        Vector3 tp = EntityPartPos(tgt, G.vatsPart);
        Vector3 to = tp - PlayerEye();
        float wantYaw = atan2f(to.x, to.z);
        float wantPitch = atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z));
        P.yaw += AngleWrap(wantYaw - P.yaw) * Clamp01(dt * 10);
        P.pitch += (wantPitch - P.pitch) * Clamp01(dt * 10);
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int maxShots = w.melee ? 99 : P.mag + P.inv.Count(w.ammo);
            if (QueuedAP() + w.apCost <= P.ap && (int)G.vatsQueue.size() < maxShots && G.vatsQueue.size() < 8) {
                G.vatsQueue.push_back({G.vatsTargets[G.vatsSel].ent, G.vatsPart});
                PlaySfx(SFX_UI_OK);
            } else PlaySfx(SFX_UI_BACK);
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) && !G.vatsQueue.empty()) {
            G.vatsQueue.pop_back();
            PlaySfx(SFX_UI_BACK);
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Q)) {
            if (G.vatsQueue.empty()) {
                PlaySfx(SFX_VATS_OUT);
                SetGameState(GS_PLAY);
                return;
            }
            G.vatsExec = true;
            G.vatsT = 0.25f;
        }
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
            PlaySfx(SFX_VATS_OUT);
            SetGameState(GS_PLAY);
        }
        return;
    }
    // fase di esecuzione al rallentatore
    G.vatsT -= dt;
    if (!G.vatsQueue.empty()) {
        VatsShot s = G.vatsQueue.front();
        Entity& e = G.ents[s.ent];
        if (e.dead) { G.vatsQueue.erase(G.vatsQueue.begin()); return; }
        Vector3 tp = EntityPartPos(e, s.part);
        Vector3 to = tp - PlayerEye();
        P.yaw += AngleWrap(atan2f(to.x, to.z) - P.yaw) * Clamp01(dt * 12);
        P.pitch += (atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z)) - P.pitch) * Clamp01(dt * 12);
        if (G.vatsT <= 0) {
            G.vatsQueue.erase(G.vatsQueue.begin());
            G.vatsT = w.melee ? 0.55f : std::max(0.45f, w.fireDelay * 1.5f);
            P.ap = std::max(0.0f, P.ap - w.apCost);
            float chance = VatsChance(e, s.part);
            bool hit = gRng.chance(chance);
            if (w.melee) {
                P.swingT = 0.001f;
                P.swingDone = true;
                PlaySfx(w.sfx);
                if (hit) {
                    float dmg = WeaponDamage(w) * 1.15f;
                    bool crit = gRng.chance(Spec(ST_LCK) * 0.015f + 0.05f);
                    DamageEntity(e, crit ? dmg * 2 : dmg, s.part, Vector3Normalize(to), true, crit);
                    PlaySfx3D(SFX_PUNCH, e.pos);
                }
            } else {
                if (P.mag <= 0) {
                    int nAmmo = std::min(w.mag, P.inv.Count(w.ammo));
                    P.inv.Remove(w.ammo, nAmmo);
                    P.mag += nAmmo;
                }
                if (P.mag > 0) {
                    P.mag--;
                    P.shots++;
                    Vector3 o = G.cam.position;
                    Vector3 dir = Vector3Normalize(tp - o);
                    if (!hit) {
                        Vector3 r = Vector3Normalize(Vector3CrossProduct(dir, V3(0, 1, 0)));
                        dir = Vector3Normalize(dir + r * gRng.range(0.04f, 0.09f) * (gRng.chance(0.5f) ? 1.f : -1.f) + V3(0, gRng.range(-0.05f, 0.06f), 0));
                    }
                    float dmg = WeaponDamage(w) * 1.1f;
                    for (int k = 0; k < w.pellets; k++) {
                        Vector3 dd = k == 0 ? dir : Vector3Normalize(dir + V3(gRng.range(-0.04f, 0.04f), gRng.range(-0.04f, 0.04f), gRng.range(-0.04f, 0.04f)));
                        FireRay(o, dd, w, dmg, MuzzleWorld(), s.ent, s.part);
                    }
                    P.flash = 0.08f;
                    P.recoil += w.recoil;
                    SpawnDynLight(P.zone, MuzzleWorld(), w.energy ? w.beam : Col(255, 200, 120), 8, 0.1f);
                    PlaySfx(w.sfx);
                    AlertNearby(P.zone, P.pos, 50);
                }
            }
        }
    } else if (G.vatsT <= 0) {
        PlaySfx(SFX_VATS_OUT);
        SetGameState(GS_PLAY);
    }
}

void VatsDraw() {
    float W_ = UIW();
    UIRect(0, 0, W_, 720, Color{20, 80, 30, 60});
    Color pip = PIP;
    if (!G.vatsExec && !G.vatsTargets.empty()) {
        Entity& e = G.ents[G.vatsTargets[G.vatsSel].ent];
        float s = UIS();
        Vector2 cs = GetWorldToScreen(EntityPartPos(e, 0), G.cam);
        float cx = cs.x / s, cy = cs.y / s;
        const int order[3] = {1, 0, 2};
        for (int k = 0; k < 3; k++) {
            int p = order[k];
            Vector2 sp = GetWorldToScreen(EntityPartPos(e, p), G.cam);
            float x = sp.x / s, y = sp.y / s;
            bool sel = p == G.vatsPart;
            int pct = (int)roundf(VatsChance(e, p) * 100);
            std::string label = Fmt("%s %d%%", EntityPartName(e, p), pct);
            Vector2 m = UIMeasure(label, 20);
            float bx = cx + 90, by = cy - 58 + k * 40;
            UIRect(bx - 6, by - 2, m.x + 12, 28, sel ? Color{30, 120, 50, 220} : Color{10, 40, 15, 180});
            UIRectLines(bx - 6, by - 2, m.x + 12, 28, sel ? 2 : 1, pip);
            UIText(label, bx, by + 2, 20, sel ? Color{220, 255, 220, 255} : pip);
            UIRect(x - 4, y - 4, 8, 8, sel ? Color{220, 255, 220, 255} : pip);
            DrawLineEx(V2(x * s, y * s), V2((bx - 6) * s, (by + 12) * s), 1.5f, ColAlpha(pip, sel ? 1.0f : 0.5f));
        }
        // nome e salute
        Vector2 hp = GetWorldToScreen(e.pos + V3(0, 2.3f * e.scale + (e.kind == EK_DEATHCLAW ? 1.0f : 0), 0), G.cam);
        float hx = hp.x / UIS(), hy = hp.y / UIS();
        UIText(e.name, hx, hy - 40, 22, pip, 1);
        UIRect(hx - 60, hy - 14, 120, 8, Color{10, 40, 15, 200});
        UIRect(hx - 60, hy - 14, 120 * Clamp01(e.hp / e.maxHp), 8, pip);
    }
    // barra PA e coda
    const WeaponDef& w = CurrentWeapon();
    float apLeft = G.player.ap - (G.vatsExec ? 0 : QueuedAP());
    UIRect(W_ / 2 - 260, 600, 520, 90, Color{5, 25, 10, 200});
    UIRectLines(W_ / 2 - 260, 600, 520, 90, 2, pip);
    UIText("S.P.A.T.  -  SISTEMA DI PUNTAMENTO ASSISTITO TATTICO", W_ / 2, 608, 18, pip, 1);
    UIText(Fmt("PA %d / %d    Costo per azione: %d    Azioni in coda: %d", (int)apLeft, (int)MaxAP(), w.apCost, (int)G.vatsQueue.size()), W_ / 2, 634, 18, pip, 1);
    UIRect(W_ / 2 - 240, 662, 480, 10, Color{10, 40, 15, 255});
    UIRect(W_ / 2 - 240, 662, 480 * Clamp01(apLeft / MaxAP()), 10, pip);
    if (!G.vatsExec)
        UIText("A/D bersaglio   W/S parte   E/Clic metti in coda   RMB annulla   INVIO esegui   ESC esci", W_ / 2, 572, 16, PIP_DIM, 1);
}

// ---------------------------------------------------------------------------
//  Oggetti interattivi
// ---------------------------------------------------------------------------
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

void Interact(int idx) {
    Interactable& it = G.inters[idx];
    switch (it.type) {
        case INT_CONTAINER:
            if (it.locked && !TryUnlock(it)) return;
            LootOpen(idx, -1);
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
                Notify("Serve una tessera d'accesso militare. Forse il terminale...", Col(255, 180, 120));
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
                Notify("Non puoi riposare con dei nemici nelle vicinanze.", Col(255, 180, 120));
                return;
            }
            SleepOpen();
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

static void DrawContainer(const Interactable& it) {
    Matrix base = MatrixRotateY(it.yaw) * MatrixTranslate(it.pos.x, it.pos.y, it.pos.z);
    Vector3 s = it.size;
    auto Bx = [&](Vector3 c, Vector3 sz, Color col, float em = 0) { DrawPrim(PR_CUBE, MatTRS(c, V3(0, 0, 0), sz) * base, col, em); };
    bool empty = it.inv.Empty();
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
            // corpo a terra
            Look l;
            l.outfit = it.tag == "scout" ? OF_LEATHER : OF_SETTLER;
            l.skin = Col(160, 130, 110);
            DrawHumanoid(it.pos, it.yaw, l, 0, 0, 0, 0, 5.0f, -1, 0, 1.0f, false);
            break;
        }
    }
    (void)empty;
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

void InteractablesDraw(Camera3D cam) {
    int zone = G.player.zone;
    for (auto& it : G.inters) {
        if (it.zone != zone) continue;
        if (Vector3Distance(it.pos, cam.position) > 110) continue;
        switch (it.type) {
            case INT_CONTAINER: DrawContainer(it); break;
            case INT_ITEM:
                if (it.used) break;
                if (!it.inv.Empty() && it.inv.items[0].id == I_FUSIONCELL) {
                    float bob = sinf(G.time * 2) * 0.05f;
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.35f + bob, 0), V3(0, G.time, 0), V3(0.3f, 0.6f, 0.3f)), Col(120, 200, 255), 0.8f);
                    DrawPrim(PR_CYL, MatTRS(it.pos + V3(0, 0.35f + bob, 0), V3(0, G.time, 0), V3(0.36f, 0.12f, 0.36f)), Col(150, 150, 150));
                } else DrawPrim(PR_CUBE, MatTRS(it.pos + V3(0, 0.12f, 0), V3(0, it.yaw, 0), V3(0.3f, 0.2f, 0.3f)), Col(160, 150, 120));
                break;
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
