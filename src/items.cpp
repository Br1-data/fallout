// Oggetti, armi, inventario, bottino, abilità e talenti.
#include "game.h"

const char* SPECIAL_NAMES[ST_COUNT] = {"Forza", "Percezione", "Resistenza", "Carisma", "Intelligenza", "Agilità", "Fortuna"};
const char* SPECIAL_DESC[ST_COUNT] = {
    "Potenza fisica. Aumenta il peso trasportabile e il danno in mischia.",
    "Sensi acuti. Migliora la precisione nello S.P.A.T. e le armi a energia.",
    "Robustezza. Aumenta i punti ferita e la resistenza alle radiazioni.",
    "Fascino e presenza. Migliora i prezzi e le opzioni di dialogo.",
    "Acume mentale. Più punti abilità a ogni livello, scienza e medicina.",
    "Coordinazione. Aumenta i Punti Azione e le armi leggere.",
    "Il favore del destino. Aumenta i colpi critici e tutte le abilità."};

const char* SKILL_NAMES[SK_COUNT] = {"Armi leggere", "Armi a energia", "Mischia", "Esplosivi", "Medicina", "Scasso", "Scienza", "Baratto", "Eloquenza", "Furtività"};
const char* SKILL_DESC[SK_COUNT] = {
    "Precisione e danno con pistole, fucili e mitragliatrici.",
    "Precisione e danno con armi laser e al plasma.",
    "Danno con armi da mischia e a mani nude.",
    "Danno e gittata delle granate.",
    "Efficacia di stimpak e Rad-Via.",
    "Capacità di forzare serrature con le forcine.",
    "Capacità di violare i terminali protetti.",
    "Prezzi migliori con i mercanti.",
    "Persuasione nei dialoghi.",
    "Capacità di muoversi senza essere notati."};
const int SKILL_GOV[SK_COUNT] = {ST_AGI, ST_PER, ST_STR, ST_PER, ST_INT, ST_PER, ST_INT, ST_CHA, ST_CHA, ST_AGI};

PerkDef PERKS[PK_COUNT] = {
    {"Pellaccia", "+10 alla Resistenza ai Danni.", 2, ST_END, 4},
    {"Pistolero", "+25% danno e precisione con pistole a una mano.", 2, ST_AGI, 4},
    {"Commando", "+25% danno e precisione con armi a due mani.", 4, ST_AGI, 5},
    {"Pugno di Ferro", "+50% danno in mischia e a mani nude.", 2, ST_STR, 5},
    {"Medico da Campo", "Stimpak e Rad-Via sono il 50% più efficaci.", 2, ST_INT, 4},
    {"Stomaco di Piombo", "-50% radiazioni da cibo e acqua.", 2, ST_END, 3},
    {"Resistenza alle Radiazioni", "+25% resistenza alle radiazioni.", 4, ST_END, 5},
    {"Cervello Fino", "+3 punti abilità a ogni livello.", 2, ST_INT, 4},
    {"Schiena Forte", "+50 al peso trasportabile.", 4, ST_STR, 5},
    {"Energia Inesauribile", "+25 Punti Azione massimi.", 4, ST_AGI, 5},
    {"Cecchino", "+25% probabilità di colpire la testa nello S.P.A.T.", 6, ST_PER, 6},
    {"Mano Fortunata", "+5% probabilità di colpo critico.", 4, ST_LCK, 5},
    {"Sanguinario", "+5% danno. I nemici esplodono in modo spettacolare.", 2, ST_LCK, 1},
    {"Cercatore di Fortuna", "Trovi il doppio dei tappi nei contenitori.", 2, ST_LCK, 5},
    {"Comandante Laser", "+25% danno con le armi a energia.", 6, ST_PER, 6},
    {"Ricarica Rapida", "Ricarichi il 40% più velocemente.", 2, ST_AGI, 5},
    {"Datore di Vita", "+30 punti ferita massimi.", 6, ST_END, 5},
    {"Passo Felpato", "+20 Furtività. Correre non ti rende più visibile.", 4, ST_AGI, 5},
    {"Rigenerazione", "Rigeneri lentamente i PF fuori dal combattimento.", 8, ST_END, 6},
    {"Infiltrato", "Il punto giusto delle serrature è più ampio.", 4, ST_PER, 5},
    {"Hacker", "+1 tentativo nei terminali.", 4, ST_INT, 5},
    {"Demolitore", "+50% danno degli esplosivi.", 4, ST_PER, 4},
};

