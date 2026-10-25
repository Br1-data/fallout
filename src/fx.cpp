// Effetti visivi, oggetti lanciati (esche e barattoli rumorosi) e scanner del Pip-Boy.
#include "game.h"
#include "platform.h"

static std::vector<Particle> sParts;
static std::vector<Throwable> sThrown;
static std::vector<DynLight> sDynLights;
struct LaserBeam { Vector3 from, to; float life; int zone; };
static std::vector<LaserBeam> sLasers;

// ---------------------------------------------------------------------------
//  Particelle e luci
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

void SpawnImpact(int zone, Vector3 p, Vector3 n, bool metal) {
    if (metal) {
        for (int i = 0; i < 6; i++)
            SpawnParticle(zone, p, n * 2 + V3(gRng.range(-2.5f, 2.5f), gRng.range(0, 3), gRng.range(-2.5f, 2.5f)), 0.3f, 0.05f, Col(255, 210, 120), 9.8f, true);
    } else {
        for (int i = 0; i < 4; i++)
            SpawnParticle(zone, p, n * gRng.range(0.5f, 2) + V3(gRng.range(-0.6f, 0.6f), gRng.range(0.2f, 1.2f), gRng.range(-0.6f, 0.6f)), gRng.range(0.5f, 1.0f), gRng.range(0.12f, 0.25f), Col(150, 130, 105, 200), 1.5f, false, 0.8f);
    }
}

void SpawnDynLight(int zone, Vector3 p, Color c, float range, float t) {
    if (sDynLights.size() > 32) return;
    sDynLights.push_back({p, c, range, t, zone});
}

void SpawnLaser(int zone, Vector3 from, Vector3 to) {
    if (zone < 0 || zone >= ZONE_COUNT || Vector3DistanceSqr(from, to) < 0.0001f) return;
    if (sLasers.size() >= 64) sLasers.erase(sLasers.begin());
    sLasers.push_back({from, to, 0.18f, zone});
    SpawnDynLight(zone, from, Col(70, 255, 215), 7, 0.12f);
    SpawnDynLight(zone, to, Col(90, 255, 235), 5, 0.18f);
    for (int i = 0; i < 7; i++)
        SpawnParticle(zone, to, V3(gRng.range(-1.8f, 1.8f), gRng.range(0.4f, 2.5f), gRng.range(-1.8f, 1.8f)),
                      gRng.range(0.15f, 0.3f), 0.045f, Col(130, 255, 225), 3.0f, true);
}

// ---------------------------------------------------------------------------
//  Esche e barattoli: vengono lanciati, rimbalzano e, una volta fermi, attirano
// ---------------------------------------------------------------------------
void ThrowObject(int zone, Vector3 p, Vector3 v, int kind) {
    Throwable t;
    t.pos = p;
    t.vel = v;
    t.kind = kind;
    t.zone = zone;
    t.landed = false;
    t.life = kind == TOOL_BAIT ? 16.0f : 4.0f;
    t.spin = 0;
    sThrown.push_back(t);
}

static void Land(Throwable& t) {
    t.landed = true;
    G.player.distractions++;
    if (t.kind == TOOL_BAIT) {
        BaitAt(t.zone, t.pos);
        PlaySfx3D(SFX_IMPACT, t.pos, 0.4f);
    } else {
        float radius = 25.0f + (G.player.perks[PK_THROWER] ? 8.0f : 0.0f);
        NoiseAt(t.zone, t.pos, radius, true);
        PlaySfx3D(SFX_RATTLE, t.pos, 1.0f, 0.1f);
    }
}

