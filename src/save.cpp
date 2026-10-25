// Salvataggi e impostazioni.
#include "game.h"
#include "platform.h"
#include <fstream>
#include <sstream>

static std::string SaveDir() { return DataDir() + "saves/"; }
static std::string SlotPath(int slot) { return SaveDir() + Fmt("slot%d.sav", slot); }
static std::string SettingsPath() { return DataDir() + "settings.cfg"; }

bool SaveExists(int slot) { return FileExists(SlotPath(slot).c_str()); }
long SaveTime(int slot) { return GetFileModTime(SlotPath(slot).c_str()); }

static void WriteInv(std::ostream& o, const Inventory& inv) {
    o << inv.items.size();
    for (auto& s : inv.items) o << ' ' << s.id << ' ' << s.count;
    o << '\n';
}

static void ReadInv(std::istream& in, Inventory& inv) {
    inv.Clear();
    size_t n = 0;
    in >> n;
    for (size_t i = 0; i < n && in; i++) {
        int id, c;
        in >> id >> c;
        if (id >= 0 && id < I_COUNT && c > 0) inv.Add(id, c);
    }
}

bool SaveGame(int slot) {
    if (!DirectoryExists(SaveDir().c_str())) MakeDirectory(SaveDir().c_str());
    std::ofstream o(SlotPath(slot));
    if (!o) {
        Notify("Impossibile salvare la partita!", Col(255, 120, 100));
        return false;
    }
    Player& P = G.player;
    float dist;
    int li = NearestLocation(P.pos, &dist);
    std::string where = P.zone != ZONE_WASTE ? W.zones[P.zone].name : (li >= 0 && dist < 120 ? W.locations[li].name : std::string("Terre Bruciate"));
    o << "DUSTFALL_SAVE 2\n";
    o << P.name << '\n';
    o << where << '\n';
    o << P.level << ' ' << G.day << ' ' << G.timeOfDay << '\n';
    o << P.female << '\n';
    for (int i = 0; i < ST_COUNT; i++) o << P.special[i] << ' ';
    o << '\n';
    for (int i = 0; i < SK_COUNT; i++) o << P.skillPts[i] << ' ' << P.tagged[i] << ' ';
    o << '\n';
    o << PK_COUNT;
    for (int i = 0; i < PK_COUNT; i++) o << ' ' << P.perks[i];
    o << '\n';
    o << P.level << ' ' << P.xp << ' ' << P.pendingLevels << '\n';
    o << P.hp << ' ' << P.ap << ' ' << P.rads << ' ' << P.radXTimer << ' ' << P.coffeeTimer << ' ' << P.teaTimer << ' ' << P.stealthT << '\n';
    o << P.tool << ' ' << P.armor << ' ' << P.karma << '\n';
    o << P.zone << ' ' << P.pos.x << ' ' << P.pos.y << ' ' << P.pos.z << ' ' << P.yaw << ' ' << P.pitch << ' ' << P.thirdPerson << '\n';
    o << P.timesCaught << ' ' << P.locksPicked << ' ' << P.termsHacked << ' ' << P.questsDone << ' ' << P.capsEarned << ' ' << P.medkitsUsed << ' '
      << P.repairs << ' ' << P.distractions << ' ' << P.walked << ' ' << P.playTime << '\n';
    WriteInv(o, P.inv);
    o << P.photos.size();
    for (auto& k : P.photos) o << ' ' << k;
    o << '\n';
    o << G.flags.size() << '\n';
    for (auto& kv : G.flags) o << kv.first << ' ' << kv.second << '\n';
    for (int q = 0; q < Q_COUNT; q++) o << QUESTS[q].stage << ' ' << QUESTS[q].done << ' ';
    o << '\n';
    o << W.locations.size();
    for (auto& l : W.locations) o << ' ' << l.discovered;
    o << '\n';
    o << G.ents.size() << '\n';
    for (auto& e : G.ents) {
        o << e.zone << ' ' << e.pos.x << ' ' << e.pos.y << ' ' << e.pos.z << ' ' << e.yaw << ' ' << e.hostile << ' ' << e.off << ' ' << e.asleep << ' '
          << e.companion << ' ' << e.home.x << ' ' << e.home.y << ' ' << e.home.z << ' ' << e.territory << '\n';
    }
    o << gInterTemplates.size() << '\n';
    for (size_t i = 0; i < gInterTemplates.size(); i++) {
        auto& it = G.inters[i];
        o << it.locked << ' ' << it.open << ' ' << it.used << ' ' << it.hacked << ' ' << it.lockedOut << ' ';
        WriteInv(o, it.inv);
    }
    o << G.inters.size() - gInterTemplates.size() << '\n';
    for (size_t i = gInterTemplates.size(); i < G.inters.size(); i++) {
        auto& it = G.inters[i];
        o << it.zone << ' ' << it.pos.x << ' ' << it.pos.y << ' ' << it.pos.z << ' ' << it.used << ' ';
        WriteInv(o, it.inv);
    }
    o << G.vendors.size() << '\n';
    for (auto& v : G.vendors) {
        o << v.caps << ' ';
        WriteInv(o, v.inv);
    }
    o << gRadioStation << '\n';
    o.close();
    PlatformSyncStorage();
    return !o.fail();
}