WeaponDef WEAPONS[] = {
    // model,         skill,         ammo,         mag, dmg,  delay, spread, pel, range, reload, ap, auto,  melee, energy, 2h,   sfx,         beam,                zoom, recoil
    {WM_KNIFE,        SK_MELEE,      -1,           0,   14,   0.45f, 0,      1,   2.3f,  0,      12, false, true,  false,  false, SFX_SWING,   {0, 0, 0, 0},        1.0f, 0},
    {WM_PIPE,         SK_MELEE,      -1,           0,   17,   0.70f, 0,      1,   2.4f,  0,      15, false, true,  false,  false, SFX_SWING,   {0, 0, 0, 0},        1.0f, 0},
    {WM_BAT,          SK_MELEE,      -1,           0,   23,   0.80f, 0,      1,   2.5f,  0,      18, false, true,  false,  true,  SFX_SWING,   {0, 0, 0, 0},        1.0f, 0},
    {WM_SLEDGE,       SK_MELEE,      -1,           0,   38,   1.20f, 0,      1,   2.7f,  0,      25, false, true,  false,  true,  SFX_SWING,   {0, 0, 0, 0},        1.0f, 0},
    {WM_PISTOL,       SK_SMALLGUNS,  I_AMMO_10,    12,  17,   0.22f, 2.0f,   1,   120,   1.6f,   17, false, false, false,  false, SFX_PISTOL,  {255, 220, 150, 255}, 1.3f, 1.6f},
    {WM_RIFLE,        SK_SMALLGUNS,  I_AMMO_308,   5,   52,   1.00f, 0.4f,   1,   260,   2.6f,   25, false, false, false,  true,  SFX_RIFLE,   {255, 220, 150, 255}, 3.0f, 4.0f},
    {WM_SHOTGUN,      SK_SMALLGUNS,  I_AMMO_SHELL, 2,   9,    0.35f, 6.5f,   9,   40,    2.4f,   25, false, false, false,  true,  SFX_SHOTGUN, {255, 220, 150, 255}, 1.2f, 6.0f},
    {WM_ASSAULT,      SK_SMALLGUNS,  I_AMMO_556,   24,  12,   0.11f, 2.8f,   1,   150,   2.2f,   20, true,  false, false,  true,  SFX_AUTO,    {255, 220, 150, 255}, 1.6f, 1.0f},
    {WM_LASERPISTOL,  SK_ENERGY,     I_AMMO_CELL,  30,  19,   0.30f, 1.2f,   1,   140,   1.8f,   15, false, false, true,   false, SFX_LASER,   {255, 50, 40, 255},   1.3f, 0.6f},
    {WM_LASERRIFLE,   SK_ENERGY,     I_AMMO_CELL,  24,  30,   0.32f, 0.7f,   1,   200,   2.2f,   20, false, false, true,   true,  SFX_LASER,   {255, 50, 40, 255},   2.0f, 0.8f},
    {WM_PLASMA,       SK_ENERGY,     I_AMMO_MFC,   12,  58,   0.55f, 1.0f,   1,   160,   2.4f,   25, false, false, true,   true,  SFX_PLASMA,  {90, 255, 90, 255},   1.6f, 2.5f},
    {WM_MINIGUN,      SK_SMALLGUNS,  I_AMMO_5MM,   120, 8,    0.06f, 4.5f,   1,   120,   4.5f,   30, true,  false, false,  true,  SFX_MINIGUN, {255, 220, 150, 255}, 1.0f, 0.7f},
    {WM_GRENADE,      SK_EXPLOSIVES, I_GRENADE,    1,   110,  1.00f, 0,      1,   6,     0,      20, false, false, false,  false, SFX_THROW,   {0, 0, 0, 0},        1.0f, 0},
    {WM_FIST,         SK_MELEE,      -1,           0,   6,    0.45f, 0,      1,   2.0f,  0,      10, false, true,  false,  false, SFX_PUNCH,   {0, 0, 0, 0},        1.0f, 0},
};
const int WEAPON_FIST = 13;

ItemDef ITEMS[I_COUNT];

