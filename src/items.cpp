// Oggetti, attrezzi, inventario, bottino, abilità e talenti.
#include "game.h"

const char* SPECIAL_NAMES[ST_COUNT] = {"Forza", "Percezione", "Resistenza", "Carisma", "Intelligenza", "Agilità", "Fortuna"};
const char* SPECIAL_DESC[ST_COUNT] = {
    "Potenza fisica. Aumenta il peso trasportabile e la distanza dei lanci.",
    "Sensi acuti. Ti aiuta a notare i pericoli e a capire gli animali.",
    "Robustezza. Aumenta l'energia fisica e la resistenza alle radiazioni.",
    "Fascino e presenza. Migliora i prezzi e le opzioni di dialogo.",
    "Acume mentale. Più punti abilità a ogni livello, scienza e riparazioni.",
    "Coordinazione. Aumenta i Punti Azione, l'atletica e la furtività.",
    "Il favore del destino. Migliora un po' tutte le abilità."};

const char* SKILL_NAMES[SK_COUNT] = {"Atletica", "Riparazione", "Sopravvivenza", "Natura", "Medicina", "Scasso", "Scienza", "Baratto", "Eloquenza", "Furtività"};
const char* SKILL_DESC[SK_COUNT] = {
    "Corse più lunghe, salti più alti e lanci più lontani.",
    "Permette di aggiustare pompe, generatori e vecchi macchinari.",
    "Cibo e acqua ti curano di più e resisti meglio alle radiazioni.",
    "Gli animali ti notano più tardi e le esche li tengono occupati più a lungo.",
    "Efficacia di kit medici e Rad-Via.",
    "Capacità di aprire serrature con le forcine.",
    "Capacità di violare i terminali protetti.",
    "Prezzi migliori con i mercanti.",
    "Persuasione nei dialoghi.",
    "Capacità di muoversi senza essere notati."};
const int SKILL_GOV[SK_COUNT] = {ST_AGI, ST_INT, ST_END, ST_PER, ST_INT, ST_PER, ST_INT, ST_CHA, ST_CHA, ST_AGI};

