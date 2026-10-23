// Giocatore: statistiche, movimento, armi, oggetti, danni, visuale.
#include "game.h"

// ---------------------------------------------------------------------------
//  Statistiche
// ---------------------------------------------------------------------------
int Spec(int st) {
    int v = G.player.special[st];
    if (G.player.mentatsTimer > 0 && (st == ST_INT || st == ST_PER)) v += 2;
    return std::min(v, 12);
}

int SkillValue(Skill s) {
    const Player& P = G.player;
    int v = 2 + Spec(SKILL_GOV[s]) * 2 + (Spec(ST_LCK) + 1) / 2;
    if (P.tagged[s]) v += 15;
    v += P.skillPts[s];
    if (s == SK_SNEAK && P.perks[PK_NINJA]) v += 20;
    return Clamp(v, 0, 100);
}

float MaxHP() {
    const Player& P = G.player;
    return 80.0f + P.special[ST_END] * 15.0f + (P.level - 1) * 10.0f + (P.perks[PK_LIFEGIVER] ? 30.0f : 0.0f);
}
float HPCap() { return std::max(1.0f, MaxHP() * (1.0f - Clamp01(G.player.rads / 1000.0f))); }
float MaxAP() { return 65.0f + G.player.special[ST_AGI] * 3.0f + (G.player.perks[PK_ACTIONBOY] ? 25.0f : 0.0f); }
float CarryMax() { return 150.0f + G.player.special[ST_STR] * 10.0f + (G.player.perks[PK_STRONGBACK] ? 50.0f : 0.0f); }
float PlayerDR() {
    float dr = G.player.armor >= 0 ? ITEMS[G.player.armor].dr : 0;
    if (G.player.perks[PK_TOUGHNESS]) dr += 10;
    return dr;
}
float PlayerRadRes() {
    const Player& P = G.player;
    float r = P.special[ST_END] * 0.02f;
    if (P.armor >= 0) r += ITEMS[P.armor].radRes;
    if (P.perks[PK_RADRES]) r += 0.25f;
    if (P.radXTimer > 0) r += 0.5f;
    return Clamp(r, 0.0f, 0.9f);
}
int XPForLevel(int lvl) { return 75 * (lvl - 1) * lvl; }
int ArmorOutfit() { return G.player.armor >= 0 ? ITEMS[G.player.armor].look : OF_SETTLER; }

float PlayerSpeedMul() {
    float m = 1.0f;
    if (G.player.armor == I_POWER) m *= 0.9f;
    if (G.player.inv.Weight() > CarryMax()) m *= 0.5f;
    return m;
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
    if (id == G.player.weapon && G.player.inv.Count(id) == 0) { G.player.weapon = -1; G.player.mag = 0; }
}

Vector3 PlayerEye() { return G.player.pos + V3(0, G.player.eyeH, 0); }
Vector3 PlayerChest() { return G.player.pos + V3(0, G.player.eyeH - 0.45f, 0); }
Vector3 PlayerForward() {
    const Player& P = G.player;
    return V3(cosf(P.pitch) * sinf(P.yaw), sinf(P.pitch), cosf(P.pitch) * cosf(P.yaw));
}

int CurrentWeaponDefIndex() { return G.player.weapon >= 0 ? ITEMS[G.player.weapon].weapon : WEAPON_FIST; }
const WeaponDef& CurrentWeapon() { return WEAPONS[CurrentWeaponDefIndex()]; }

bool HostilesNear(float range) {
    for (auto& e : G.ents)
        if (!e.dead && e.hostile && e.aware && e.zone == G.player.zone && Vector3Distance(e.pos, G.player.pos) < range) return true;
    return false;
}

Vector3 MuzzleWorld() {
    Vector3 f = PlayerForward();
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, V3(0, 1, 0)));
    Vector3 u = Vector3CrossProduct(r, f);
    float aim = G.player.aimT;
    if (G.player.thirdPerson) return G.player.pos + V3(0, 1.45f, 0) + YawDir(G.player.yaw) * 0.7f + r * 0.15f;
    return G.cam.position + r * LerpF(0.16f, 0.0f, aim) - u * LerpF(0.12f, 0.07f, aim) + f * 0.75f;
}