static void Def(int id, const char* name, ItemType t, float w, int v, const char* desc) {
    ItemDef d;
    d.name = name;
    d.type = t;
    d.weight = w;
    d.value = v;
    d.desc = desc;
    ITEMS[id] = d;
}

void ItemsInit() {
    Def(I_CAPS, "Tappi", IT_MISC, 0, 1, "Tappi di bottiglia: la moneta delle Terre Bruciate.");
    Def(I_KNIFE, "Coltello da combattimento", IT_WEAPON, 1, 30, "Lama affilata, veloce e silenziosa.");
    Def(I_PIPE, "Tubo di piombo", IT_WEAPON, 3, 15, "Un tubo pesante. Rozzo ma efficace.");
    Def(I_BAT, "Mazza chiodata", IT_WEAPON, 3, 45, "Una mazza da baseball piena di chiodi.");
    Def(I_SLEDGE, "Supermartello", IT_WEAPON, 15, 180, "Un enorme martello da guerra. Lento, devastante.");
    Def(I_PISTOL, "Pistola 10mm", IT_WEAPON, 3, 120, "Affidabile pistola semiautomatica d'anteguerra.");
    Def(I_HUNTRIFLE, "Fucile da caccia", IT_WEAPON, 6, 220, "Fucile a otturatore con mirino. Lento ma letale.");
    Def(I_SHOTGUN, "Fucile a canne mozze", IT_WEAPON, 4, 180, "Due canne, due colpi, molto rumore.");
    Def(I_ASSAULT, "Fucile d'assalto", IT_WEAPON, 7, 350, "Arma automatica militare da 5,56mm.");
    Def(I_LASERPISTOL, "Pistola laser", IT_WEAPON, 2, 300, "Pistola a energia. Silenziosa e precisa.");
    Def(I_LASERRIFLE, "Fucile laser", IT_WEAPON, 7, 600, "Fucile a energia militare a lunga gittata.");
    Def(I_PLASMA, "Fucile al plasma", IT_WEAPON, 8, 1100, "Spara globi di plasma surriscaldato.");
    Def(I_MINIGUN, "Mitragliatrice rotante", IT_WEAPON, 18, 900, "Sei canne rotanti. Pesantissima.");
    Def(I_GRENADE, "Granata a frammentazione", IT_WEAPON, 0.5f, 25, "Premi G per lanciarla.");
    for (int i = I_KNIFE; i <= I_GRENADE; i++) ITEMS[i].weapon = i - I_KNIFE;

    Def(I_AMMO_10, "Munizioni 10mm", IT_AMMO, 0, 1, "Per la pistola 10mm.");
    Def(I_AMMO_308, "Munizioni .308", IT_AMMO, 0, 3, "Per il fucile da caccia.");
    Def(I_AMMO_SHELL, "Cartucce cal. 12", IT_AMMO, 0, 2, "Per il fucile a canne mozze.");
    Def(I_AMMO_556, "Munizioni 5,56mm", IT_AMMO, 0, 2, "Per il fucile d'assalto.");
    Def(I_AMMO_CELL, "Cella energetica", IT_AMMO, 0, 3, "Per le armi laser.");
    Def(I_AMMO_MFC, "Cella a microfusione", IT_AMMO, 0, 5, "Per il fucile al plasma.");
    Def(I_AMMO_5MM, "Munizioni 5mm", IT_AMMO, 0, 1, "Per la mitragliatrice rotante.");

    Def(I_VAULTSUIT, "Tuta del Rifugio 33", IT_ARMOR, 1, 10, "Blu e gialla, con il numero 33 sulla schiena.");
    Def(I_LEATHER, "Armatura di cuoio", IT_ARMOR, 12, 160, "Cuoio indurito di bramino. Leggera e robusta.");
    Def(I_RAIDER, "Armatura da predone", IT_ARMOR, 15, 120, "Pezzi di copertone e metallo legati insieme.");
    Def(I_METAL, "Armatura di metallo", IT_ARMOR, 25, 300, "Lamiere martellate. Pesante, ma ferma i proiettili.");
    Def(I_COMBAT, "Armatura da combattimento", IT_ARMOR, 20, 600, "Protezione militare d'anteguerra.");
    Def(I_RADSUIT, "Tuta anti-radiazioni", IT_ARMOR, 5, 150, "Riduce fortemente le radiazioni assorbite.");
    Def(I_POWER, "Armatura potenziata", IT_ARMOR, 45, 2500, "Esoscheletro militare. Protezione quasi totale.");
    Def(I_COAT, "Spolverino", IT_ARMOR, 3, 90, "Un lungo cappotto da viaggio.");
    ITEMS[I_VAULTSUIT].dr = 3;  ITEMS[I_VAULTSUIT].look = OF_VAULT;
    ITEMS[I_LEATHER].dr = 12;   ITEMS[I_LEATHER].look = OF_LEATHER;
    ITEMS[I_RAIDER].dr = 14;    ITEMS[I_RAIDER].look = OF_RAIDER;
    ITEMS[I_METAL].dr = 24;     ITEMS[I_METAL].look = OF_GUARD;
    ITEMS[I_COMBAT].dr = 30;    ITEMS[I_COMBAT].look = OF_COMBAT;
    ITEMS[I_RADSUIT].dr = 6;    ITEMS[I_RADSUIT].radRes = 0.45f; ITEMS[I_RADSUIT].look = OF_RADSUIT;
    ITEMS[I_POWER].dr = 55;     ITEMS[I_POWER].radRes = 0.25f;   ITEMS[I_POWER].look = OF_POWER;
    ITEMS[I_COAT].dr = 6;       ITEMS[I_COAT].look = OF_SHERIFF;

    Def(I_STIMPAK, "Stimpak", IT_AID, 0, 60, "Siringa curativa. Ripristina punti ferita.");
    Def(I_RADAWAY, "Rad-Via", IT_AID, 0, 50, "Flebo chelante. Elimina le radiazioni.");
    Def(I_RADX, "Anti-Rad", IT_AID, 0, 40, "Pillole che aumentano la resistenza alle radiazioni.");
    Def(I_COLA, "Cola Atomica", IT_AID, 1, 20, "La bibita preferita d'anteguerra. Un po' radioattiva.");
    Def(I_WATER, "Acqua sporca", IT_AID, 1, 5, "Meglio di niente. Forse.");
    Def(I_PUREWATER, "Acqua purificata", IT_AID, 1, 20, "Acqua pulita. Un lusso.");
    Def(I_MEAT, "Carne di ratto talpa", IT_AID, 1, 8, "Dura e saporita. Leggermente irradiata.");
    Def(I_BEANS, "Fagioli in scatola", IT_AID, 1, 8, "Duecento anni e ancora buoni.");
    Def(I_CRAM, "Carne in scatola", IT_AID, 1, 10, "Carne... di qualche tipo.");
    Def(I_JET, "Scheggia", IT_AID, 0, 40, "Droga: rallenta la percezione del tempo e ricarica i PA.");
    Def(I_MENTATS, "Mentalix", IT_AID, 0, 50, "Droga: +2 Intelligenza e Percezione per due minuti.");
    ITEMS[I_STIMPAK].heal = 45;
    ITEMS[I_RADAWAY].rads = -150;
    ITEMS[I_RADX].radX = 0.5f;
    ITEMS[I_COLA].heal = 12;    ITEMS[I_COLA].rads = 5;    ITEMS[I_COLA].ap = 20;
    ITEMS[I_WATER].heal = 6;    ITEMS[I_WATER].rads = 8;
    ITEMS[I_PUREWATER].heal = 18;
    ITEMS[I_MEAT].heal = 16;    ITEMS[I_MEAT].rads = 4;
    ITEMS[I_BEANS].heal = 12;   ITEMS[I_BEANS].rads = 2;
    ITEMS[I_CRAM].heal = 15;    ITEMS[I_CRAM].rads = 3;
    ITEMS[I_JET].ap = 60;

    Def(I_BOBBYPIN, "Forcina", IT_MISC, 0, 1, "Indispensabile per forzare le serrature.");
    Def(I_SCRAP, "Rottami metallici", IT_MISC, 1, 6, "Pezzi di metallo. I mercanti li comprano.");
    Def(I_TINCAN, "Lattina vuota", IT_MISC, 0.5f, 1, "Vuota.");
    Def(I_TOASTER, "Tostapane rotto", IT_MISC, 3, 5, "Non toste più niente da due secoli.");
    Def(I_TEDDY, "Orsacchiotto", IT_MISC, 1, 4, "Un vecchio orsacchiotto spelacchiato.");
    Def(I_CLOCK, "Sveglia rotta", IT_MISC, 1, 6, "Ferma sulle 9:47.");
    Def(I_BOTTLE, "Bottiglia vuota", IT_MISC, 0.5f, 1, "Un vuoto a rendere.");
    Def(I_FUSE, "Fusibile", IT_MISC, 0.2f, 10, "Componente elettrico.");
    Def(I_CIRCUIT, "Circuito elettronico", IT_MISC, 0.5f, 25, "Componenti d'anteguerra molto ricercati.");
    Def(I_MEDSUPPLIES, "Rifornimenti medici", IT_KEY, 5, 0, "Una cassa di medicinali per la dottoressa Rosi.");
    Def(I_KEYCARD, "Tessera d'accesso Delta-9", IT_KEY, 0, 0, "Apre il cancello della Base Delta-9.");
    Def(I_FUSIONCELL, "Cella di fusione prototipo", IT_KEY, 8, 0, "Il cuore pulsante del Rifugio 33.");
    Def(I_CLAW, "Artiglio dello Squarciatore", IT_MISC, 3, 60, "Prova dell'uccisione della bestia.");
    Def(I_CACHEKEY, "Chiave del deposito", IT_KEY, 0, 0, "Apre il baule nel deposito vicino al Cratere Verde.");
    Def(I_SCOUTNOTE, "Diario dell'esploratore", IT_KEY, 0, 0,
        "\"Giorno 12. Ho nascosto la mia roba nel deposito sul bordo orientale del Cratere Verde. "
        "Il segnale di soccorso è ancora acceso, ma i cani mi hanno trovato. Se qualcuno legge: la chiave è con me.\"");
    Def(I_SAFEKEY, "Chiave della cassaforte", IT_KEY, 0, 0, "Apre la cassaforte di Teschio.");
}

