// Oggetti, armi, inventario, bottino, abilità e talenti.
#include "game.h"

const char* SPECIAL_NAMES[ST_COUNT] = {"Forza", "Percezione", "Resistenza", "Carisma", "Intelligenza", "Agilità", "Fortuna"};
const char* SPECIAL_DESC[ST_COUNT] = {
    "Potenza fisica. Aumenta il peso trasportabile e la potenza degli emettitori ravvicinati.",
    "Sensi acuti. Migliora la precisione nello S.P.A.T. e le armi a energia.",
    "Robustezza. Aumenta i punti ferita e la resistenza alle radiazioni.",
    "Fascino e presenza. Migliora i prezzi e le opzioni di dialogo.",
    "Acume mentale. Più punti abilità a ogni livello, scienza e medicina.",
    "Coordinazione. Aumenta i Punti Azione e la precisione con i laser leggeri.",
    "Il favore del destino. Aumenta i colpi critici e tutte le abilità."};

const char* SKILL_NAMES[SK_COUNT] = {"Laser leggeri", "Laser avanzati", "Laser ravvicinati", "Impulsi EMP", "Medicina", "Scasso", "Scienza", "Baratto", "Eloquenza", "Furtività"};
const char* SKILL_DESC[SK_COUNT] = {
    "Precisione e potenza stordente con pistole, carabine e laser automatici.",
    "Precisione e potenza stordente con emettitori laser avanzati.",
    "Potenza stordente delle lame laser e del guanto a impulsi.",
    "Potenza stordente e gittata degli impulsi EMP.",
    "Efficacia di stimpak e Rad-Via.",
    "Capacità di forzare serrature con le forcine.",
    "Capacità di violare i terminali protetti.",
    "Prezzi migliori con i mercanti.",
    "Persuasione nei dialoghi.",
    "Capacità di muoversi senza essere notati."};
const int SKILL_GOV[SK_COUNT] = {ST_AGI, ST_PER, ST_STR, ST_PER, ST_INT, ST_PER, ST_INT, ST_CHA, ST_CHA, ST_AGI};

PerkDef PERKS[PK_COUNT] = {
    {"Pellaccia", "+10 alla resistenza agli impulsi stordenti.", 2, ST_END, 4},
    {"Pistolero", "+25% potenza stordente e precisione con pistole laser a una mano.", 2, ST_AGI, 4},
    {"Commando", "+25% potenza stordente e precisione con laser a due mani.", 4, ST_AGI, 5},
    {"Duellante laser", "+50% potenza stordente nei colpi ravvicinati.", 2, ST_STR, 5},
    {"Medico da Campo", "Stimpak e Rad-Via sono il 50% più efficaci.", 2, ST_INT, 4},
    {"Stomaco di Piombo", "-50% radiazioni da cibo e acqua.", 2, ST_END, 3},
    {"Resistenza alle Radiazioni", "+25% resistenza alle radiazioni.", 4, ST_END, 5},
    {"Cervello Fino", "+3 punti abilità a ogni livello.", 2, ST_INT, 4},
    {"Schiena Forte", "+50 al peso trasportabile.", 4, ST_STR, 5},
    {"Energia Inesauribile", "+25 Punti Azione massimi.", 4, ST_AGI, 5},
    {"Cecchino", "+25% probabilità di colpire la testa nello S.P.A.T.", 6, ST_PER, 6},
    {"Mano Fortunata", "+5% probabilità di colpo critico.", 4, ST_LCK, 5},
    {"Sovraccarico", "+5% potenza stordente. Gli impulsi hanno una scarica luminosa piu' intensa.", 2, ST_LCK, 1},
    {"Cercatore di Fortuna", "Trovi il doppio dei tappi nei contenitori.", 2, ST_LCK, 5},
    {"Comandante Laser", "+25% potenza stordente con i laser avanzati.", 6, ST_PER, 6},
    {"Ricarica Rapida", "Ricarichi il 40% più velocemente.", 2, ST_AGI, 5},
    {"Datore di Vita", "+30 punti ferita massimi.", 6, ST_END, 5},
    {"Passo Felpato", "+20 Furtività. Correre non ti rende più visibile.", 4, ST_AGI, 5},
    {"Rigenerazione", "Rigeneri lentamente i PF fuori dal combattimento.", 8, ST_END, 6},
    {"Infiltrato", "Il punto giusto delle serrature è più ampio.", 4, ST_PER, 5},
    {"Hacker", "+1 tentativo nei terminali.", 4, ST_INT, 5},
    {"Impulso espanso", "+50% potenza stordente degli impulsi EMP.", 4, ST_PER, 4},
};