// ---------------------------------------------------------------------------
//  Armi
// ---------------------------------------------------------------------------
float WeaponDamage(const WeaponDef& w) {
    const Player& P = G.player;
    float skill = SkillValue(w.skill);
    float d = w.damage * (0.5f + skill / 200.0f);
    bool oneHandGun = w.model == WM_PISTOL || w.model == WM_LASERPISTOL;
    if (P.perks[PK_GUNSLINGER] && oneHandGun) d *= 1.25f;
    if (P.perks[PK_COMMANDO] && w.twoHanded && !w.melee) d *= 1.25f;
    if (P.perks[PK_LASER] && w.energy) d *= 1.25f;
    if (w.melee) {
        d *= 1.0f + Spec(ST_STR) * 0.05f;
        if (P.perks[PK_IRONFIST]) d *= 1.5f;
    }
    if (P.perks[PK_BLOODYMESS]) d *= 1.05f;
    return d;
}

static float CritChance() {
    return Spec(ST_LCK) * 0.01f + (G.player.perks[PK_FINESSE] ? 0.05f : 0.0f);
}

void FireRay(Vector3 o, Vector3 dir, const WeaponDef& w, float dmg, Vector3 muzzle, int vatsTarget, int vatsPart) {
    int zone = G.player.zone;
    RayHit wh, eh;
    float maxT = w.range;
    bool hw = RaycastWorld(zone, o, dir, maxT, wh);
    bool he = RaycastEntities(zone, o, dir, hw ? wh.t : maxT, eh);
    Vector3 end = he ? eh.point : (hw ? wh.point : o + dir * maxT);
    if (he) {
        Entity& e = G.ents[eh.entity];
        bool crit = gRng.chance(CritChance());
        int part = eh.part;
        if (vatsTarget == eh.entity && vatsPart >= 0) part = vatsPart;
        float d = dmg;
        if (!e.aware && e.hostile) {
            d *= 2.0f;
            Notify("Attacco furtivo critico!", Col(255, 230, 120));
        } else if (crit) {
            d *= 2.0f;
            Notify("Colpo critico!", Col(255, 230, 120));
        }
        DamageEntity(e, d, part, dir, true, crit);
        PlaySfx3D(eh.metal ? SFX_METALHIT : SFX_HIT, eh.point, 0.7f);
    } else if (hw) {
        SpawnImpact(zone, wh.point, wh.normal, wh.metal);
        if (gRng.chance(0.3f)) PlaySfx3D(wh.metal ? SFX_METALHIT : SFX_IMPACT, wh.point, 0.5f);
    }
    if (w.energy) {
        SpawnTracer(zone, muzzle, end, w.beam, w.model == WM_PLASMA ? 0.07f : 0.03f, w.model == WM_PLASMA ? 0.25f : 0.15f);
        if (he || hw) SpawnDynLight(zone, end, w.beam, 4, 0.12f);
    } else if (gRng.chance(0.5f)) SpawnTracer(zone, muzzle, end, Col(255, 230, 170), 0.01f, 0.05f);
}

static Vector3 Spread(Vector3 f, float deg) {
    if (deg <= 0) return f;
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, V3(0, 1, 0)));
    Vector3 u = Vector3CrossProduct(r, f);
    float a = gRng.range(0, 2 * PI), m = sqrtf(gRng.f()) * tanf(deg * DEG2RAD);
    return Vector3Normalize(f + r * (cosf(a) * m) + u * (sinf(a) * m));
}

