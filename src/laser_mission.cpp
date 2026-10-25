// SOS Lucciola: una piccola missione a impulsi, condivisa con la stanza.
// Il progresso personale usa i flag gia' salvati dal gioco: nessuna modifica al formato dei salvataggi.
#include "game.h"
#include "net.h"
#include "platform.h"

namespace {
struct RelaySite { float x, z; const char* name; };
const RelaySite kRelays[] = {
    {0, 70, "Cancello di Ferrocitta"},
    {42, 164, "Stazione Atomo"},
    {-145, 151, "Fattoria abbandonata"}
};
const Color kLaser = {100, 245, 255, 255};
Vector3 sDronePos{};
bool sDroneFollowing = false;

bool MissionFlag(const std::string& name) {
    auto f = G.flags.find(name);
    return f != G.flags.end() && f->second != 0;
}

std::string RelayFlag(int i) { return "lucciola_relay" + std::to_string(i); }

Vector3 RelayPos(int i) {
    const auto& r = kRelays[i];
    return V3(r.x, TerrainHeight(r.x, r.z) + 1.6f, r.z);
}

Vector3 CrashPos() { return V3(-270, TerrainHeight(-270, -10) + 0.55f, -10); }

int RepairedCount() {
    int count = 0;
    for (int i = 0; i < 3; ++i) count += MissionFlag(RelayFlag(i));
    return count;
}

bool NearDrone() {
    return G.player.zone == ZONE_WASTE && Vector3Distance(G.player.pos, CrashPos()) < 4.5f;
}

void DrawDrone(Vector3 p, bool repaired) {
    float bob = repaired ? sinf(G.time * 3.4f) * 0.09f : sinf(G.time * 5.0f) * 0.025f;
    p.y += bob;
    DrawCube(p, 0.62f, 0.30f, 0.45f, Col(52, 65, 77));
    DrawCubeWires(p, 0.64f, 0.32f, 0.47f, Col(110, 135, 150));
    DrawSphere(p + V3(0, 0.02f, -0.25f), 0.105f, repaired ? kLaser : Col(255, 170, 65));
    DrawCylinderEx(p + V3(0, 0.15f, 0), p + V3(0, 0.46f, 0), 0.018f, 0.018f, 6, Col(140, 160, 165));
    DrawSphere(p + V3(0, 0.47f, 0), 0.045f, repaired ? kLaser : Col(255, 80, 55));
    for (int side : {-1, 1}) {
        Vector3 rotor = p + V3(side * 0.47f, 0.12f, 0);
        DrawCylinderEx(p + V3(side * 0.23f, 0.08f, 0), rotor, 0.025f, 0.025f, 6, Col(100, 120, 128));
        DrawCylinder(rotor, 0.20f, 0.20f, 0.025f, 12, Col(55, 80, 90));
        if (repaired) {
            float a = G.time * 28.0f;
            Vector3 blade = V3(cosf(a) * 0.23f, 0.025f, sinf(a) * 0.23f);
            DrawLine3D(rotor - blade, rotor + blade, kLaser);
        }
    }
}
}

void LaserMissionUpdate(float dt) {
    if (G.state != GS_PLAY || G.player.zone != ZONE_WASTE) {
        if (G.player.zone != ZONE_WASTE || !MissionFlag("lucciola_done")) sDroneFollowing = false;
        return;
    }
    if (!MissionFlag("lucciola_started")) {
        G.flags["lucciola_started"] = 1;
        if (G.player.inv.Count(I_BLASTER) == 0) GiveItem(I_BLASTER, 1, false);
        BigNotify("SOS LUCCIOLA", PlatformTouch() ? "Un drone e' disperso! Scegli il blaster con ATTREZZO e riattiva i ripetitori." : "Un drone e' disperso! Tasto 5: blaster. Riattiva i tre ripetitori a impulsi.");
        Notify("Ripetitori: cancello di Ferrocitta, Stazione Atomo e fattoria. Cercali sulla mappa del Pip-Boy.", kLaser);
    }
    int shared = NetLaserRelays();
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
        if ((shared & (1 << i)) && !MissionFlag(RelayFlag(i))) {
            G.flags[RelayFlag(i)] = 1;
            changed = true;
        }
    }
    if (changed) {
        Notify(Fmt("SOS Lucciola: segnale della squadra ricevuto! Ripetitori %d/3.", RepairedCount()), kLaser);
        if (RepairedCount() == 3) BigNotify("SEGNALE AGGANCIATO", "Lucciola e' ai piedi della Torre Radio. Raggiungila e premi E!");
    }
    if (MissionFlag("lucciola_done")) {
        Vector3 target = G.player.pos + V3(-1.5f, 2.1f, 1.8f);
        if (!sDroneFollowing || Vector3Distance(sDronePos, target) > 25) sDronePos = target;
        else sDronePos = Vector3Lerp(sDronePos, target, 1.0f - expf(-std::min(dt, 0.1f) * 4.0f));
        sDroneFollowing = true;
    } else sDroneFollowing = false;
}