WeaponDef WEAPONS[] = {
    // Stessi ID, prestazioni e costi del gioco originale; energia stordente per tutte le armi.
    // model,         skill,         ammo,         mag, dmg,  delay, spread, pel, range, reload, ap, auto,  melee, energy, 2h,   sfx,         beam,                zoom, recoil
    {WM_KNIFE,        SK_MELEE,      -1,           0,   14,   0.45f, 0,      1,   2.3f,  0,      12, false, true,  true,   false, SFX_LASER,   {80, 245, 255, 255},  1.0f, 0},
    {WM_PIPE,         SK_MELEE,      -1,           0,   17,   0.70f, 0,      1,   2.4f,  0,      15, false, true,  true,   false, SFX_LASER,   {110, 255, 150, 255}, 1.0f, 0},
    {WM_BAT,          SK_MELEE,      -1,           0,   23,   0.80f, 0,      1,   2.5f,  0,      18, false, true,  true,   true,  SFX_LASER,   {180, 125, 255, 255}, 1.0f, 0},
    {WM_SLEDGE,       SK_MELEE,      -1,           0,   38,   1.20f, 0,      1,   2.7f,  0,      25, false, true,  true,   true,  SFX_PLASMA,  {90, 255, 190, 255},  1.0f, 0},
    {WM_LASERPISTOL,  SK_SMALLGUNS,  I_AMMO_10,    12,  17,   0.22f, 2.0f,   1,   120,   1.6f,   17, false, false, true,   false, SFX_LASER,   {80, 245, 255, 255},  1.3f, 1.6f},
    {WM_LASERRIFLE,   SK_SMALLGUNS,  I_AMMO_308,   5,   52,   1.00f, 0.4f,   1,   260,   2.6f,   25, false, false, true,   true,  SFX_LASER,   {100, 195, 255, 255}, 3.0f, 4.0f},
    {WM_LASERRIFLE,   SK_SMALLGUNS,  I_AMMO_SHELL, 2,   9,    0.35f, 6.5f,   9,   40,    2.4f,   25, false, false, true,   true,  SFX_LASER,   {180, 140, 255, 255}, 1.2f, 6.0f},
    {WM_LASERRIFLE,   SK_SMALLGUNS,  I_AMMO_556,   24,  12,   0.11f, 2.8f,   1,   150,   2.2f,   20, true,  false, true,   true,  SFX_LASER,   {90, 255, 215, 255},  1.6f, 1.0f},
    {WM_LASERPISTOL,  SK_ENERGY,     I_AMMO_CELL,  30,  19,   0.30f, 1.2f,   1,   140,   1.8f,   15, false, false, true,   false, SFX_LASER,   {255, 65, 90, 255},   1.3f, 0.6f},
    {WM_LASERRIFLE,   SK_ENERGY,     I_AMMO_CELL,  24,  30,   0.32f, 0.7f,   1,   200,   2.2f,   20, false, false, true,   true,  SFX_LASER,   {255, 65, 90, 255},   2.0f, 0.8f},
    {WM_PLASMA,       SK_ENERGY,     I_AMMO_MFC,   12,  58,   0.55f, 1.0f,   1,   160,   2.4f,   25, false, false, true,   true,  SFX_PLASMA,  {90, 255, 150, 255},  1.6f, 2.5f},
    {WM_LASERRIFLE,   SK_SMALLGUNS,  I_AMMO_5MM,   120, 8,    0.06f, 4.5f,   1,   120,   4.5f,   30, true,  false, true,   true,  SFX_LASER,   {100, 220, 255, 255}, 1.0f, 0.7f},
    {WM_GRENADE,      SK_EXPLOSIVES, I_GRENADE,    1,   110,  1.00f, 0,      1,   6,     0,      20, false, false, true,   false, SFX_THROW,   {80, 245, 255, 255},  1.0f, 0},
    {WM_FIST,         SK_MELEE,      -1,           0,   6,    0.45f, 0,      1,   2.0f,  0,      10, false, true,  true,   false, SFX_LASER,   {80, 245, 255, 255},        1.0f, 0},
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
    Def(I_KNIFE, "Lama laser corta", IT_WEAPON, 1, 30, "Un emettitore compatto per il combattimento ravvicinato. La lama a energia stordisce senza uccidere.");
    Def(I_PIPE, "Bastone laser", IT_WEAPON, 3, 15, "Un vecchio emettitore a energia: portata corta e impulso stordente.");
    Def(I_BAT, "Lama laser lunga", IT_WEAPON, 3, 45, "Una lama energetica a due mani, potente negli scontri ravvicinati.");
    Def(I_SLEDGE, "Emettitore laser pesante", IT_WEAPON, 15, 180, "Lento e pesante: scarica un impulso stordente molto potente a distanza ravvicinata.");
    Def(I_PISTOL, "Pistola laser compatta", IT_WEAPON, 3, 120, "Affidabile emettitore semiautomatico d'anteguerra. Usa microcelle A e mette fuori combattimento senza uccidere.");
    Def(I_HUNTRIFLE, "Carabina laser di precisione", IT_WEAPON, 6, 220, "Lenta, precisa e dotata di mirino: concentra la carica stordente a lunga distanza.");
    Def(I_SHOTGUN, "Dispersore laser", IT_WEAPON, 4, 180, "Due cariche di impulsi laser a ventaglio, efficaci a breve distanza.");
    Def(I_ASSAULT, "Laser automatico", IT_WEAPON, 7, 350, "Emettitore militare a raffiche di impulsi stordenti. Usa celle automatiche.");
    Def(I_LASERPISTOL, "Pistola laser avanzata", IT_WEAPON, 2, 300, "Emettitore a energia preciso e a basso rinculo. Gli impulsi neutralizzano il bersaglio definitivamente senza ucciderlo.");
    Def(I_LASERRIFLE, "Fucile laser stordente", IT_WEAPON, 7, 600, "Laser militare a lunga gittata. Una volta esaurita la resistenza, il bersaglio resta fuori combattimento per tutta la partita.");
    Def(I_PLASMA, "Proiettore laser a impulsi", IT_WEAPON, 8, 1100, "Concentra una grande quantita' di energia stordente in ogni impulso. Usa celle ad alta capacita'.");
    Def(I_MINIGUN, "Gatling laser", IT_WEAPON, 18, 900, "Emettitore pesante a raffica continua. Pesante, rapido e non letale.");
    Def(I_GRENADE, "Impulso EMP", IT_WEAPON, 0.5f, 25, "Premi G per lanciare un condensatore che libera una scarica stordente ad area, senza frammenti.");
    for (int i = I_KNIFE; i <= I_GRENADE; i++) ITEMS[i].weapon = i - I_KNIFE;

    Def(I_AMMO_10, "Microcelle A", IT_AMMO, 0, 1, "Ricariche energetiche per la pistola laser compatta.");
    Def(I_AMMO_308, "Celle di precisione", IT_AMMO, 0, 3, "Ricariche energetiche per la carabina laser di precisione.");
    Def(I_AMMO_SHELL, "Condensatori a dispersione", IT_AMMO, 0, 2, "Cariche energetiche per il dispersore laser.");
    Def(I_AMMO_556, "Celle automatiche", IT_AMMO, 0, 2, "Ricariche energetiche per il laser automatico.");
    Def(I_AMMO_CELL, "Cella laser", IT_AMMO, 0, 3, "Ricarica per la pistola laser avanzata e il fucile laser stordente.");
    Def(I_AMMO_MFC, "Cella ad alta capacita'", IT_AMMO, 0, 5, "Ricarica ad alta capacita' per il proiettore laser a impulsi.");
    Def(I_AMMO_5MM, "Microcelle rapide", IT_AMMO, 0, 1, "Ricariche energetiche per la gatling laser.");

    Def(I_VAULTSUIT, "Tuta del Rifugio 33", IT_ARMOR, 1, 10, "Blu e gialla, con il numero 33 sulla schiena.");
    Def(I_LEATHER, "Armatura di cuoio", IT_ARMOR, 12, 160, "Cuoio indurito di bramino. Leggera e robusta.");
    Def(I_RAIDER, "Armatura da predone", IT_ARMOR, 15, 120, "Pezzi di copertone e metallo legati insieme.");
    Def(I_METAL, "Armatura di metallo", IT_ARMOR, 25, 300, "Lamiere martellate. Pesante, ma attenua gli impulsi stordenti.");
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
    Def(I_MEAT, "Razione di carne secca", IT_AID, 1, 8, "Una razione conservata trovata fra le provviste. Leggermente irradiata.");
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
    Def(I_CLAW, "Artiglio dello Squarciatore", IT_MISC, 3, 60, "Un artiglio spezzato recuperato accanto alla bestia neutralizzata. La prova richiesta da Jack: lo Squarciatore e' ancora vivo.");
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