static void ThrownUpdate(float dt) {
    for (auto& t : sThrown) {
        t.life -= t.landed ? dt : 0;
        if (t.landed) {
            if (t.kind == TOOL_BAIT && gRng.chance(dt * 4))
                SpawnParticle(t.zone, t.pos + V3(gRng.range(-0.1f, 0.1f), 0.1f, gRng.range(-0.1f, 0.1f)), V3(gRng.range(-0.1f, 0.1f), gRng.range(0.3f, 0.6f), gRng.range(-0.1f, 0.1f)), 1.6f, 0.12f, Col(190, 210, 120, 70), -0.05f, false, 0.25f);
            if (t.kind == TOOL_NOISE && t.life > 1.0f && gRng.chance(dt * 1.5f)) PlaySfx3D(SFX_RATTLE, t.pos, 0.6f, 0.15f);
            continue;
        }
        t.vel.y -= 14 * dt;
        t.spin += dt * 9;
        Vector3 np = t.pos + t.vel * dt;
        Vector3 d = np - t.pos;
        float len = Vector3Length(d);
        RayHit h;
        if (len > 1e-4f && RaycastWorld(t.zone, t.pos, d / len, len + 0.08f, h)) {
            Vector3 n = h.normal;
            t.vel = (t.vel - n * (2 * Vector3DotProduct(t.vel, n))) * (t.kind == TOOL_BAIT ? 0.2f : 0.4f);
            np = h.point + n * 0.08f;
            if (Vector3Length(t.vel) > 1.5f) PlaySfx3D(t.kind == TOOL_NOISE ? SFX_METALHIT : SFX_BOUNCE, np, 0.5f);
            if (n.y > 0.6f && Vector3Length(t.vel) < 1.6f) {
                np = h.point + V3(0, 0.05f, 0);
                t.pos = np;
                Land(t);
                continue;
            }
        }
        t.pos = np;
        // caduto fuori dal mondo o in volo da troppo tempo: si ferma dove si trova
        float g = GroundHeight(t.zone, t.pos, 0.3f);
        if (t.pos.y < g - 0.5f) { t.pos.y = g + 0.05f; Land(t); }
        t.life -= dt * 0.25f;
        if (t.life < 0) { t.life = 1; Land(t); }
    }
    sThrown.erase(std::remove_if(sThrown.begin(), sThrown.end(), [](const Throwable& t) { return t.landed && t.life <= 0; }), sThrown.end());
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
                case EM_DRIP:
                    SpawnParticle(zone, em.pos, V3(0, -0.5f, 0), 0.8f, 0.04f, Col(140, 170, 190, 180), 9.8f, false);
                    break;
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
    for (auto& laser : sLasers) laser.life -= dt;
    sLasers.erase(std::remove_if(sLasers.begin(), sLasers.end(), [](const LaserBeam& laser) { return laser.life <= 0; }), sLasers.end());
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
    for (auto& l : sDynLights) l.t -= dt;
    sDynLights.erase(std::remove_if(sDynLights.begin(), sDynLights.end(), [](const DynLight& l) { return l.t <= 0; }), sDynLights.end());
    ThrownUpdate(dt);
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
    // oggetti lanciati (modelli degli attrezzi, ingranditi per leggibilità)
    for (auto& t : sThrown) {
        if (t.zone != zone) continue;
        float fade = t.landed && t.kind == TOOL_BAIT ? Clamp01(t.life / 2.0f) : 1.0f;
        if (fade <= 0.02f) continue;
        Vector3 rot = t.landed ? V3(0, t.spin, 0) : V3(t.spin, t.spin * 0.7f, 0);
        Matrix m = MatrixScale(1.8f * fade, 1.8f * fade, 1.8f * fade) * MatrixRotateXYZ(rot) * MatrixTranslate(t.pos.x, t.pos.y + 0.02f, t.pos.z);
        DrawToolModel(m, t.kind, 0);
    }
    rlDrawRenderBatchActive();
    // particelle additive
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    for (const auto& laser : sLasers) {
        if (laser.zone != zone) continue;
        float fade = Clamp01(laser.life / 0.18f);
        DrawBeam(laser.from, laser.to, 0.045f, ColAlpha(Col(30, 180, 155), fade * 0.7f));
        DrawBeam(laser.from, laser.to, 0.012f, ColAlpha(Col(200, 255, 245), fade));
        DrawBillboard(cam, R.softDot, laser.to, 0.5f * fade, ColAlpha(Col(100, 255, 220), fade));
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
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
    sThrown.clear();
    sDynLights.clear();
    sLasers.clear();
}

// ---------------------------------------------------------------------------
//  Scanner del Pip-Boy: ferma il tempo e mostra creature, oggetti e soggetti
//  fotografici nelle vicinanze, con il loro stato.
// ---------------------------------------------------------------------------
static float sScanT = 0;

void ScanEnter() {
    sScanT = 0;
    PlaySfx(SFX_SCAN_IN);
    SetGameState(GS_SCAN);
}

void ScanUpdate(float dt) {
    Player& P = G.player;
    sScanT += dt;
    Vector2 md = LookDelta();
    if (G.mouseSkip > 0) { G.mouseSkip--; md = V2(0, 0); }
    float sens = 0.0022f * G.settings.sensitivity;
    P.yaw = AngleWrap(P.yaw - md.x * sens);
    P.pitch = Clamp(P.pitch - md.y * sens * (G.settings.invertY ? -1.f : 1.f), -1.5f, 1.5f);
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        PlaySfx(SFX_SCAN_OUT);
        SetGameState(GS_PLAY);
    }
}