// ---------------------------------------------------------------------------
//  Inventario
// ---------------------------------------------------------------------------
int Inventory::Count(int id) const {
    for (auto& s : items)
        if (s.id == id) return s.count;
    return 0;
}

void Inventory::Add(int id, int n) {
    if (n <= 0) return;
    for (auto& s : items)
        if (s.id == id) { s.count += n; return; }
    items.push_back({id, n});
}

bool Inventory::Remove(int id, int n) {
    for (size_t i = 0; i < items.size(); i++)
        if (items[i].id == id) {
            if (items[i].count < n) return false;
            items[i].count -= n;
            if (items[i].count <= 0) items.erase(items.begin() + i);
            return true;
        }
    return false;
}

float Inventory::Weight() const {
    float w = 0;
    for (auto& s : items) w += ITEMS[s.id].weight * s.count;
    return w;
}

// ---------------------------------------------------------------------------
//  Bottino nei contenitori
// ---------------------------------------------------------------------------
static int RandAmmo(Rng& r, int tier) {
    int pool[] = {I_AMMO_10, I_AMMO_SHELL, I_AMMO_308, I_AMMO_556, I_AMMO_CELL, I_AMMO_MFC};
    int n = tier <= 1 ? 2 : (tier == 2 ? 4 : 6);
    return pool[r.irange(0, n - 1)];
}