void PlayerFire() {
    Player& P = G.player;
    const WeaponDef& w = CurrentWeapon();
    if (w.melee) {
        P.swingT = 0.001f;
        P.swingDone = false;
        P.fireCd = w.fireDelay;
        PlaySfx(w.sfx, 0.7f, gRng.range(0.9f, 1.1f));
        AlertNearby(P.zone, P.pos, 8);
        return;
    }
    if (P.mag <= 0) {
        PlaySfx(SFX_EMPTY);
        P.fireCd = 0.3f;
        if (P.inv.Count(w.ammo) > 0) PlayerReload();
        else Notify("Munizioni esaurite!", Col(255, 140, 100));
        return;
    }
    P.mag--;
    P.shots++;
    P.fireCd = w.fireDelay;
    P.lastCombat = 0;
    float skill = SkillValue(w.skill);
    float spread = w.spread * (1.6f - skill / 100.0f);
    if (P.aimT > 0.5f) spread *= 0.4f;
    if (Vector3Length(V3(P.vel.x, 0, P.vel.z)) > 1) spread *= 1.4f;
    if (P.crouch) spread *= 0.8f;
    if ((P.perks[PK_GUNSLINGER] && (w.model == WM_PISTOL || w.model == WM_LASERPISTOL)) || (P.perks[PK_COMMANDO] && w.twoHanded)) spread *= 0.75f;
    if (w.pellets > 1) spread = std::max(spread, w.spread * 0.8f);
    Vector3 o = G.cam.position;
    Vector3 f = PlayerForward();
    if (P.thirdPerson) f = Vector3Normalize(G.cam.target - G.cam.position);
    Vector3 m = MuzzleWorld();
    float dmg = WeaponDamage(w);
    for (int i = 0; i < w.pellets; i++) FireRay(o, Spread(f, spread), w, dmg, m);
    P.recoil += w.recoil;
    P.pitch += w.recoil * DEG2RAD * 0.35f;
    P.yaw += gRng.range(-1, 1) * w.recoil * DEG2RAD * 0.15f;
    P.flash = 0.06f;
    SpawnDynLight(P.zone, m, w.energy ? w.beam : Col(255, 200, 120), 8, 0.06f);
    PlaySfx(w.sfx, 1.0f, gRng.range(0.95f, 1.05f));
    AlertNearby(P.zone, P.pos, w.energy ? 30 : 55);
    if (P.mag <= 0 && P.inv.Count(w.ammo) > 0) P.fireCd = std::max(P.fireCd, 0.25f);
}

void PlayerMeleeHit() {
    Player& P = G.player;
    const WeaponDef& w = CurrentWeapon();
    Vector3 eye = PlayerEye(), f = PlayerForward();
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < (int)G.ents.size(); i++) {
        Entity& e = G.ents[i];
        if (e.dead || e.zone != P.zone) continue;
        Vector3 c = e.pos + V3(0, std::min(1.0f, 0.9f * e.scale), 0);
        Vector3 to = c - eye;
        float d = Vector3Length(to);
        if (d > w.range + EntityRadius(e) + 0.3f) continue;
        float ang = acosf(Clamp(Vector3DotProduct(to / d, f), -1.f, 1.f));
        if (ang > 0.75f && d > 1.2f) continue;
        if (d < bd) { bd = d; best = i; }
    }
    if (best >= 0) {
        Entity& e = G.ents[best];
        float dmg = WeaponDamage(w);
        bool crit = gRng.chance(CritChance());
        if (!e.aware && e.hostile) { dmg *= 2; Notify("Attacco furtivo critico!", Col(255, 230, 120)); }
        else if (crit) { dmg *= 2; Notify("Colpo critico!", Col(255, 230, 120)); }
        DamageEntity(e, dmg, 0, f, true, crit);
        PlaySfx3D(e.kind == EK_ROBOT || e.kind == EK_TURRET ? SFX_METALHIT : SFX_PUNCH, e.pos, 1.0f);
        P.lastCombat = 0;
    } else {
        RayHit h;
        if (RaycastWorld(P.zone, eye, f, w.range, h)) {
            SpawnImpact(P.zone, h.point, h.normal, h.metal);
            PlaySfx3D(h.metal ? SFX_METALHIT : SFX_IMPACT, h.point, 0.6f);
        }
    }
}

void PlayerReload() {
    Player& P = G.player;
    const WeaponDef& w = CurrentWeapon();
    if (w.melee || w.ammo < 0 || w.mag <= 0) return;
    if (P.mag >= w.mag || P.reloadT > 0) return;
    if (P.inv.Count(w.ammo) <= 0) {
        Notify("Nessuna munizione di riserva.", Col(255, 140, 100));
        return;
    }
    P.reloadT = w.reload * (P.perks[PK_FASTRELOAD] ? 0.6f : 1.0f);
    PlaySfx(SFX_RELOAD);
}

static void FinishReload() {
    Player& P = G.player;
    const WeaponDef& w = CurrentWeapon();
    int n = std::min(w.mag - P.mag, P.inv.Count(w.ammo));
    if (n > 0) {
        P.inv.Remove(w.ammo, n);
        P.mag += n;
    }
}