bool LaserMissionHit(Vector3 origin, Vector3 direction, float maxDistance, Vector3& hit) {
    if (G.player.zone != ZONE_WASTE || !MissionFlag("lucciola_started")) return false;
    Ray ray{origin, direction};
    int best = -1;
    float nearest = maxDistance;
    for (int i = 0; i < 3; ++i) {
        Vector3 p = RelayPos(i);
        RayCollision h = GetRayCollisionSphere(ray, p, 0.65f);
        if (!h.hit || h.distance < 0 || h.distance >= nearest || !LineOfSight(ZONE_WASTE, origin, p)) continue;
        best = i;
        nearest = h.distance;
        hit = h.point;
    }
    if (best < 0) return false;
    NetLaserRelay(best); // Solo un nuovo colpo condivide il ripetitore: mai i vecchi flag di un salvataggio.
    if (!MissionFlag(RelayFlag(best))) {
        G.flags[RelayFlag(best)] = 1;
        PlaySfx(SFX_DISCOVER, 0.7f);
        Notify(Fmt("Ripetitore attivato: %s (%d/3)", kRelays[best].name, RepairedCount()), kLaser);
        if (RepairedCount() == 3) BigNotify("SEGNALE AGGANCIATO", "Lucciola e' ai piedi della Torre Radio. Raggiungila e premi E!");
    } else Notify(std::string("Ripetitore gia' attivo: ") + kRelays[best].name, kLaser);
    return true;
}

bool LaserMissionInteract() {
    if (G.state != GS_PLAY || !MissionFlag("lucciola_started") || MissionFlag("lucciola_done") || !NearDrone()) return false;
    if (RepairedCount() < 3) {
        Notify(Fmt("Lucciola: segnale troppo debole. Riattiva gli altri ripetitori (%d/3).", RepairedCount()), kLaser);
        return true;
    }
    G.flags["lucciola_done"] = 1; // Prima delle ricompense: l'interazione ripetuta non paga di nuovo.
    GiveXP(200, "SOS Lucciola");
    GiveItem(I_CAPS, 150, false);
    BigNotify("LUCCIOLA E' SALVA!", "+200 PE, +150 tappi. Il piccolo drone ora ti accompagna nelle Terre Bruciate.");
    PlaySfx(SFX_DISCOVER, 0.9f);
    return true;
}

void LaserMissionDraw3D() {
    if (!MissionFlag("lucciola_started")) return;
    if (G.player.zone == ZONE_WASTE) {
        for (int i = 0; i < 3; ++i) {
            Vector3 p = RelayPos(i);
            if (Vector3Distance(p, G.cam.position) > 350) continue;
            bool repaired = MissionFlag(RelayFlag(i));
            Color color = repaired ? kLaser : Col(255, 185, 70);
            Vector3 base = p - V3(0, 1.6f, 0);
            DrawCylinder(base + V3(0, 0.1f, 0), 0.48f, 0.60f, 0.2f, 10, Col(55, 66, 75));
            DrawCylinder(base + V3(0, 0.85f, 0), 0.13f, 0.20f, 1.5f, 8, Col(95, 110, 120));
            DrawSphere(p, 0.34f, color);
            DrawSphereWires(p, 0.58f, 5, 10, color);
            float pulse = 0.60f + sinf(G.time * 3 + i) * 0.12f;
            DrawCylinderWires(base + V3(0, 0.03f, 0), pulse, pulse, 0.04f, 16, color);
            DrawLine3D(p + V3(0, 0.6f, 0), p + V3(0, repaired ? 2.1f : 5.5f, 0), color);
        }
        if (!MissionFlag("lucciola_done")) DrawDrone(CrashPos(), false);
    }
    if (G.player.zone == ZONE_WASTE && MissionFlag("lucciola_done") && sDroneFollowing) DrawDrone(sDronePos, true);
}