PerkDef PERKS[PK_COUNT] = {
    {"Passo Felpato", "+15 Furtività. Correre fa meno rumore.", 2, ST_AGI, 4},
    {"Amico degli Animali", "Gli animali ti notano molto più tardi e restano più a lungo sulle esche.", 2, ST_CHA, 4},
    {"Esploratore", "Tutti i luoghi compaiono sulla mappa e ti muovi il 10% più veloce.", 2, ST_PER, 5},
    {"Fotoreporter", "+50% punti esperienza dalle fotografie.", 2, ST_PER, 4},
    {"Medico da Campo", "Kit medici e Rad-Via sono il 50% più efficaci.", 2, ST_INT, 4},
    {"Stomaco di Piombo", "-50% radiazioni da cibo e acqua.", 2, ST_END, 3},
    {"Resistenza alle Radiazioni", "+25% resistenza alle radiazioni.", 4, ST_END, 5},
    {"Cervello Fino", "+3 punti abilità a ogni livello.", 2, ST_INT, 4},
    {"Schiena Forte", "+50 al peso trasportabile.", 4, ST_STR, 5},
    {"Energia Inesauribile", "+25 Punti Azione massimi.", 4, ST_AGI, 5},
    {"Ombra", "Il mantello ottico dura il doppio e al buio sei più difficile da vedere.", 6, ST_AGI, 6},
    {"Cercatore di Fortuna", "Trovi il doppio dei tappi nei contenitori.", 2, ST_LCK, 5},
    {"Braccio d'Oro", "Lanci più lontano e i barattoli fanno più rumore.", 2, ST_STR, 4},
    {"Meccanico", "+15 Riparazione e un componente in meno per ogni riparazione.", 4, ST_INT, 5},
    {"Riserva di Energia", "+30 energia fisica massima.", 6, ST_END, 5},
    {"Recupero", "Recuperi lentamente l'energia fisica.", 8, ST_END, 6},
    {"Infiltrato", "Il punto giusto delle serrature è più ampio.", 4, ST_PER, 5},
    {"Hacker", "+1 tentativo nei terminali.", 4, ST_INT, 5},
    {"Lingua d'Argento", "+15 Eloquenza.", 4, ST_CHA, 5},
    {"Mercante", "+15 Baratto.", 2, ST_CHA, 4},
    {"Corridore", "Correre consuma metà dei Punti Azione.", 4, ST_AGI, 5},
    {"Sonno Ristoratore", "Due ore di riposo ti curano del tutto e riducono le radiazioni.", 2, ST_END, 4},
};

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

    Def(I_CAMERA, "Fotocamera", IT_TOOL, 1.5f, 80, "Una vecchia fotocamera d'anteguerra. Clic sinistro scatta, clic destro ingrandisce. Fotografa creature e luoghi per il tuo catalogo.");
    Def(I_BAIT, "Esca", IT_TOOL, 0.3f, 6, "Un fagotto di carne dall'odore fortissimo. Lanciala: gli animali correranno a mangiarla e si dimenticheranno di te.");
    Def(I_NOISE, "Barattolo rumoroso", IT_TOOL, 0.3f, 4, "Una lattina piena di sassolini. Lanciala per attirare guardie e curiosi da un'altra parte.");
    ITEMS[I_CAMERA].tool = TOOL_CAMERA;
    ITEMS[I_BAIT].tool = TOOL_BAIT;
    ITEMS[I_NOISE].tool = TOOL_NOISE;
    Def(I_SCANNER, "Scanner da campo", IT_TOOL, 0.8f, 140, "Strumento di rilevamento ambientale. Equipaggia con 5: clic esamina indizi, terreno e macchinari. Non consuma cariche e non agisce su persone o animali.");
    Def(I_MULTITOOL, "Multistrumento", IT_TOOL, 1.0f, 160, "Pinze, chiavi e sonde per rimettere in funzione gli impianti dell'Atlante. Equipaggia con 6 e usa clic vicino a un macchinario; E permette la stessa riparazione contestuale.");
    Def(I_RECORDER, "Registratore di segnali", IT_TOOL, 0.7f, 120, "Ascolta le tracce della rete dell'Alba e conserva gli indizi sonori. Equipaggia con 7 e usa clic presso un punto di ascolto. Uno strumento silenzioso e riutilizzabile.");
    ITEMS[I_SCANNER].tool = TOOL_SCANNER;
    ITEMS[I_MULTITOOL].tool = TOOL_MULTITOOL;
    ITEMS[I_RECORDER].tool = TOOL_RECORDER;

    Def(I_VAULTSUIT, "Tuta del Rifugio 33", IT_ARMOR, 1, 10, "Blu e gialla, con il numero 33 sulla schiena.");
    Def(I_JACKET, "Giacca da viaggio", IT_ARMOR, 4, 120, "Robusta e piena di tasche: porti più oggetti.");
    Def(I_DARKCLOAK, "Mantella scura", IT_ARMOR, 2, 150, "Una mantella color sabbia e cenere. Ti confonde con il paesaggio.");
    Def(I_RADSUIT, "Tuta anti-radiazioni", IT_ARMOR, 5, 150, "Riduce fortemente le radiazioni assorbite. Un po' rumorosa.");
    Def(I_EXOSUIT, "Esoscheletro da lavoro", IT_ARMOR, 30, 1500, "Un supporto per i lavori di manutenzione: trasporti carichi enormi, ma fai un gran baccano.");
    Def(I_COAT, "Spolverino elegante", IT_ARMOR, 3, 110, "Un lungo cappotto da viaggio. Fa sempre una buona impressione.");
    ITEMS[I_VAULTSUIT].look = OF_VAULT;
    ITEMS[I_JACKET].carry = 25;  ITEMS[I_JACKET].look = OF_LEATHER;
    ITEMS[I_DARKCLOAK].sneak = 15; ITEMS[I_DARKCLOAK].look = OF_DARK;
    ITEMS[I_RADSUIT].radRes = 0.45f; ITEMS[I_RADSUIT].sneak = -5; ITEMS[I_RADSUIT].look = OF_RADSUIT;
    ITEMS[I_EXOSUIT].carry = 120; ITEMS[I_EXOSUIT].radRes = 0.25f; ITEMS[I_EXOSUIT].sneak = -20; ITEMS[I_EXOSUIT].look = OF_POWER;
    ITEMS[I_COAT].talk = 10; ITEMS[I_COAT].look = OF_SHERIFF;

    Def(I_MEDKIT, "Kit di ristoro", IT_AID, 0, 50, "Acqua, integratori e piccoli rimedi per le lunghe escursioni. Ripristina l'energia fisica.");
    Def(I_RADAWAY, "Rad-Via", IT_AID, 0, 50, "Flebo chelante. Elimina le radiazioni.");
    Def(I_RADX, "Anti-Rad", IT_AID, 0, 40, "Pillole che aumentano la resistenza alle radiazioni.");
    Def(I_STEALTHBOY, "Mantello ottico", IT_AID, 0.5f, 200, "Un generatore di campo d'anteguerra: per qualche secondo diventi quasi invisibile.");
    Def(I_COLA, "Cola Atomica", IT_AID, 1, 20, "La bibita preferita d'anteguerra. Un po' radioattiva.");
    Def(I_WATER, "Acqua sporca", IT_AID, 1, 5, "Meglio di niente. Forse.");
    Def(I_PUREWATER, "Acqua purificata", IT_AID, 1, 20, "Acqua pulita. Un lusso.");
    Def(I_MEAT, "Carne secca", IT_AID, 1, 8, "Strisce di carne essiccata. Piacciono molto anche ai cani.");
    Def(I_BEANS, "Fagioli in scatola", IT_AID, 1, 8, "Duecento anni e ancora buoni.");
    Def(I_CRAM, "Carne in scatola", IT_AID, 1, 10, "Carne... di qualche tipo.");
    Def(I_COFFEE, "Caffè d'anteguerra", IT_AID, 0.5f, 30, "Ricarica i Punti Azione e li fa recuperare più in fretta per un minuto.");
    Def(I_HERBTEA, "Tisana del deserto", IT_AID, 0.5f, 35, "Erbe aromatiche: +2 Intelligenza e Percezione per due minuti.");
    ITEMS[I_MEDKIT].heal = 45;
    ITEMS[I_RADAWAY].rads = -150;
    ITEMS[I_RADX].radX = 0.5f;
    ITEMS[I_STEALTHBOY].stealth = 25;
    ITEMS[I_COLA].heal = 12;    ITEMS[I_COLA].rads = 5;    ITEMS[I_COLA].ap = 20;
    ITEMS[I_WATER].heal = 6;    ITEMS[I_WATER].rads = 8;
    ITEMS[I_PUREWATER].heal = 18;
    ITEMS[I_MEAT].heal = 16;    ITEMS[I_MEAT].rads = 2;
    ITEMS[I_BEANS].heal = 12;   ITEMS[I_BEANS].rads = 2;
    ITEMS[I_CRAM].heal = 15;    ITEMS[I_CRAM].rads = 3;
    ITEMS[I_COFFEE].ap = 40;

    Def(I_BOBBYPIN, "Forcina", IT_MISC, 0, 1, "Indispensabile per aprire le serrature.");
    Def(I_REPAIRKIT, "Kit di riparazione", IT_MISC, 1, 40, "Attrezzi, viti e nastro adesivo. Serve per ogni riparazione.");
    Def(I_PARTS, "Componenti meccanici", IT_MISC, 1, 15, "Ingranaggi, molle e guarnizioni. Utili per riparare i macchinari.");
    Def(I_SCRAP, "Rottami metallici", IT_MISC, 1, 6, "Pezzi di metallo. I mercanti li comprano.");
    Def(I_TINCAN, "Lattina vuota", IT_MISC, 0.5f, 1, "Vuota.");
    Def(I_TOASTER, "Tostapane rotto", IT_MISC, 3, 5, "Non tosta più niente da due secoli.");
    Def(I_TEDDY, "Orsacchiotto", IT_MISC, 1, 4, "Un vecchio orsacchiotto spelacchiato.");
    Def(I_CLOCK, "Sveglia rotta", IT_MISC, 1, 6, "Ferma sulle 9:47.");
    Def(I_BOTTLE, "Bottiglia vuota", IT_MISC, 0.5f, 1, "Un vuoto a rendere.");
    Def(I_FUSE, "Fusibile", IT_MISC, 0.2f, 10, "Componente elettrico.");
    Def(I_CIRCUIT, "Circuito elettronico", IT_MISC, 0.5f, 25, "Componenti d'anteguerra molto ricercati.");
    Def(I_COMIC, "Fumetto d'anteguerra", IT_MISC, 0.5f, 30, "\"Le avventure di Capitan Atomo\". Un pezzo da collezione.");

    Def(I_MEDSUPPLIES, "Rifornimenti medici", IT_KEY, 5, 0, "Una cassa di medicinali per la dottoressa Rosi.");
    Def(I_KEYCARD, "Tessera d'accesso Delta-9", IT_KEY, 0, 0, "Apre il cancello della Base Delta-9.");
    Def(I_FUSIONCELL, "Cella di fusione prototipo", IT_KEY, 8, 0, "Il cuore pulsante del Rifugio 33.");
    Def(I_JOURNAL, "Diario di Jack", IT_KEY, 1, 0, "Il diario del naturalista Jack Falco, pieno di schizzi di animali.");
    Def(I_CACHEKEY, "Chiave del deposito", IT_KEY, 0, 0, "Apre il baule nel deposito vicino al Cratere Verde.");
    Def(I_SCOUTNOTE, "Biglietto dell'esploratore", IT_KEY, 0, 0,
        "\"Sono salito sulla torre per aggiustare l'antenna, ma ho dovuto lasciare tutto in fretta. "
        "La mia roba è nel deposito sul bordo orientale del Cratere Verde: la chiave è qui. Usala bene, chiunque tu sia.\"");
    Def(I_SAFEKEY, "Chiave della cassaforte", IT_KEY, 0, 0, "Apre la cassaforte del custode Neri.");
    Def(I_RECORD, "Disco d'anteguerra", IT_KEY, 1, 0, "Un 78 giri: \"Swing sotto la luna atomica\". Qualcuno ne sarebbe felicissimo.");
    Def(I_ROTFOOD, "Sacco di cibo marcio", IT_KEY, 4, 0, "Puzza in modo indescrivibile. È ciò che attira i radscarafaggi.");
    Def(I_MAINTCODE, "Codice di manutenzione", IT_KEY, 0, 0, "Un foglietto: \"Sentinella Mk.V - arresto di manutenzione: PROMETEO-0451\".");
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
void GenContainerLoot(Inventory& inv, int model, Rng& r, int tier) {
    tier = std::max(tier, 1);
    int junk[] = {I_TINCAN, I_TOASTER, I_TEDDY, I_CLOCK, I_BOTTLE, I_SCRAP};
    auto Junk = [&] { inv.Add(junk[r.irange(0, 5)], 1); };
    switch (model) {
        case CM_AMMOBOX: // cassetta da manutenzione
            if (r.chance(0.6f)) inv.Add(I_NOISE, r.irange(1, 3));
            if (r.chance(0.5f)) inv.Add(I_PARTS, r.irange(1, 2));
            if (r.chance(0.35f)) inv.Add(I_REPAIRKIT);
            if (r.chance(0.08f * tier)) inv.Add(I_STEALTHBOY);
            break;
        case CM_FIRSTAID:
            if (r.chance(0.8f)) inv.Add(I_MEDKIT, r.irange(1, tier));
            if (r.chance(0.5f)) inv.Add(I_RADAWAY, 1);
            if (r.chance(0.3f)) inv.Add(I_RADX, 1);
            break;
        case CM_FRIDGE: {
            int food[] = {I_COLA, I_BEANS, I_WATER, I_MEAT, I_CRAM, I_PUREWATER};
            int n = r.irange(1, 3);
            for (int i = 0; i < n; i++) inv.Add(food[r.irange(0, 5)], 1);
            if (r.chance(0.3f)) inv.Add(I_BAIT);
            break;
        }
        case CM_DESK:
            if (r.chance(0.7f)) inv.Add(I_CAPS, r.irange(5, 25) * tier);
            if (r.chance(0.5f)) inv.Add(I_BOBBYPIN, r.irange(1, 3));
            if (r.chance(0.4f)) Junk();
            if (r.chance(0.2f)) inv.Add(I_COMIC);
            if (r.chance(0.15f)) inv.Add(I_HERBTEA);
            break;
        case CM_LOCKER:
            if (r.chance(0.2f + 0.08f * tier)) {
                int wear[] = {I_JACKET, I_DARKCLOAK, I_COAT, I_RADSUIT};
                inv.Add(wear[r.irange(0, std::min(3, tier))]);
            }
            if (r.chance(0.5f)) inv.Add(r.chance(0.5f) ? I_BAIT : I_NOISE, r.irange(1, 2));
            if (r.chance(0.4f)) inv.Add(I_MEDKIT);
            if (r.chance(0.2f)) inv.Add(I_COFFEE);
            break;
        case CM_SAFE:
            inv.Add(I_CAPS, r.irange(40, 100) * tier);
            if (r.chance(0.4f)) inv.Add(I_CIRCUIT);
            if (r.chance(0.3f)) inv.Add(I_STEALTHBOY);
            if (r.chance(0.3f)) inv.Add(I_COMIC);
            break;
        case CM_FOOTLOCKER:
            if (r.chance(0.6f)) inv.Add(I_CAPS, r.irange(5, 30) * tier);
            if (r.chance(0.4f)) Junk();
            if (r.chance(0.35f)) inv.Add(r.chance(0.5f) ? I_MEDKIT : I_RADAWAY);
            if (r.chance(0.3f)) inv.Add(I_BOBBYPIN, r.irange(1, 4));
            if (r.chance(0.3f)) inv.Add(r.chance(0.5f) ? I_BAIT : I_NOISE);
            break;
        case CM_TOOLBOX:
            inv.Add(I_SCRAP, r.irange(1, 2));
            if (r.chance(0.6f)) inv.Add(I_PARTS, 1);
            if (r.chance(0.4f)) inv.Add(I_REPAIRKIT);
            if (r.chance(0.5f)) inv.Add(I_FUSE, r.irange(1, 2));
            if (r.chance(0.4f)) inv.Add(I_BOBBYPIN, r.irange(1, 3));
            break;
        case CM_TRASH:
            if (r.chance(0.7f)) Junk();
            if (r.chance(0.4f)) inv.Add(I_TINCAN);
            if (r.chance(0.2f)) inv.Add(I_CAPS, r.irange(1, 8));
            break;
        case CM_CRATE:
            if (r.chance(0.5f)) inv.Add(I_SCRAP, r.irange(1, 2));
            if (r.chance(0.4f)) Junk();
            if (r.chance(0.3f)) inv.Add(I_PARTS);
            if (r.chance(0.3f)) inv.Add(r.chance(0.5f) ? I_BEANS : I_COLA);
            if (r.chance(0.25f)) inv.Add(I_BAIT);
            break;
        default: break;
    }
}