void EquipWeapon(int item) {
    Player& P = G.player;
    if (item >= 0 && ITEMS[item].weapon >= 0 && WEAPONS[ITEMS[item].weapon].model == WM_GRENADE) {
        Notify("Le granate si lanciano con il tasto G.");
        return;
    }
    // scarica l'arma attuale
    const WeaponDef& cur = CurrentWeapon();
    if (P.weapon >= 0 && cur.ammo >= 0 && P.mag > 0) P.inv.Add(cur.ammo, P.mag);
    P.mag = 0;
    P.reloadT = 0;
    if (item == P.weapon) item = -1;
    P.weapon = item;
    const WeaponDef& w = CurrentWeapon();
    if (item >= 0 && w.ammo >= 0 && w.mag > 0) {
        int n = std::min(w.mag, P.inv.Count(w.ammo));
        P.inv.Remove(w.ammo, n);
        P.mag = n;
    }
    P.equipT = 0.45f;
    P.swingT = 0;
    PlaySfx(SFX_RELOAD, 0.5f, 1.3f);
}

void EquipArmor(int item) {
    Player& P = G.player;
    P.armor = (P.armor == item) ? -1 : item;
    PlaySfx(SFX_PICKUP, 0.6f, 0.8f);
}

void UseItem(int item) {
    Player& P = G.player;
    const ItemDef& d = ITEMS[item];
    if (d.type == IT_WEAPON) { EquipWeapon(item); return; }
    if (d.type == IT_ARMOR) { EquipArmor(item); return; }
    if (d.type != IT_AID) {
        if (item == I_SCOUTNOTE) BigNotify("Diario dell'esploratore", "Il deposito è sul bordo orientale del Cratere Verde.");
        return;
    }
    if (P.inv.Count(item) <= 0) return;
    P.inv.Remove(item, 1);
    float med = 0.6f + SkillValue(SK_MEDICINE) / 100.0f * 0.8f;
    if (P.perks[PK_MEDIC]) med *= 1.5f;
    if (item == I_STIMPAK) {
        P.healOverTime += d.heal * med;
        P.stimpaksUsed++;
        PlaySfx(SFX_STIM);
    } else if (item == I_RADAWAY) {
        P.rads = std::max(0.0f, P.rads + d.rads * med);
        PlaySfx(SFX_STIM, 0.8f, 0.8f);
        Notify("Radiazioni ridotte.");
    } else if (item == I_RADX) {
        P.radXTimer = 180;
        PlaySfx(SFX_EAT, 0.6f, 1.4f);
        Notify("Resistenza alle radiazioni aumentata.");
    } else if (item == I_JET) {
        P.jetTimer = 12;
        P.ap = std::min(MaxAP(), P.ap + d.ap);
        PlaySfx(SFX_STIM, 0.8f, 1.4f);
        Notify("Scheggia: il tempo rallenta...", Col(255, 200, 120));
    } else if (item == I_MENTATS) {
        P.mentatsTimer = 120;
        PlaySfx(SFX_EAT, 0.6f, 1.5f);
        Notify("Mentalix: +2 Intelligenza e Percezione.");
    } else {
        P.healOverTime += d.heal;
        float r = d.rads * (P.perks[PK_LEADBELLY] ? 0.5f : 1.0f);
        P.rads += r * (1.0f - PlayerRadRes() * 0.5f);
        if (d.ap > 0) P.ap = std::min(MaxAP(), P.ap + d.ap);
        PlaySfx(SFX_EAT);
    }
}

// ---------------------------------------------------------------------------
//  Danni e uccisioni
// ---------------------------------------------------------------------------
void PlayerTakeDamage(float dmg, Vector3 from, bool radiation) {
    Player& P = G.player;
    if (G.state == GS_DEAD || G.state == GS_ENDING) return;
    if (G.state == GS_VATS && G.vatsExec) dmg *= 0.25f;
    float final_ = radiation ? dmg : dmg * 40.0f / (40.0f + PlayerDR());
    P.hp -= final_;
    P.hurtFlash = std::min(1.0f, P.hurtFlash + final_ / 22.0f + 0.15f);
    P.hurtDir = AngleWrap(AngleTo(P.pos, from) - P.yaw);
    P.lastCombat = 0;
    static float hurtCd = 0;
    if (G.time - hurtCd > 0.35f) {
        PlaySfx(SFX_HURT, 0.8f, gRng.range(0.9f, 1.1f));
        hurtCd = G.time;
    }
    if (P.hp <= 0) {
        P.hp = 0;
        PlaySfx(SFX_DEATH);
        SetGameState(GS_DEAD);
    }
}