std::string SaveInfo(int slot) {
    std::ifstream in(SlotPath(slot));
    if (!in) return "";
    std::string magic, name, where;
    std::getline(in, magic);
    if (magic.rfind("DUSTFALL_SAVE 2", 0) != 0) return "Salvataggio di una versione precedente (non compatibile)";
    std::getline(in, name);
    std::getline(in, where);
    int level = 1, day = 1;
    float tod = 0;
    in >> level >> day >> tod;
    return Fmt("%s  -  Liv. %d  -  %s  -  Giorno %d, %02d:%02d", name.c_str(), level, where.c_str(), day, (int)tod, (int)((tod - (int)tod) * 60));
}

bool LoadGame(int slot) {
    std::ifstream in(SlotPath(slot));
    if (!in) return false;
    std::string magic, name, where;
    std::getline(in, magic);
    if (magic.rfind("DUSTFALL_SAVE 2", 0) != 0) {
        Notify("Salvataggio non valido.", Col(255, 120, 100));
        return false;
    }
    NewGame();
    Player& P = G.player;
    std::getline(in, name);
    std::getline(in, where);
    P.name = name;
    int lvl0, day;
    float tod;
    in >> lvl0 >> day >> tod;
    G.day = day;
    G.timeOfDay = tod;
    in >> P.female;
    for (int i = 0; i < ST_COUNT; i++) in >> P.special[i];
    for (int i = 0; i < SK_COUNT; i++) in >> P.skillPts[i] >> P.tagged[i];
    int pc = 0;
    in >> pc;
    for (int i = 0; i < pc; i++) {
        bool b;
        in >> b;
        if (i < PK_COUNT) P.perks[i] = b;
    }
    in >> P.level >> P.xp >> P.pendingLevels;
    in >> P.hp >> P.ap >> P.rads >> P.radXTimer >> P.coffeeTimer >> P.teaTimer >> P.stealthT;
    in >> P.tool >> P.armor >> P.karma;
    in >> P.zone >> P.pos.x >> P.pos.y >> P.pos.z >> P.yaw >> P.pitch >> P.thirdPerson;
    in >> P.timesCaught >> P.locksPicked >> P.termsHacked >> P.questsDone >> P.capsEarned >> P.medkitsUsed >> P.repairs >> P.distractions >> P.walked >> P.playTime;
    ReadInv(in, P.inv);
    size_t np = 0;
    in >> np;
    P.photos.clear();
    for (size_t i = 0; i < np && in; i++) {
        std::string k;
        in >> k;
        P.photos.push_back(k);
    }
    size_t nf = 0;
    in >> nf;
    G.flags.clear();
    for (size_t i = 0; i < nf; i++) {
        std::string k;
        int v;
        in >> k >> v;
        G.flags[k] = v;
    }
    for (int q = 0; q < Q_COUNT; q++) in >> QUESTS[q].stage >> QUESTS[q].done;
    for (int q = CAMPAIGN_QUEST_COUNT; q < Q_COUNT; q++) QUESTS[q] = Quest{QUESTS[q].title};
    P.questsDone = 0;
    for (int q = 0; q < CAMPAIGN_QUEST_COUNT; q++) P.questsDone += QUESTS[q].done;
    if (!QUESTS[Q_SIGNAL].done && QUESTS[Q_SIGNAL].stage > 20) QUESTS[Q_SIGNAL].stage = 20;
    if (!G.flags["original_blaster_granted"]) {
        if (!P.inv.Count(I_BLASTER)) P.inv.Add(I_BLASTER);
        G.flags["original_blaster_granted"] = 1;
    }
    size_t nl = 0;
    in >> nl;
    for (size_t i = 0; i < nl; i++) {
        bool d;
        in >> d;
        if (i < W.locations.size()) W.locations[i].discovered = d;
    }
    size_t ne = 0;
    in >> ne;
    for (size_t i = 0; i < ne && in; i++) {
        Entity tmp;
        Entity& e = i < G.ents.size() ? G.ents[i] : tmp;
        in >> e.zone >> e.pos.x >> e.pos.y >> e.pos.z >> e.yaw >> e.hostile >> e.off >> e.asleep >> e.companion >> e.home.x >> e.home.y >> e.home.z >> e.territory;
        e.zone = Clamp(e.zone, 0, ZONE_COUNT - 1);
        e.ai = e.companion ? AI_FOLLOW : AI_IDLE;
        e.aware = false;
        e.alert = 0;
    }
    size_t ni = 0;
    in >> ni;
    for (size_t i = 0; i < ni && in; i++) {
        Interactable tmp;
        Interactable& it = i < G.inters.size() ? G.inters[i] : tmp;
        in >> it.locked >> it.open >> it.used >> it.hacked >> it.lockedOut;
        ReadInv(in, it.inv);
        it.anim = it.open ? 1.0f : 0.0f;
        if (it.collider >= 0) W.zones[it.zone].boxes[it.collider].enabled = !it.open;
    }
    size_t nx = 0;
    in >> nx;
    for (size_t i = 0; i < nx && in; i++) {
        Interactable it;
        it.type = INT_ITEM;
        in >> it.zone >> it.pos.x >> it.pos.y >> it.pos.z >> it.used;
        ReadInv(in, it.inv);
        it.size = V3(0.4f, 0.3f, 0.4f);
        it.name = it.inv.Empty() ? "Oggetto" : ITEMS[it.inv.items[0].id].name;
        G.inters.push_back(it);
    }
    size_t nv = 0;
    in >> nv;
    for (size_t i = 0; i < nv && in; i++) {
        Vendor tmp;
        Vendor& v = i < G.vendors.size() ? G.vendors[i] : tmp;
        in >> v.caps;
        ReadInv(in, v.inv);
    }
    int radio = 0;
    in >> radio;
    // I vecchi salvataggi della campagna fotografica potevano aver gia' svuotato lo zaino.
    // Il biglietto e la chiave originali devono restare recuperabili senza rifare la partita.
    if (!QUESTS[Q_SIGNAL].done && !P.inv.Count(I_CACHEKEY)) {
        Interactable* cache = FindInterByTag("cache");
        Interactable* pack = FindInterByTag("scout_pack");
        if (cache && cache->locked && pack) {
            if (!pack->inv.Count(I_CACHEKEY)) pack->inv.Add(I_CACHEKEY);
            if (!P.inv.Count(I_SCOUTNOTE) && !pack->inv.Count(I_SCOUTNOTE)) pack->inv.Add(I_SCOUTNOTE);
        }
    }
    SetRadioStation(radio);
    if (P.tool < 0 || P.tool >= TOOL_COUNT || (P.tool != TOOL_HANDS && P.inv.Count(ToolItem(P.tool)) == 0)) P.tool = TOOL_HANDS;
    if (P.armor >= I_COUNT || (P.armor >= 0 && P.inv.Count(P.armor) == 0)) P.armor = -1;
    P.zone = Clamp(P.zone, 0, ZONE_COUNT - 1);
    G.gameStarted = true;
    SetGameState(GS_PLAY);
    Notify("Partita caricata.");
    return true;
}