void LaserMissionDrawHud() {
    if (G.state != GS_PLAY || !MissionFlag("lucciola_started")) return;
    bool done = MissionFlag("lucciola_done");
    bool near = NearDrone();
    if (G.player.tool != TOOL_BLASTER && !near) return;
    float width = std::min(350.0f, UIW() - 32.0f);
    float x = PlatformTouch() ? UIW() / 2 - width / 2 : 20;
    float y = PlatformTouch() ? 430 : 300;
    float height = done ? 58 : 150;
    UIRect(x, y, width, height, Col(4, 20, 28, 195));
    UIRectLines(x, y, width, height, 1, Col(60, 160, 180));
    UIText(done ? "SOS LUCCIOLA - MISSIONE COMPIUTA" : Fmt("SOS LUCCIOLA - RIPETITORI %d/3", RepairedCount()), x + 10, y + 8, 17, kLaser);
    if (done) {
        UIText("Lucciola ti segue. Grazie, viandante!", x + 10, y + 32, 15, PIP_DIM);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        bool repaired = MissionFlag(RelayFlag(i));
        std::string label = repaired ? "[OK] " : "[  ] ";
        label += kRelays[i].name;
        if (!repaired && G.player.zone == ZONE_WASTE) label += Fmt("  %dm", (int)DistXZ(G.player.pos, RelayPos(i)));
        UIText(label, x + 10, y + 35 + i * 22, 15, repaired ? kLaser : Col(255, 210, 125));
    }
    std::string hint;
    if (near) hint = RepairedCount() == 3 ? "E: recupera Lucciola (+200 PE, 150 tappi)" : "Lucciola attende i tre segnali.";
    else if (RepairedCount() == 3) hint = Fmt("Raggiungi Lucciola: Torre Radio, %dm. Poi E.", (int)DistXZ(G.player.pos, CrashPos()));
    else hint = PlatformTouch() ? "ATTREZZO: blaster. Mira ai globi e premi USA." : "5: blaster. Mira ai globi e spara per attivarli.";
    UIText(hint, x + 10, y + 112, 14, PIP);
    UIText("Gli amici possono riattivare i ripetitori con te.", x + 10, y + 131, 13, PIP_DIM);
}

Vector3 LaserRelayPosition(int index) {
    return index >= 0 && index < 3 ? RelayPos(index) : V3(0, 0, 0);
}

Vector3 LaserMissionDronePosition() { return CrashPos(); }

void LaserMissionMapDraw(const std::function<Vector2(Vector3)>& toMap, std::string& hover) {
    if (!MissionFlag("lucciola_started") || MissionFlag("lucciola_done")) return;
    float scale = UIS();
    for (int i = 0; i < 3; ++i) {
        Vector2 p = toMap(RelayPos(i));
        bool repaired = MissionFlag(RelayFlag(i));
        bool hovered = UIHover(p.x - 9, p.y - 9, 18, 18);
        Color color = repaired ? Col(70, 145, 150) : (hovered ? Col(220, 255, 255) : kLaser);
        DrawPoly(V2(p.x * scale, p.y * scale), 6, 7 * scale, 30, color);
        DrawPoly(V2(p.x * scale, p.y * scale), 6, 3.5f * scale, 30, PIP_BG);
        if (hovered) hover = std::string("SOS Lucciola: ") + kRelays[i].name + (repaired ? " (attivo)" : " (mira con il blaster)");
    }
    if (RepairedCount() == 3) {
        Vector2 p = toMap(CrashPos());
        DrawPoly(V2(p.x * scale, p.y * scale), 5, 9 * scale, -90, Col(255, 225, 125));
        if (UIHover(p.x - 10, p.y - 10, 20, 20)) hover = "Lucciola: raggiungi il drone e premi E";
    }
}