void PlayerKilledEntity(Entity& e) {
    Player& P = G.player;
    P.kills++;
    GiveXP(e.xp, ("Ucciso: " + e.name).c_str());
    if (e.fac == FAC_TOWN || e.fac == FAC_VAULT) P.karma -= 50;
    else if (e.fac == FAC_RAIDER) P.karma += 5;
    QuestsOnKill(e);
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
        if (e.zone != P.zone) continue;
        if (!e.dead && (e.hostile || e.dlg.empty())) continue;
        if (e.dead && e.inv.Empty()) continue;
        Vector3 c = e.pos + V3(0, e.dead ? 0.3f : 1.1f * e.scale, 0);
        Vector3 to = c - eye;
        float d = Vector3Length(to);
        if (d > (e.dead ? 2.6f : 3.2f) + EntityRadius(e)) continue;
        float ang = acosf(Clamp(Vector3DotProduct(to / d, f), -1.f, 1.f));
        if (d < 1.2f) ang *= 0.5f;
        if (ang < best) { best = ang; G.lookEnt = i; }
    }
    for (int i = 0; i < (int)G.inters.size(); i++) {
        Interactable& it = G.inters[i];
        if (it.zone != P.zone || (it.type == INT_ITEM && it.used)) continue;
        Vector3 c = it.pos + (it.type == INT_CONTAINER ? V3(0, it.size.y * 0.5f, 0) : V3(0, 0, 0));
        Vector3 to = c - eye;
        float d = Vector3Length(to);
        float range = std::max(2.6f, std::max(it.size.x, it.size.y) * 0.65f);
        if (d > range) continue;
        float ang = acosf(Clamp(Vector3DotProduct(to / d, f), -1.f, 1.f));
        float tol = (it.type == INT_TRANSITION || it.type == INT_VAULTDOOR || it.type == INT_GATE || it.type == INT_REACTOR) ? 0.75f : 0.45f;
        if (ang < tol && ang * 0.8f < best) { best = ang * 0.8f; G.lookInter = i; G.lookEnt = -1; }
    }
}

static int WeaponSlot(int n) {
    // n-esima arma nell'inventario (per i tasti 1-9 e la rotella)
    int k = 0;
    for (auto& s : G.player.inv.items) {
        if (ITEMS[s.id].type != IT_WEAPON || s.id == I_GRENADE) continue;
        if (k == n) return s.id;
        k++;
    }
    return -2;
}

static void CycleWeapon(int dir) {
    std::vector<int> list = {-1};
    for (auto& s : G.player.inv.items)
        if (ITEMS[s.id].type == IT_WEAPON && s.id != I_GRENADE) list.push_back(s.id);
    if (list.size() <= 1) return;
    int cur = 0;
    for (int i = 0; i < (int)list.size(); i++)
        if (list[i] == G.player.weapon) cur = i;
    int nx = (cur + dir + (int)list.size()) % (int)list.size();
    int target = list[nx];
    if (target == -1) EquipWeapon(G.player.weapon);
    else EquipWeapon(target);
}