static int AmmoQty(int id, Rng& r) {
    switch (id) {
        case I_AMMO_10: return r.irange(8, 24);
        case I_AMMO_308: return r.irange(4, 10);
        case I_AMMO_SHELL: return r.irange(4, 10);
        case I_AMMO_556: return r.irange(12, 30);
        case I_AMMO_CELL: return r.irange(10, 25);
        case I_AMMO_MFC: return r.irange(6, 14);
        default: return r.irange(5, 15);
    }
}

void GenContainerLoot(Inventory& inv, int model, Rng& r, int tier) {
    tier = std::max(tier, 1);
    int junk[] = {I_TINCAN, I_TOASTER, I_TEDDY, I_CLOCK, I_BOTTLE, I_SCRAP};
    auto Junk = [&] { inv.Add(junk[r.irange(0, 5)], 1); };
    switch (model) {
        case CM_AMMOBOX: {
            int n = r.irange(1, 2 + (tier > 2));
            for (int i = 0; i < n; i++) { int a = RandAmmo(r, tier); inv.Add(a, AmmoQty(a, r)); }
            if (r.chance(0.15f * tier)) inv.Add(I_GRENADE, r.irange(1, 2));
            break;
        }
        case CM_FIRSTAID:
            if (r.chance(0.8f)) inv.Add(I_STIMPAK, r.irange(1, tier));
            if (r.chance(0.5f)) inv.Add(I_RADAWAY, 1);
            if (r.chance(0.3f)) inv.Add(I_RADX, 1);
            break;
        case CM_FRIDGE: {
            int food[] = {I_COLA, I_BEANS, I_WATER, I_MEAT, I_CRAM, I_PUREWATER};
            int n = r.irange(1, 3);
            for (int i = 0; i < n; i++) inv.Add(food[r.irange(0, 5)], 1);
            break;
        }
        case CM_DESK:
            if (r.chance(0.7f)) inv.Add(I_CAPS, r.irange(5, 25) * tier);
            if (r.chance(0.5f)) inv.Add(I_BOBBYPIN, r.irange(1, 3));
            if (r.chance(0.5f)) Junk();
            if (r.chance(0.15f)) inv.Add(I_MENTATS);
            break;
        case CM_LOCKER:
            if (r.chance(0.3f + 0.1f * tier)) {
                int arm[] = {I_LEATHER, I_RAIDER, I_METAL, I_COMBAT};
                inv.Add(arm[r.irange(0, std::min(3, tier))]);
            }
            if (r.chance(0.6f)) { int a = RandAmmo(r, tier); inv.Add(a, AmmoQty(a, r)); }
            if (r.chance(0.25f)) {
                int wpn[] = {I_PISTOL, I_SHOTGUN, I_HUNTRIFLE, I_ASSAULT, I_LASERPISTOL};
                inv.Add(wpn[r.irange(0, std::min(4, tier + 1))]);
            }
            if (r.chance(0.4f)) inv.Add(I_STIMPAK);
            break;
        case CM_SAFE:
            inv.Add(I_CAPS, r.irange(40, 100) * tier);
            if (r.chance(0.6f)) { int a = RandAmmo(r, tier + 1); inv.Add(a, AmmoQty(a, r)); }
            if (r.chance(0.5f)) inv.Add(r.chance(0.5f) ? I_JET : I_MENTATS);
            if (r.chance(0.4f)) inv.Add(I_STIMPAK, r.irange(1, 2));
            if (r.chance(0.3f)) inv.Add(I_CIRCUIT);
            break;
        case CM_FOOTLOCKER:
            if (r.chance(0.6f)) inv.Add(I_CAPS, r.irange(5, 30) * tier);
            if (r.chance(0.5f)) { int a = RandAmmo(r, tier); inv.Add(a, AmmoQty(a, r)); }
            if (r.chance(0.4f)) Junk();
            if (r.chance(0.3f)) inv.Add(r.chance(0.5f) ? I_STIMPAK : I_RADAWAY);
            if (r.chance(0.3f)) inv.Add(I_BOBBYPIN, r.irange(1, 4));
            break;
        case CM_TOOLBOX:
            inv.Add(I_SCRAP, r.irange(1, 3));
            if (r.chance(0.5f)) inv.Add(I_FUSE, r.irange(1, 2));
            if (r.chance(0.3f)) inv.Add(I_CIRCUIT);
            if (r.chance(0.5f)) inv.Add(I_BOBBYPIN, r.irange(1, 3));
            break;
        case CM_TRASH:
            if (r.chance(0.7f)) Junk();
            if (r.chance(0.4f)) inv.Add(I_TINCAN);
            if (r.chance(0.2f)) inv.Add(I_CAPS, r.irange(1, 8));
            break;
        case CM_CRATE:
            if (r.chance(0.5f)) inv.Add(I_SCRAP, r.irange(1, 2));
            if (r.chance(0.4f)) Junk();
            if (r.chance(0.35f)) { int a = RandAmmo(r, tier); inv.Add(a, AmmoQty(a, r)); }
            if (r.chance(0.3f)) inv.Add(r.chance(0.5f) ? I_BEANS : I_COLA);
            break;
        default: break;
    }
}