void SaveSettings() {
    std::ofstream o(SettingsPath());
    if (!o) return;
    Settings& s = G.settings;
    o << "sensitivity " << s.sensitivity << '\n'
      << "volume " << s.volume << '\n'
      << "radio " << s.radioVolume << '\n'
      << "fov " << s.fov << '\n'
      << "invertY " << s.invertY << '\n'
      << "showFps " << s.showFps << '\n'
      << "fullscreen " << (DUSTFALL_WEB ? false : s.fullscreen) << '\n'; // nel browser lo schermo intero richiede un clic
    o.close();
    PlatformSyncStorage();
}

void LoadSettings() {
    std::ifstream in(SettingsPath());
    if (!in) return;
    Settings& s = G.settings;
    std::string k;
    while (in >> k) {
        if (k == "sensitivity") in >> s.sensitivity;
        else if (k == "volume") in >> s.volume;
        else if (k == "radio") in >> s.radioVolume;
        else if (k == "fov") in >> s.fov;
        else if (k == "invertY") in >> s.invertY;
        else if (k == "showFps") in >> s.showFps;
        else if (k == "fullscreen") in >> s.fullscreen;
        else { std::string skip; in >> skip; }
    }
    s.sensitivity = Clamp(s.sensitivity, 0.2f, 3.0f);
    s.volume = Clamp(s.volume, 0.0f, 1.0f);
    s.radioVolume = Clamp(s.radioVolume, 0.0f, 1.0f);
    s.fov = Clamp(s.fov, 55.0f, 110.0f);
}

void ApplySettings() {
    SetMasterVolume(G.settings.volume);
    if (G.settings.fullscreen != PlatformIsFullscreen()) PlatformSetFullscreen(G.settings.fullscreen);
}