void PlayerUpdate(float dt) {
    Player& P = G.player;
    P.playTime += dt;
    P.lastCombat += dt;
    // --- visuale ---
    Vector2 md = GetMouseDelta();
    if (G.mouseSkip > 0) { G.mouseSkip--; md = V2(0, 0); }
    float sens = 0.0022f * G.settings.sensitivity * (P.aimT > 0.5f ? 0.55f : 1.0f);
    P.yaw -= md.x * sens;
    P.pitch -= md.y * sens * (G.settings.invertY ? -1.f : 1.f);
    P.pitch = Clamp(P.pitch, -1.5f, 1.5f);
    P.yaw = AngleWrap(P.yaw);
    if (IsKeyPressed(KEY_V)) P.thirdPerson = !P.thirdPerson;
    if (IsKeyPressed(KEY_L)) {
        P.pipLight = !P.pipLight;
        PlaySfx(SFX_UI_MOVE);
    }
    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_LEFT_CONTROL)) P.crouch = !P.crouch;

    // --- movimento ---
    Vector3 fwd = YawDir(P.yaw), right = V3(-cosf(P.yaw), 0, sinf(P.yaw));
    Vector3 wish = V3(0, 0, 0);
    if (IsKeyDown(KEY_W)) wish = wish + fwd;
    if (IsKeyDown(KEY_S)) wish = wish - fwd;
    if (IsKeyDown(KEY_D)) wish = wish + right;
    if (IsKeyDown(KEY_A)) wish = wish - right;
    bool moving = Vector3Length(wish) > 0.1f;
    if (moving) wish = Vector3Normalize(wish);
    bool aiming = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !CurrentWeapon().melee;
    P.aimT = LerpF(P.aimT, aiming ? 1.0f : 0.0f, Clamp01(dt * 12));
    bool encumbered = P.inv.Weight() > CarryMax();
    P.sprint = IsKeyDown(KEY_LEFT_SHIFT) && moving && P.ap > 1 && !encumbered && !aiming && IsKeyDown(KEY_W);
    if (P.sprint && P.crouch) P.crouch = false;
    float speed = P.crouch ? 2.1f : (P.sprint ? 7.2f : 4.4f);
    if (aiming) speed = std::min(speed, 2.6f);
    speed *= PlayerSpeedMul();
    if (P.sprint) P.ap = std::max(0.0f, P.ap - 11.0f * dt);
    else P.ap = std::min(MaxAP(), P.ap + (7.0f + Spec(ST_AGI) * 0.7f) * dt);
    Vector3 target = wish * speed;
    float accel = P.onGround ? 14.0f : 2.0f;
    P.vel.x = LerpF(P.vel.x, target.x, Clamp01(dt * accel));
    P.vel.z = LerpF(P.vel.z, target.z, Clamp01(dt * accel));
    if (IsKeyPressed(KEY_SPACE) && P.onGround) {
        P.vel.y = 5.4f;
        P.onGround = false;
        P.crouch = false;
        PlaySfx(SFX_JUMP, 0.5f);
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
            if (fall > 1.2f) PlaySfx(SFX_LAND, Clamp01(fall / 4));
            if (fall > 5.5f) PlayerTakeDamage((fall - 5.5f) * 14.0f, np, true);
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
    }

    // --- armi ---
    const WeaponDef& w = CurrentWeapon();
    P.fireCd -= dt;
    if (P.equipT > 0) P.equipT -= dt;
    if (P.flash > 0) P.flash -= dt;
    P.recoil = std::max(0.0f, P.recoil - dt * 12);
    if (P.reloadT > 0) {
        P.reloadT -= dt;
        if (P.reloadT <= 0) FinishReload();
    }
    if (P.swingT > 0) {
        P.swingT += dt / 0.5f;
        if (P.swingT > 0.38f && !P.swingDone) {
            P.swingDone = true;
            PlayerMeleeHit();
        }
        if (P.swingT >= 1) P.swingT = 0;
    }
    bool trigger = w.automatic ? IsMouseButtonDown(MOUSE_BUTTON_LEFT) : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (trigger && P.fireCd <= 0 && P.reloadT <= 0 && P.equipT <= 0 && P.swingT <= 0) {
        if (P.sprint) P.sprint = false;
        PlayerFire();
    }
    if (IsKeyPressed(KEY_R)) PlayerReload();
    if (IsKeyPressed(KEY_G) && P.fireCd <= 0) {
        if (P.inv.Count(I_GRENADE) > 0) {
            P.inv.Remove(I_GRENADE, 1);
            Vector3 f = PlayerForward();
            float pw = 13.0f + SkillValue(SK_EXPLOSIVES) * 0.05f;
            ThrowGrenade(P.zone, PlayerEye() + f * 0.6f, f * pw + V3(0, 3.0f, 0), true);
            PlaySfx(SFX_THROW);
            P.fireCd = 0.8f;
            P.swingT = 0.001f;
            P.swingDone = true;
        } else Notify("Non hai granate.");
    }
    float wheel = GetMouseWheelMove();
    if (wheel > 0.5f) CycleWeapon(1);
    else if (wheel < -0.5f) CycleWeapon(-1);
    for (int k = 0; k < 9; k++)
        if (IsKeyPressed(KEY_ONE + k)) {
            int id = WeaponSlot(k);
            if (id >= 0 && id != P.weapon) EquipWeapon(id);
        }
    if (IsKeyPressed(KEY_H)) {
        if (P.inv.Count(I_STIMPAK) > 0) UseItem(I_STIMPAK);
        else Notify("Non hai stimpak.");
    }

    // --- interazione ---
    FindInteraction();
    if (IsKeyPressed(KEY_E)) {
        if (G.lookInter >= 0) Interact(G.lookInter);
        else if (G.lookEnt >= 0) {
            Entity& e = G.ents[G.lookEnt];
            if (e.dead) LootOpen(-1, G.lookEnt);
            else if (!e.hostile && !e.dlg.empty()) DialogueStart(G.lookEnt);
        }
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
        P.hp = 0;
        SetGameState(GS_DEAD);
        return;
    }
    if (P.healOverTime > 0) {
        float a = std::min(P.healOverTime, 35.0f * dt);
        P.hp += a;
        P.healOverTime -= a;
    }
    if (P.perks[PK_REGEN] && P.lastCombat > 10) P.hp += 1.5f * dt;
    P.hp = std::min(P.hp, HPCap());
    if (P.radXTimer > 0) P.radXTimer -= dt;
    if (P.mentatsTimer > 0) P.mentatsTimer -= dt;
    if (P.jetTimer > 0) P.jetTimer -= dt;
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
    const WeaponDef& w = CurrentWeapon();
    fov = LerpF(fov, fov / w.zoom, P.aimT);
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
//  Arma in prima persona (disegnata vicinissima alla camera)
// ---------------------------------------------------------------------------
void DrawViewModel() {
    Player& P = G.player;
    if (P.thirdPerson) return;
    const WeaponDef& w = CurrentWeapon();
    Vector3 f = Vector3Normalize(G.cam.target - G.cam.position);
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, V3(0, 1, 0)));
    Vector3 u = Vector3CrossProduct(r, f);
    // nello spazio camera +x è a destra: la base usa r come asse x
    Vector3 eye = G.cam.position;
    Matrix B = Matrix{r.x, u.x, f.x, eye.x, r.y, u.y, f.y, eye.y, r.z, u.z, f.z, eye.z, 0, 0, 0, 1};
    Matrix S = MatrixScale(0.1f, 0.1f, 0.1f) * B;
    float hs = Vector3Length(V3(P.vel.x, 0, P.vel.z));
    float bobAmt = P.onGround ? Clamp01(hs / 6.0f) : 0;
    Vector2 md = GetMouseDelta();
    static Vector2 sway{0, 0};
    sway.x = LerpF(sway.x, Clamp(md.x * 0.0006f, -0.03f, 0.03f), Clamp01(GetFrameTime() * 8));
    sway.y = LerpF(sway.y, Clamp(md.y * 0.0006f, -0.03f, 0.03f), Clamp01(GetFrameTime() * 8));
    float aim = P.aimT;
    Vector3 hip = w.melee ? V3(0.2f, -0.24f, 0.55f) : V3(0.16f, -0.165f, 0.52f);
    Vector3 ads = V3(0.0f, -0.072f, 0.42f);
    if (w.model == WM_RIFLE) ads = V3(0.0f, -0.102f, 0.34f);
    if (w.model == WM_MINIGUN) { hip = V3(0.16f, -0.22f, 0.5f); ads = V3(0.08f, -0.18f, 0.45f); }
    Vector3 pos = Vector3Lerp(hip, ads, aim);
    pos = pos + V3(sinf(P.bob) * 0.012f * bobAmt * (1 - aim * 0.8f) - sway.x, fabsf(cosf(P.bob)) * 0.01f * bobAmt * (1 - aim * 0.8f) + sway.y, -P.recoil * 0.004f);
    float pitch = -P.recoil * 0.02f, yawA = 0, roll = 0;
    if (P.sprint) {
        pos = pos + V3(0.05f, -0.06f, -0.04f);
        yawA += 0.6f;
        pitch += 0.2f;
    }
    if (P.reloadT > 0) {
        float t = 1.0f - P.reloadT / (w.reload * (P.perks[PK_FASTRELOAD] ? 0.6f : 1.0f));
        float k = sinf(Clamp01(t) * PI);
        pos.y -= 0.12f * k;
        pitch += 0.6f * k;
        roll += 0.4f * k;
    }
    if (P.equipT > 0) pos.y -= P.equipT * 0.6f;
    if (P.swingT > 0) {
        float t = P.swingT;
        float k1 = t < 0.35f ? t / 0.35f : 1.0f, k2 = t < 0.35f ? 0 : (t - 0.35f) / 0.65f;
        float s2 = sinf(k2 * PI);
        if (w.model == WM_FIST) pos = pos + V3(-0.08f * s2, 0.04f * s2, 0.2f * s2);
        else {
            pos = pos + V3(0.05f * k1 - 0.25f * s2, 0.12f * k1 - 0.1f * s2, 0.08f * s2);
            pitch += -1.0f * k1 + 1.8f * s2;
            yawA += 0.8f * s2;
            roll += 0.5f * s2;
        }
    }
    Matrix wm = MatrixRotateXYZ(V3(pitch, yawA, roll)) * MatrixTranslate(pos.x, pos.y, pos.z);
    Color sleeve = OutfitColor(ArmorOutfit()), skin = Col(215, 170, 140);
    if (P.armor == I_POWER) skin = Col(95, 98, 88);
    if (w.model == WM_FIST) {
        for (int s = -1; s <= 1; s += 2) {
            float jab = (s > 0 && P.swingT > 0.35f) ? sinf((P.swingT - 0.35f) / 0.65f * PI) : 0;
            Vector3 fist = V3(s * 0.15f, -0.2f + jab * 0.05f, 0.45f + jab * 0.2f);
            if (P.equipT > 0) fist.y -= P.equipT * 0.6f;
            fist = fist + V3(sinf(P.bob) * 0.01f * bobAmt, 0, 0);
            DrawLimb(S, V3(s * 0.25f, -0.5f, 0.2f), fist - V3(0, 0, 0.05f), 0.032f, sleeve);
            DrawLimb(S, fist - V3(0, 0, 0.1f), fist - V3(0, 0, 0.03f), 0.03f, skin);
            DrawPrim(PR_SPHERE, MatTRS(fist, V3(0, 0, 0), V3(0.075f, 0.068f, 0.085f)) * S, skin);
        }
        return;
    }
    Matrix W = wm * S;
    float spin = (float)GetTime() * (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 25.0f : 2.0f);
    DrawWeaponModel(W, w.model, P.flash > 0 ? 1.0f : 0.0f, spin);
    // mani e braccia
    Vector3 grip = Vector3Transform(V3(0, -0.04f, 0), wm);
    DrawLimb(S, V3(0.3f, -0.5f, 0.22f), grip + V3(0.01f, -0.03f, -0.05f), 0.032f, sleeve);
    DrawPrim(PR_CUBE, MatTRS(V3(0, -0.04f, -0.01f), V3(0, 0, 0), V3(0.05f, 0.06f, 0.08f)) * W, skin);
    if (!w.melee && w.model != WM_GRENADE) {
        Vector3 fore = Vector3Transform(V3(0, -0.02f, w.twoHanded ? 0.22f : 0.04f), wm);
        DrawLimb(S, V3(-0.26f, -0.5f, 0.28f), fore + V3(-0.02f, -0.03f, -0.04f), 0.032f, sleeve);
        DrawPrim(PR_CUBE, MatTRS(V3(-0.025f, -0.035f, w.twoHanded ? 0.22f : 0.04f), V3(0, 0, 0), V3(0.05f, 0.05f, 0.08f)) * W, skin);
    }
    // vampata
    if (P.flash > 0 && !w.melee) {
        float L = w.model == WM_RIFLE ? 0.78f : (w.model == WM_SHOTGUN ? 0.47f : (w.model == WM_ASSAULT ? 0.58f : (w.model == WM_MINIGUN ? 0.78f : (w.model == WM_LASERRIFLE || w.model == WM_PLASMA ? 0.62f : 0.18f))));
        Color fc = w.energy ? w.beam : Col(255, 210, 120);
        float sz = 0.05f + gRng.range(0, 0.04f);
        DrawPrim(PR_SPHERE, MatTRS(V3(0, 0.03f, L), V3(0, 0, gRng.range(0, 3)), V3(sz, sz, sz * 2)) * W, fc, 1.0f);
    }
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
    const WeaponDef& w = CurrentWeapon();
    float hs = Vector3Length(V3(P.vel.x, 0, P.vel.z));
    float aim = w.melee ? 0 : (P.lastCombat < 3 || P.aimT > 0.1f || IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 1.0f : 0.3f);
    DrawHumanoid(P.pos, P.yaw, look, P.bob * 1.1f, Clamp01(hs / 4.5f), aim, P.swingT, 0, w.model, P.flash > 0 ? 1 : 0, 1.0f, false, P.crouch ? 1.0f : 0.0f);
}