struct ScanLabel { Vector3 pos; std::string title, line1, line2; Color col; float dist; };

static bool OnScreen(Vector3 p, Vector2& out) {
    Vector3 f = Vector3Normalize(G.cam.target - G.cam.position);
    if (Vector3DotProduct(p - G.cam.position, f) < 0.5f) return false;
    out = GetWorldToScreen(p, G.cam);
    return out.x > -50 && out.y > -50 && out.x < GetScreenWidth() + 50 && out.y < GetScreenHeight() + 50;
}

static const char* InterLabel(const Interactable& it) {
    switch (it.type) {
        case INT_CONTAINER: return it.locked ? "Chiuso a chiave" : (it.inv.Empty() ? "Vuoto" : "Contiene qualcosa");
        case INT_DOOR: return it.open ? "Aperta" : (it.locked ? "Chiusa a chiave" : "Chiusa");
        case INT_TERMINAL: return it.lockedOut ? "Bloccato" : (it.hackLevel > 0 && !it.hacked ? "Protetto da password" : "Accessibile");
        case INT_REPAIR: return it.used ? "Riparato" : "Guasto: si può riparare";
        case INT_LADDER: return "Scala";
        case INT_GATE: return it.open ? "Aperto" : "Chiuso";
        case INT_ITEM: return "Oggetto a terra";
        case INT_BED: return "Riposa qui";
        default: return "";
    }
}

void ScanDraw() {
    Player& P = G.player;
    float s = UIS(), W_ = UIW();
    float k = Clamp01(sScanT * 3);
    UIRect(0, 0, W_, 720, Color{20, 90, 40, (unsigned char)(70 * k)});
    // linee di scansione
    float sweep = fmodf(sScanT * 260, 760) - 20;
    UIRect(0, sweep, W_, 3, ColAlpha(PIP, 0.35f));
    for (int y = 0; y < 720; y += 6) UIRect(0, (float)y, W_, 1, Color{0, 0, 0, 30});
    std::vector<ScanLabel> labels;
    const float range = 45;
    for (auto& e : G.ents) {
        if (e.zone != P.zone) continue;
        float d = Vector3Distance(e.pos, P.pos);
        if (d > range) continue;
        if (!LineOfSight(P.zone, PlayerEye(), EntityCenter(e)) && d > 12) continue;
        ScanLabel l;
        l.pos = EntityCenter(e) + V3(0, 0.6f * e.scale + (e.kind == EK_DEATHCLAW ? 1.2f : 0.3f), 0);
        l.title = e.name;
        l.line1 = EntityState(e);
        l.line2 = EntityHint(e);
        if (!e.photoKey.empty()) l.line2 += HasPhoto(e.photoKey) ? "  [foto: sì]" : "  [foto: no]";
        bool danger = e.hostile && !e.off && !e.asleep;
        l.col = e.ai == AI_CHASE ? Col(255, 110, 80) : (danger ? Col(255, 220, 110) : PIP);
        l.dist = d;
        labels.push_back(l);
    }
    for (auto& it : G.inters) {
        if (it.zone != P.zone || (it.type == INT_ITEM && it.used) || it.type == INT_TRANSITION || it.type == INT_REACTOR || it.type == INT_VAULTDOOR) continue;
        float d = Vector3Distance(it.pos, P.pos);
        if (d > 20) continue;
        if (!LineOfSight(P.zone, PlayerEye(), it.pos + V3(0, 0.5f, 0)) && d > 5) continue;
        ScanLabel l;
        l.pos = it.pos + V3(0, std::max(0.6f, it.size.y) + 0.2f, 0);
        l.title = it.name;
        l.line1 = InterLabel(it);
        if (it.type == INT_REPAIR && !it.used) l.line2 = Fmt("Riparazione %d, componenti %d", it.skillReq, it.partsReq);
        if ((it.type == INT_CONTAINER || it.type == INT_DOOR) && it.locked) {
            static const int req[] = {0, 0, 25, 50, 75, 999};
            l.line2 = it.lock >= 5 ? std::string("Serve una chiave") : Fmt("Scasso %d", req[Clamp(it.lock, 0, 5)]);
        }
        l.col = Col(170, 230, 255);
        l.dist = d;
        labels.push_back(l);
    }
    for (auto& ps : PHOTO_SUBJECTS) {
        if (!ps.landmark || ps.zone != P.zone) continue;
        float d = Vector3Distance(ps.pos, P.pos);
        if (d > 200) continue;
        ScanLabel l;
        l.pos = ps.pos;
        l.title = Fmt("[FOTO] %s", ps.name);
        l.line1 = HasPhoto(ps.key) ? "Già nel catalogo" : "Da fotografare";
        l.col = HasPhoto(ps.key) ? PIP_DIM : Col(255, 240, 160);
        l.dist = d;
        labels.push_back(l);
    }
    // più lontani prima, così i vicini restano leggibili sopra
    // i più vicini hanno la precedenza; le etichette che si sovrappongono salgono
    std::sort(labels.begin(), labels.end(), [](const ScanLabel& a, const ScanLabel& b) { return a.dist < b.dist; });
    std::vector<Rectangle> placed;
    for (auto& l : labels) {
        Vector2 sp;
        if (!OnScreen(l.pos, sp)) continue;
        float x = sp.x / s, y = sp.y / s;
        float fs = l.dist < 15 ? 18.0f : 15.0f;
        std::string dist = Fmt("%.0f m", l.dist);
        float w = std::max({UIMeasure(l.title, fs).x + UIMeasure(dist, fs - 4).x + 16, UIMeasure(l.line1, fs - 2).x, UIMeasure(l.line2, fs - 4).x, 60.0f}) + 14;
        float h = 8 + fs + (l.line1.empty() ? 0 : fs) + (l.line2.empty() ? 0 : fs - 2);
        float bx = x + 10, by = y - h - 6;
        for (int tries = 0; tries < 12; tries++) {
            bool hit = false;
            for (auto& r : placed)
                if (bx < r.x + r.width && bx + w > r.x && by < r.y + r.height && by + h > r.y) { by = r.y - h - 4; hit = true; }
            if (!hit) break;
        }
        if (by < 4) continue;
        placed.push_back(Rectangle{bx, by, w, h});
        UIRect(x - 3, y - 3, 6, 6, l.col);
        UIRect(bx, by, w, h, Color{4, 22, 10, (unsigned char)(200 * k)});
        UIRectLines(bx, by, w, h, 1, ColAlpha(l.col, k));
        DrawLineEx(V2(x * s, y * s), V2(bx * s, (by + h) * s), 1.2f, ColAlpha(l.col, 0.7f * k));
        x = bx - 10;
        float ty = by + 4;
        UIText(l.title, x + 17, ty, fs, ColAlpha(l.col, k));
        UIText(dist, x + 10 + w - 6, ty, fs - 4, ColAlpha(PIP_DIM, k), 2);
        ty += fs;
        if (!l.line1.empty()) { UIText(l.line1, x + 17, ty, fs - 2, ColAlpha(PIP, k)); ty += fs; }
        if (!l.line2.empty()) UIText(l.line2, x + 17, ty, fs - 4, ColAlpha(PIP_DIM, k));
    }
    // pannello
    UIRect(W_ / 2 - 280, 640, 560, 56, Color{5, 25, 10, 210});
    UIRectLines(W_ / 2 - 280, 640, 560, 56, 2, PIP);
    UIText("SCANNER DEL PIP-BOY  -  TEMPO SOSPESO", W_ / 2, 646, 18, PIP, 1);
    UIText("Muovi il mouse per guardarti intorno     Q / ESC / clic destro: esci", W_ / 2, 670, 15, PIP_DIM, 1);
}
