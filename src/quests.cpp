// Missioni, dialoghi, catalogo fotografico e contenuti dei terminali.
#include "game.h"

Quest QUESTS[Q_COUNT] = {
    {"Il cuore del Rifugio"},
    {"Ospiti indesiderati"},
    {"Medicine per Ferrocittà"},
    {"La bestia della gola"},
    {"Segnale nel deserto"},
    {"Il catalogo di Jack"},
    {"Acqua per Ferrocittà"},
    {"Il cane di Rita"},
};

std::vector<TerminalDef> TERMINALS;
std::vector<PhotoSubject> PHOTO_SUBJECTS;

static const int QUEST_XP[Q_COUNT] = {500, 150, 160, 300, 200, 250, 150, 150};
static const int PHOTOS_NEEDED = 8;

// ---------------------------------------------------------------------------
//  Catalogo fotografico
// ---------------------------------------------------------------------------
void InitPhotoSubjects() {
    auto T = [](float x, float z, float up) { return V3(x, TerrainHeight(x, z) + up, z); };
    PHOTO_SUBJECTS = {
        {"roach", "Radscarafaggio", "Grosso come un gatto, veloce e con la vista corta. Va matto per il cibo marcio.", 15, ZONE_WASTE, {}, false},
        {"molerat", "Ratto talpa", "Scava tane nei campi abbandonati e le difende con impegno.", 20, ZONE_WASTE, {}, false},
        {"dog", "Cane selvatico", "Vive in branchi attorno alle rovine. Una striscia di carne lo rende tuo amico per un po'.", 20, ZONE_WASTE, {}, false},
        {"ruggine", "Ruggine", "Il cane di Rita: pelo rossiccio, coda instancabile.", 25, ZONE_WASTE, {}, false},
        {"ghoul", "Ghoul sonnambulo", "Vaga confuso tra le radiazioni. Non è cattivo: solo molto, molto stanco.", 30, ZONE_WASTE, {}, false},
        {"glowghoul", "Ghoul luminoso", "Brilla di verde nel buio del Cratere. Meglio non stargli troppo vicino.", 50, ZONE_WASTE, {}, false},
        {"mutant", "Mutante", "Grande, verde e brontolone. Difende il suo accampamento tra le rovine.", 40, ZONE_WASTE, {}, false},
        {"bigmutant", "Grande Mutante", "Il capo dei mutanti di Nuova Speranza. Ha una voce da baritono.", 70, ZONE_WASTE, {}, false},
        {"robot", "Sentinella di sicurezza", "Robot di guardia della Base Delta-9. Esegue gli ordini anche dopo due secoli.", 35, ZONE_WASTE, {}, false},
        {"camera", "Telecamera di sicurezza", "Occhio elettronico che dà l'allarme alle sentinelle.", 15, ZONE_WASTE, {}, false},
        {"mk5", "Sentinella Mk.V", "Il guardiano del Progetto Prometeo. Enorme, lento e testardo.", 80, ZONE_BASE, {}, false},
        {"deathclaw", "Squarciatore", "La leggenda della gola: tre metri di artigli, e un sonno molto leggero.", 120, ZONE_WASTE, {}, false},
        {"vault_door", "Porta del Rifugio 33", "Casa. Vista da fuori sembra molto più piccola.", 20, ZONE_WASTE, T(-240, 248, 4), true},
        {"water_tower", "Torre dell'acqua", "Il cuore di Ferrocittà, in cima alla collina di lamiera.", 20, ZONE_WASTE, T(2, -2, 12.5f), true},
        {"atom_sign", "Insegna della Stazione Atomo", "L'atomo rosso che di notte si vede da chilometri.", 25, ZONE_WASTE, T(46, 171, 10.5f), true},
        {"silo", "Silo della fattoria", "Arrugginito, ma ancora in piedi.", 20, ZONE_WASTE, T(-142, 162, 11), true},
        {"radio_tower", "Torre Radio", "Trentadue metri di traliccio e una luce rossa che non si spegne mai.", 30, ZONE_WASTE, T(-280, -10, 24), true},
        {"crater", "Cratere Verde", "Una bomba mai esplosa del tutto. L'acqua brilla di verde.", 40, ZONE_WASTE, T(-170, -120, 1), true},
        {"hospital", "Ospedale di Nuova Speranza", "Le lettere dell'insegna resistono ancora.", 30, ZONE_WASTE, T(79, -198, 6), true},
        {"skyscraper", "Scheletro del grattacielo", "Il palazzo più alto delle Terre Bruciate. O ciò che ne resta.", 35, ZONE_WASTE, T(95, -245, 22), true},
        {"base_gate", "Cancello della Base Delta-9", "Cemento, filo spinato e un cartello che dice tutto.", 30, ZONE_WASTE, T(200, 220, 3), true},
        {"reactor", "Reattore del Rifugio", "Il nucleo a fusione del Rifugio 33.", 20, ZONE_VAULT, V3(-22, 3.5f, 8), true},
    };
}

int PhotoIndex(const std::string& key) {
    for (int i = 0; i < (int)PHOTO_SUBJECTS.size(); i++)
        if (key == PHOTO_SUBJECTS[i].key) return i;
    return -1;
}

// ---------------------------------------------------------------------------
//  Missioni
// ---------------------------------------------------------------------------
void SetQuestStage(int q, int stage) {
    Quest& Q = QUESTS[q];
    if (Q.done || stage <= Q.stage) return;
    bool isNew = Q.stage == 0;
    Q.stage = stage;
    if (isNew) Notify(std::string("Nuova missione: ") + Q.title, Col(255, 230, 140));
    else Notify(std::string("Missione aggiornata: ") + Q.title, Col(255, 230, 140));
    PlaySfx(SFX_QUEST);
}

void CompleteQuest(int q) {
    Quest& Q = QUESTS[q];
    if (Q.done) return;
    Q.done = true;
    Q.stage = 100;
    G.player.questsDone++;
    BigNotify("MISSIONE COMPLETATA", Q.title);
    PlaySfx(SFX_QUEST);
    GiveXP(QUEST_XP[q], Q.title);
}

static Vector3 EntPos(const char* tag, const char* dlg = nullptr) {
    for (auto& e : G.ents)
        if ((tag && e.tag == tag) || (dlg && e.dlg == dlg)) return e.pos;
    return V3(0, 0, 0);
}

static int SacksTaken() {
    int n = 0;
    for (auto& it : G.inters)
        if (it.tag == "roach_sack" && it.used) n++;
    return n;
}

static Vector3 NextSack() {
    for (auto& it : G.inters)
        if (it.tag == "roach_sack" && !it.used) return it.pos;
    return V3(60, 0, 180);
}

static int PhotoCount() { return (int)G.player.photos.size(); }

std::vector<QuestObjective> QuestObjectives(int q) {
    std::vector<QuestObjective> out;
    const Quest& Q = QUESTS[q];
    int pz = G.player.zone;
    auto Add = [&](const std::string& t, int zone, Vector3 p) {
        QuestObjective o;
        o.text = t;
        o.zone = zone;
        o.pos = p;
        o.hasPos = zone >= 0;
        out.push_back(o);
    };
    Vector3 vaultDoorOut = W.locations.size() > 0 ? W.locations[0].pos : V3(-240, 0, 250);
    auto ToVault = [&](const std::string& t, Vector3 inside) {
        if (pz == ZONE_VAULT) Add(t, ZONE_VAULT, inside);
        else if (pz == ZONE_BASE) Add(t, ZONE_BASE, V3(0, 0, -5));
        else Add(t, ZONE_WASTE, vaultDoorOut);
    };
    auto Outside = [&](const std::string& t, Vector3 p) {
        if (pz == ZONE_VAULT) Add(t, ZONE_VAULT, V3(0, 0, -30));
        else if (pz == ZONE_BASE) Add(t, ZONE_BASE, V3(0, 0, -5));
        else Add(t, ZONE_WASTE, p);
    };
    switch (q) {
        case Q_MAIN:
            if (Q.stage == 10) ToVault("Parla con il Sovrintendente Ferri nel suo ufficio.", V3(21, 0, 7));
            else if (Q.stage == 20) Outside("Esci dal Rifugio 33 e raggiungi Ferrocittà.", V3(0, 0, 20));
            else if (Q.stage == 30) Outside("Parla con lo Sceriffo Vance a Ferrocittà.", EntPos(nullptr, "sheriff"));
            else if (Q.stage == 40) {
                if (!G.flags["raider_pass"]) Outside("Fatti dare il lasciapassare da Bruto, all'ingresso del Covo dei Teschi.", EntPos("bruto"));
                Outside("Ottieni la tessera di Delta-9 da Teschio (o dalla sua cassaforte).", EntPos("boss"));
                if (G.flags["hint_terminal"]) Outside("(In alternativa) Convinci il terminale al cancello della Base Delta-9.", V3(197, 0, 212));
            } else if (Q.stage == 45) Outside("Apri il cancello della Base Delta-9.", V3(198, 0, 220));
            else if (Q.stage == 50) Outside("Entra nei Laboratori Delta-9 senza farti scortare fuori dalle sentinelle.", V3(272, 0, 220));
            else if (Q.stage == 55) {
                if (pz == ZONE_BASE) Add("Recupera la cella di fusione nel laboratorio. Attento alla Sentinella Mk.V.", ZONE_BASE, V3(0, 0, 74));
                else Outside("Entra nei Laboratori Delta-9.", V3(272, 0, 220));
            } else if (Q.stage == 60) ToVault("Porta la cella di fusione al reattore del Rifugio 33.", V3(-22, 0, 8));
            break;
        case Q_ROACH:
            if (Q.stage == 10) Outside(Fmt("Porta via i sacchi di cibo marcio dalla Stazione Atomo (%d/3).", SacksTaken()), NextSack());
            else if (Q.stage == 20) Outside("Torna da Tano al bar di Ferrocittà.", EntPos(nullptr, "barman"));
            break;
        case Q_MEDS:
            if (Q.stage == 10) Outside("Trova i rifornimenti medici nell'ospedale di Nuova Speranza.", V3(96, 0, -194));
            else if (Q.stage == 20) Outside("Porta i rifornimenti alla dottoressa Rosi.", EntPos(nullptr, "doctor"));
            break;
        case Q_BEAST:
            if (Q.stage == 10) {
                Outside("Fotografa lo Squarciatore che dorme nella gola a nord-ovest. Non svegliarlo!", EntPos("beast"));
                Interactable* pack = FindInterByTag("jack_pack");
                if (pack && pack->inv.Count(I_JOURNAL) > 0) Outside("(Facoltativo) Recupera lo zaino di Jack, accanto alla bestia.", pack->pos);
            } else if (Q.stage == 20) Outside("Mostra la foto a Jack Falco, a Ferrocittà.", EntPos(nullptr, "naturalist"));
            break;
        case Q_SIGNAL:
            if (Q.stage == 10) Outside("Segui il segnale di soccorso fino alla Torre Radio, a ovest.", V3(-280, 0, -10));
            else if (Q.stage == 20) Outside("Ripara il trasmettitore sulla piattaforma della Torre Radio (scala sul lato sud).", V3(-280, 0, -13));
            else if (Q.stage == 30) Outside("Torna da Vera, nella baracca accanto alla torre.", EntPos(nullptr, "scout"));
            else if (Q.stage == 40) Outside("Apri il baule di Vera sul bordo orientale del Cratere Verde.", V3(-128, 0, -150));
            break;
        case Q_PHOTO:
            if (Q.stage == 10) {
                QuestObjective o;
                o.text = Fmt("Fotografa creature e luoghi notevoli per il catalogo di Jack (%d/%d).", std::min(PhotoCount(), PHOTOS_NEEDED), PHOTOS_NEEDED);
                out.push_back(o);
                o.text = "Fotocamera: tasto 2, clic sinistro per scattare, destro per lo zoom. Lo scanner (Q) mostra i soggetti.";
                out.push_back(o);
            } else if (Q.stage == 20) Outside("Porta le foto a Jack Falco, a Ferrocittà.", EntPos(nullptr, "naturalist"));
            break;
        case Q_WATER:
            if (Q.stage == 10) {
                Interactable* p = FindInterByTag("pump");
                Outside("Ripara la pompa sotto la torre dell'acqua (Riparazione 20, 2 componenti, 1 kit di riparazione).", p ? p->pos : V3(6, 0, -1));
            } else if (Q.stage == 20) Outside("Dai la buona notizia allo Sceriffo Vance.", EntPos(nullptr, "sheriff"));
            break;
        case Q_DOG:
            if (Q.stage == 10) Outside("Cerca Ruggine alla Fattoria Abbandonata, a nord-ovest di Ferrocittà.", EntPos("ruggine"));
            else if (Q.stage == 20) Outside("Riporta Ruggine da Rita, a Ferrocittà.", EntPos(nullptr, "rita"));
            else if (Q.stage == 30) Outside("Parla con Rita.", EntPos(nullptr, "rita"));
            break;
    }
    return out;
}

std::string QuestDescription(int q) {
    switch (q) {
        case Q_MAIN: return "Il reattore a fusione del Rifugio 33 si sta spegnendo. Il Sovrintendente mi ha mandato a cercare una cella di fusione sperimentale nella Base Delta-9, sorvegliata da robot che scortano fuori chiunque entri.";
        case Q_ROACH: return "Tano, il barista di Ferrocittà, ha la vecchia stazione di servizio invasa dai radscarafaggi. Sono attirati da tre sacchi di cibo marcio: senza quelli se ne andranno da soli.";
        case Q_MEDS: return "La clinica di Ferrocittà ha finito le medicine. La dottoressa Rosi spera in un vecchio deposito nell'ospedale delle rovine, dove vagano i ghoul.";
        case Q_BEAST: return "Jack Falco, naturalista, sogna da anni una fotografia dello Squarciatore, la bestia leggendaria che dorme nella gola a nord-ovest.";
        case Q_SIGNAL: return "La radio del Pip-Boy capta un segnale di soccorso dalla Torre Radio, a ovest. Qualcuno ha bisogno di aiuto.";
        case Q_PHOTO: return "Jack Falco sta compilando un catalogo delle Terre Bruciate: creature, luoghi, meraviglie. Gli servono fotografie nuove.";
        case Q_WATER: return "La pompa che porta l'acqua a Ferrocittà si è rotta. Con un po' di abilità e qualche ricambio si può rimettere in funzione.";
        case Q_DOG: return "Rita ha perso Ruggine, il suo cane, durante una tempesta di sabbia. Qualcuno l'ha visto verso la vecchia fattoria.";
    }
    return "";
}

void QuestsOnPhoto(const std::string& key) {
    if (key == "deathclaw" && QUESTS[Q_BEAST].stage == 10) SetQuestStage(Q_BEAST, 20);
    if (QUESTS[Q_PHOTO].stage == 10) {
        Notify(Fmt("Catalogo di Jack: %d/%d foto.", std::min(PhotoCount(), PHOTOS_NEEDED), PHOTOS_NEEDED), Col(255, 240, 170));
        if (PhotoCount() >= PHOTOS_NEEDED) SetQuestStage(Q_PHOTO, 20);
    }
}

void QuestsOnRepair(const std::string& tag) {
    if (tag == "pump") {
        G.flags["pump_fixed"] = 1;
        Notify("L'acqua torna a scorrere verso Ferrocittà!", Col(170, 220, 255));
        G.player.karma += 10;
        if (QUESTS[Q_WATER].stage == 10) SetQuestStage(Q_WATER, 20);
    } else if (tag == "transmitter") {
        G.flags["transmitter_fixed"] = 1;
        Notify("Il trasmettitore gracchia e si accende. Vera può chiamare aiuto!", Col(170, 220, 255));
        if (!QUESTS[Q_SIGNAL].done && QUESTS[Q_SIGNAL].stage < 30) {
            if (QUESTS[Q_SIGNAL].stage == 0) SetQuestStage(Q_SIGNAL, 10);
            SetQuestStage(Q_SIGNAL, 30);
        }
    } else if (tag == "generator") {
        G.flags["farm_power"] = 1;
        Notify("Le luci della fattoria si riaccendono dopo decenni.", Col(255, 230, 160));
    }
}

void QuestsOnPickup(int item) {
    if (item == I_KEYCARD) {
        G.flags["keycard_given"] = 1;
        if (QUESTS[Q_MAIN].stage >= 20 && QUESTS[Q_MAIN].stage < 45) SetQuestStage(Q_MAIN, 45);
    }
    if (item == I_FUSIONCELL && QUESTS[Q_MAIN].stage < 60) {
        if (QUESTS[Q_MAIN].stage < 20) QUESTS[Q_MAIN].stage = 20;
        SetQuestStage(Q_MAIN, 60);
    }
    if (item == I_MEDSUPPLIES && QUESTS[Q_MEDS].stage == 10) SetQuestStage(Q_MEDS, 20);
    if (item == I_ROTFOOD && QUESTS[Q_ROACH].stage == 10) Notify(Fmt("Sacchi di cibo marcio: %d/3. Che puzza!", SacksTaken()), Col(220, 230, 160));
    if (item == I_JOURNAL) Notify("Il diario di Jack! Sarà felice di riaverlo.", Col(255, 240, 170));
}

void QuestsOnEnterZone(int zone) {
    if (zone == ZONE_BASE && QUESTS[Q_MAIN].stage >= 20 && QUESTS[Q_MAIN].stage < 55) SetQuestStage(Q_MAIN, 55);
}

void LocationsUpdate() {
    if (G.player.zone != ZONE_WASTE) return;
    for (auto& l : W.locations) {
        if (l.discovered) continue;
        if (DistXZ(G.player.pos, l.pos) < l.radius) {
            l.discovered = true;
            BigNotify("LUOGO SCOPERTO", l.name);
            GiveXP(G.player.perks[PK_EXPLORER] ? 40 : 20);
        }
    }
}

// i radscarafaggi lasciano la stazione quando non c'è più niente da mangiare
static void RoachesLeave() {
    G.flags["roaches_left"] = 1;
    int k = 0;
    for (auto& e : G.ents) {
        if (e.tag != "gas_roach") continue;
        float a = 0.6f + k++ * 0.25f;
        float x = 60 + 70 * cosf(a), z = 180 + 60 * sinf(a);
        e.home = V3(x, TerrainHeight(x, z), z);
        e.ai = AI_RETURN;
        e.aware = false;
        e.alert = 0;
        e.territory = 14;
    }
}

void QuestsUpdate(float dt) {
    static float acc = 0;
    acc += dt;
    if (acc < 0.5f) return;
    acc = 0;
    LocationsUpdate();
    if (QUESTS[Q_MAIN].stage == 20 && W.locations.size() > 1 && W.locations[1].discovered) SetQuestStage(Q_MAIN, 30);
    // con il lasciapassare di Bruto i Teschi ti lasciano in pace
    if (G.flags["raider_pass"])
        for (auto& e : G.ents)
            if (e.fac == FAC_RAIDER && e.hostile) {
                e.hostile = false;
                e.aware = false;
                e.ai = AI_IDLE;
                e.alert = 0;
            }
    if (SacksTaken() >= 3) {
        if (QUESTS[Q_ROACH].stage == 10) SetQuestStage(Q_ROACH, 20);
        if (!G.flags["roaches_left"]) RoachesLeave();
    }
    if (QUESTS[Q_PHOTO].stage == 10 && PhotoCount() >= PHOTOS_NEEDED) SetQuestStage(Q_PHOTO, 20);
    if (QUESTS[Q_DOG].stage == 20) {
        Entity* dog = FindEntityByTag("ruggine");
        Entity* rita = nullptr;
        for (auto& e : G.ents)
            if (e.dlg == "rita") rita = &e;
        if (dog && rita && dog->zone == rita->zone && Vector3Distance(dog->pos, rita->pos) < 10) {
            SetQuestStage(Q_DOG, 30);
            PlaySfx3D(SFX_BARK, dog->pos, 1.0f);
        }
    }
    if (QUESTS[Q_SIGNAL].stage == 40) {
        Interactable* c = FindInterByTag("cache");
        if (c && !c->locked) CompleteQuest(Q_SIGNAL);
    }
    static float radioT = 0;
    if (gRadioStation == 2 && QUESTS[Q_SIGNAL].stage == 0) {
        radioT += 0.5f;
        if (radioT > 6) SetQuestStage(Q_SIGNAL, 10);
    }
}

// ---------------------------------------------------------------------------
//  Dialoghi
// ---------------------------------------------------------------------------
namespace {
void Say(const std::string& t) { G.dlg.text = t; }
void Opt(const std::string& t, std::function<void()> f) { G.dlg.opts.push_back({t, f, true}); }
void Go(int n) { G.dlg.next = n; }
void End() { G.dlg.end = true; }
std::string TagSk(Skill s, int v) { return Fmt("[%s %d] ", SKILL_NAMES[s], v); }
std::string TagSt(int st, int v) { return Fmt("[%s %d] ", SPECIAL_NAMES[st], v); }
bool PassSkill(Skill s, int v) {
    bool ok = SkillValue(s) >= v;
    if (ok) GiveXP(15, "Prova superata");
    return ok;
}
bool PassStat(int st, int v) {
    bool ok = Spec(st) >= v;
    if (ok) GiveXP(15, "Prova superata");
    return ok;
}
int Caps() { return G.player.inv.Count(I_CAPS); }
bool Has(int item) { return G.player.inv.Count(item) > 0; }
const std::string& Name() { return G.player.name; }
Entity& NpcE() { return G.ents[G.dlg.npc]; }
int Flag(const char* f) { return G.flags[f]; }

void DlgOverseer(int n) {
    int st = QUESTS[Q_MAIN].stage;
    if (QUESTS[Q_MAIN].done) {
        Say("Il Rifugio ti deve la vita, " + Name() + ". Le porte resteranno aperte: forse è arrivato il momento di conoscere il mondo là fuori.");
        Opt("È stato un onore.", [] { End(); });
        return;
    }
    if (st >= 60) {
        Say("È... è la cella? Presto, " + Name() + ", portala al reattore! È a ovest dell'atrio.");
        Opt("Vado subito.", [] { End(); });
        return;
    }
    if (st >= 20) {
        switch (n) {
            case 0:
                Say("Ogni giorno le luci tremano un po' di più. Trova quella cella, " + Name() + ".");
                Opt("Cosa sai della Base Delta-9?", [] { Go(10); });
                Opt("Qualche consiglio per là fuori?", [] { Go(11); });
                Opt("Sto andando.", [] { End(); });
                break;
            case 10:
                Say("Era un laboratorio dell'esercito. Gli ultimi messaggi parlavano di robot di sicurezza che non lasciano entrare nessuno. Ferrocittà è a sud-est della nostra uscita: chiedi lì.");
                Opt("Capito.", [] { Go(0); });
                break;
            case 11:
                Say("Muoviti accovacciato quando qualcosa ti sembra pericoloso. Le bestie impazziscono per la carne: lancia un'esca e avrai il tempo di passare. Un barattolo rumoroso attira chiunque altro. E se ti scoprono... corri. Nessuna missione vale una tua ferita.");
                Opt("Lo terrò a mente.", [] { Go(0); });
                break;
        }
        return;
    }
    switch (n) {
        case 0:
            Say("Ah, eccoti, " + Name() + ". Siediti... no, non c'è tempo. Il reattore a fusione del Rifugio si sta spegnendo. I tecnici dicono che tra poche settimane si fermeranno i filtri dell'aria. E poi... beh, lo sai.");
            Opt("Cosa posso fare?", [] { Go(1); });
            Opt("Perché proprio io?", [] { Go(2); });
            Opt(TagSt(ST_INT, 6) + "Non si può riparare il nucleo esistente?", [] { Go(PassStat(ST_INT, 6) ? 3 : 5); });
            break;
        case 1:
            Say("Gli archivi parlano della Base Delta-9, un complesso militare a est. Lì sviluppavano celle a fusione sperimentali: ne basterebbe una per altri cent'anni. Lungo la strada c'è un insediamento, Ferrocittà: le nostre radio captano le loro trasmissioni. Comincia da lì.");
            Opt("Farò del mio meglio.", [] { Go(4); });
            Opt("E se mi rifiutassi?", [] { Go(6); });
            break;
        case 2:
            Say("Hai ottenuto il punteggio più alto al Test di Attitudine Generale. Sei curioso, prudente e sai cavartela senza fare a pugni. È esattamente ciò che serve là fuori.");
            Opt("Capisco. Cosa devo fare?", [] { Go(1); });
            break;
        case 3:
            Say("[Successo] Ci abbiamo provato. Il nucleo è crepato e la schermatura cede: serve una cella nuova. Ma mi piace come ragioni. Prendi anche questi, ti serviranno.");
            Opt("Grazie. Dove trovo una cella nuova?", [] { GiveItem(I_MEDKIT, 2); Go(1); });
            break;
        case 5:
            Say("[Fallito] Apprezzo l'interesse, ma lascia la fisica ai tecnici. Il nucleo è andato.");
            Opt("D'accordo. Cosa devo fare?", [] { Go(1); });
            break;
        case 6:
            Say("Allora fra un mese il Rifugio resterà al buio, te compreso. Ma non credo che tu sia quel tipo di persona.");
            Opt("...Va bene. Andrò.", [] { Go(4); });
            break;
        case 4:
            Say("Prendi questa fotocamera: documenta tutto ciò che trovi, ai tecnici servirà. Poi esche di carne e barattoli rumorosi per distrarre chi non vuole visite, kit medici, un kit di riparazione e qualche tappo. Il Pip-Boy ti mostrerà mappa e obiettivi. La porta principale è in fondo al corridoio nord. E... torna a casa, " + Name() + ".");
            Opt("[Prendi l'equipaggiamento] Tornerò.", [] {
                GiveItem(I_CAMERA, 1);
                GiveItem(I_BAIT, 3);
                GiveItem(I_NOISE, 3);
                GiveItem(I_MEDKIT, 3);
                GiveItem(I_RADAWAY, 1);
                GiveItem(I_BOBBYPIN, 4);
                GiveItem(I_REPAIRKIT, 1);
                GiveItem(I_PARTS, 1);
                GiveItem(I_CAPS, 60);
                EquipTool(TOOL_CAMERA);
                SetQuestStage(Q_MAIN, 20);
                End();
            });
            break;
    }
}

void DlgVaultDoc(int n) {
    switch (n) {
        case 0:
            Say(G.player.hp < HPCap() - 1 || G.player.rads > 1 ? "Qualche graffio? Fatti vedere, " + Name() + "." : "Tutto a posto? Ricordati di bere acqua, là fuori.");
            if (G.player.hp < HPCap() - 1 || G.player.rads > 1)
                Opt("Curami, dottore.", [] {
                    G.player.rads = 0;
                    G.player.hp = HPCap();
                    PlaySfx(SFX_STIM);
                    Go(1);
                });
            Opt("Niente, grazie.", [] { End(); });
            break;
        case 1:
            Say("Ecco fatto. E cerca di non cacciarti nei guai.");
            Opt("Ci proverò.", [] { End(); });
            break;
    }
}

void DlgVaultGuard(int) {
    if (QUESTS[Q_MAIN].stage < 20) Say("Il Sovrintendente ti sta cercando. Il suo ufficio è a est dell'atrio.");
    else if (Flag("reactor_fixed")) Say("Le luci non tremano più. Sei un eroe, " + Name() + ".");
    else Say("Non avrei mai pensato di vedere quella porta aperta. Buona fortuna là fuori.");
    Opt("Grazie.", [] { End(); });
}

void DlgVaultDweller(int) {
    static const char* lines[] = {
        "Davvero uscirai là fuori? Io non ce la farei mai.",
        "Si dice che fuori il cielo sia arancione. È vero?",
        "Le luci del reattore tremano da settimane. Ho paura.",
        "Mio nonno diceva che prima della guerra c'erano gli alberi verdi. Che strano.",
    };
    if (Flag("reactor_fixed")) Say("Il reattore funziona di nuovo! Grazie, " + Name() + "!");
    else Say(lines[(G.dlg.npc * 7) % 4]);
    Opt("Ciao.", [] { End(); });
}

void DlgSheriff(int n) {
    int st = QUESTS[Q_MAIN].stage;
    int wq = QUESTS[Q_WATER].stage;
    switch (n) {
        case 0:
            if (!Flag("met_sheriff")) Say("Un abitante dei Rifugi, eh? Si vede da quella tuta. Benvenuto a Ferrocittà. Qui ci si aiuta a vicenda: rispetta la gente e andremo d'accordo.");
            else Say("Di nuovo tu. Cosa ti serve?");
            G.flags["met_sheriff"] = 1;
            if (st >= 20 && st <= 30) Opt("Cerco la Base Delta-9.", [] { Go(1); });
            if (st == 40 && !Has(I_KEYCARD)) Opt("Come arrivo da Teschio?", [] { Go(7); });
            if (wq == 0) Opt("Posso aiutare Ferrocittà in qualche modo?", [] { Go(9); });
            if (wq == 20) Opt("La pompa dell'acqua funziona di nuovo.", [] { Go(11); });
            Opt("Che posto è Ferrocittà?", [] { Go(5); });
            Opt("Arrivederci.", [] { End(); });
            break;
        case 1:
            Say("Delta-9? Nessuno ci entra da decenni. Il cancello è sigillato e serve una tessera d'accesso militare. L'unica che conosco ce l'ha Teschio, il capo della banda dei Teschi, al vecchio deposito a nord-est. Non sono assassini, ma sono gelosi del loro covo: se ti beccano ti accompagnano fuori a calci e ti alleggeriscono le tasche.");
            Opt("Allora ci parlerò.", [] { Go(2); });
            Opt(TagSk(SK_SPEECH, 40) + "Una sceriffa come te conoscerà un'altra strada.", [] { Go(PassSkill(SK_SPEECH, 40) ? 3 : 4); });
            break;
        case 2:
            Say("Bruto fa la guardia all'ingresso del covo: chi paga il pedaggio, passa. Teschio è un tipo strano: testardo come un mulo, ma ha un debole per la musica d'anteguerra.");
            Opt("Buono a sapersi.", [] { SetQuestStage(Q_MAIN, 40); End(); });
            break;
        case 3:
            Say("[Successo] ...D'accordo. Il mio predecessore teneva un diario: il terminale al cancello di Delta-9 usa una parola d'accesso di sette lettere. Chi se ne intende di computer potrebbe convincerlo ad aprire. Ma non dirlo in giro.");
            Opt("Grazie, sceriffa.", [] { G.flags["hint_terminal"] = 1; SetQuestStage(Q_MAIN, 40); End(); });
            break;
        case 4:
            Say("[Fallito] Un'altra strada? Io ho un distintivo e un sacco di lavoro. Parla con Teschio.");
            Opt("Va bene.", [] { SetQuestStage(Q_MAIN, 40); End(); });
            break;
        case 5:
            Say("Ferrocittà è nata attorno al vecchio deposito ferroviario. Abbiamo una pompa per l'acqua, una dottoressa, l'emporio di Gus e il bar di Tano. E me, che tengo l'ordine.");
            Opt("Capito.", [] { Go(0); });
            break;
        case 7:
            Say("Il Covo dei Teschi è a nord-est: segui la strada che esce dal cancello est. Hanno una vedetta sulla torretta. Se non vuoi pagare Bruto, muoviti accovacciato e tieni a portata di mano qualche barattolo rumoroso.");
            Opt("Grazie.", [] { Go(0); });
            break;
        case 9:
            if (Flag("pump_fixed")) {
                Say("Aiutare? Qualcuno ha già rimesso in sesto la nostra pompa dell'acqua... aspetta, sei stato tu? Allora Ferrocittà ti deve un grazie.");
                Opt("È stato un piacere.", [] {
                    SetQuestStage(Q_WATER, 10);
                    SetQuestStage(Q_WATER, 20);
                    Go(11);
                });
                break;
            }
            Say("La pompa sotto la torre dell'acqua è guasta da una settimana: beviamo dalle scorte. Serve qualcuno che sappia usare una chiave inglese, un kit di riparazione e un paio di componenti meccanici. Gus ne vende, oppure fruga nelle officine abbandonate.");
            Opt("Ci penso io.", [] { SetQuestStage(Q_WATER, 10); End(); });
            Opt("Non me ne intendo.", [] { Go(0); });
            break;
        case 11:
            Say("Senti che rumore? È acqua che scorre! Ferrocittà non lo dimenticherà. Prendi questi tappi e qualche bottiglia d'acqua purificata.");
            Opt("Felice di aiutare.", [] {
                GiveItem(I_CAPS, 120);
                GiveItem(I_PUREWATER, 3);
                G.player.karma += 15;
                CompleteQuest(Q_WATER);
                End();
            });
            break;
    }
}

void DlgTrader(int n) {
    switch (n) {
        case 0:
            Say("Benvenuto all'Emporio di Gus! Esche, barattoli, ricambi, vestiti e tutto ciò che il deserto non si è ancora mangiato.");
            Opt("Fammi vedere la merce.", [] { End(); BarterOpen(0, G.dlg.npc); });
            Opt("Novità in giro?", [] { Go(1); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1: {
            static const char* r[] = {
                "Nella gola a nord-ovest dorme una bestia enorme. Jack Falco sogna di fotografarla: lo trovi vicino al fuoco in piazza.",
                "Il Cratere Verde a ovest brilla di notte. Se ci vai, porta Rad-Via. Tanto Rad-Via.",
                "Sulle frequenze radio c'è un segnale di soccorso. Viene da ovest, dalla torre. Accendi la radio del tuo aggeggio da polso.",
                "Un mantello ottico ti rende quasi invisibile per mezzo minuto. Roba rara: ne ho uno solo.",
                "Rita è disperata: il suo cane è sparito. Se lo vedi, dalle una mano.",
            };
            Say(r[(int)(G.time * 3) % 5]);
            Opt("Interessante.", [] { Go(0); });
            break;
        }
    }
}

void DlgDoctor(int n) {
    int mq = QUESTS[Q_MEDS].stage;
    auto Start = [] {
        SetQuestStage(Q_MEDS, 10);
        if (G.player.inv.Count(I_MEDSUPPLIES) > 0) SetQuestStage(Q_MEDS, 20);
    };
    switch (n) {
        case 0:
            Say("Graffi? Radiazioni? Siediti e non toccare niente.");
            if (G.player.hp < HPCap() - 1) Opt("Curami. (40 tappi)", [] {
                    if (Caps() >= 40) { TakeItem(I_CAPS, 40); G.player.hp = HPCap(); PlaySfx(SFX_STIM); Go(2); }
                    else Go(3);
                });
            if (G.player.rads > 1) Opt("Toglimi le radiazioni. (60 tappi)", [] {
                    if (Caps() >= 60) { TakeItem(I_CAPS, 60); G.player.rads = 0; PlaySfx(SFX_STIM); Go(2); }
                    else Go(3);
                });
            Opt("Vorrei comprare delle medicine.", [] { End(); BarterOpen(1, G.dlg.npc); });
            if (mq == 0) Opt("Hai bisogno di aiuto?", [] { Go(4); });
            if (!QUESTS[Q_MEDS].done && mq > 0 && Has(I_MEDSUPPLIES)) Opt("Ho trovato i rifornimenti medici.", [] { Go(6); });
            Opt("Arrivederci.", [] { End(); });
            break;
        case 2:
            Say("Ecco fatto. Cerca di non tornare qui coperto di lividi.");
            Opt("Grazie.", [] { Go(0); });
            break;
        case 3:
            Say("Niente tappi, niente cure. Le medicine non crescono sugli alberi. Gli alberi, poi, non crescono proprio.");
            Opt("Capisco.", [] { Go(0); });
            break;
        case 4:
            Say("Le scorte sono finite. L'ospedale delle rovine di Nuova Speranza, a sud, aveva un deposito medico. Se me lo porti ti ricompenserò bene. Attento però: lì vagano i ghoul. Non sono cattivi, ma se ti afferrano non ti mollano finché non scappi.");
            Opt("Ci penso io.", [Start] { Start(); End(); });
            Opt(TagSk(SK_BARTER, 35) + "Quanto 'bene', esattamente?", [] { Go(PassSkill(SK_BARTER, 35) ? 5 : 7); });
            break;
        case 5:
            Say("[Successo] Sei un osso duro. Va bene: aggiungo cento tappi alla ricompensa.");
            Opt("Affare fatto.", [Start] { G.flags["meds_bonus"] = 1; Start(); End(); });
            break;
        case 7:
            Say("[Fallito] Abbastanza, fidati. Qui la gente ha la febbre, non ho tempo per contrattare.");
            Opt("Va bene, ci vado.", [Start] { Start(); End(); });
            break;
        case 6:
            Say("Questi... questi salveranno decine di vite. Grazie, davvero. Prendi questi kit medici e un po' di tappi.");
            Opt("Felice di aiutare.", [] {
                TakeItem(I_MEDSUPPLIES, 1);
                GiveItem(I_MEDKIT, 4);
                GiveItem(I_CAPS, Flag("meds_bonus") ? 200 : 100);
                G.player.karma += 25;
                CompleteQuest(Q_MEDS);
                End();
            });
            break;
    }
}

void DlgBarman(int n) {
    int rq = QUESTS[Q_ROACH].stage;
    switch (n) {
        case 0:
            Say("Cosa ti servo? Acqua torbida o acqua molto torbida?");
            Opt("Fammi vedere cosa vendi.", [] { End(); BarterOpen(2, G.dlg.npc); });
            if (rq == 0) Opt("Hai qualche lavoro?", [] { Go(1); });
            if (rq == 20) Opt("I radscarafaggi hanno lasciato la Stazione Atomo.", [] { Go(3); });
            Opt("Raccontami di questo posto.", [] { Go(4); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1:
            Say("Avevo una stazione di servizio a est, la Stazione Atomo. Qualche idiota ci ha lasciato tre sacchi di cibo andato a male e ora è un banchetto per radscarafaggi grossi come gatti. Porta via quei sacchi e se ne andranno da soli. Ma se ti vedono ti rincorrono fino alla strada!");
            Opt("Affare fatto.", [] {
                SetQuestStage(Q_ROACH, 10);
                if (SacksTaken() >= 3) SetQuestStage(Q_ROACH, 20);
                End();
            });
            Opt("Non mi interessa.", [] { Go(0); });
            break;
        case 3:
            Say("Se ne sono andati senza che ne schiacciassi nemmeno uno? Sei un genio! Dammi quei sacchi, li butto nel compost. Ecco i tuoi tappi... e questo, dal retro: un mantello ottico. Me l'ha lasciato un viaggiatore per pagare un vecchio conto.");
            Opt("Grazie, Tano.", [] {
                TakeItem(I_ROTFOOD, G.player.inv.Count(I_ROTFOOD));
                GiveItem(I_CAPS, 100);
                GiveItem(I_STEALTHBOY, 1);
                G.player.karma += 10;
                CompleteQuest(Q_ROACH);
                End();
            });
            break;
        case 4:
            Say("A ovest c'è il Cratere Verde: una bomba caduta lì non è mai esplosa del tutto e i ghoul ci sguazzano. A sud ci sono le rovine di Nuova Speranza, dove vivono i mutanti. E a est c'è la Base Delta-9, dove i robot cacciano via chiunque.");
            Opt("Allegro.", [] { Go(0); });
            break;
    }
}

void DlgNaturalist(int n) {
    int pq = QUESTS[Q_PHOTO].stage, bq = QUESTS[Q_BEAST].stage;
    switch (n) {
        case 0:
            Say("Ah, un'altra anima curiosa! Sono Jack Falco. Una volta davo la caccia alle creature delle Terre Bruciate; oggi le studio e le disegno. È molto più divertente, e nessuno si fa male.");
            if (pq == 0) Opt("Posso aiutarti con le tue ricerche?", [] { Go(1); });
            if (pq == 20) Opt("Ho le foto per il tuo catalogo.", [] { Go(4); });
            if (pq > 0 && bq == 0) Opt("Hai parlato di una bestia leggendaria...", [] { Go(5); });
            if (bq == 20) Opt("Ho fotografato lo Squarciatore!", [] { Go(7); });
            if (Has(I_JOURNAL)) Opt("Ho trovato il tuo diario.", [] { Go(9); });
            if (bq == 10) Opt("Dove dorme lo Squarciatore?", [] { Go(8); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1:
            Say("Le mie ginocchia non reggono più le escursioni. Mi serve qualcuno con una fotocamera: creature, luoghi notevoli, meraviglie. Portami almeno otto fotografie diverse e il catalogo sarà completo.");
            Opt("Lo farò.", [] {
                if (!Has(I_CAMERA)) GiveItem(I_CAMERA);
                SetQuestStage(Q_PHOTO, 10);
                if (PhotoCount() >= PHOTOS_NEEDED) SetQuestStage(Q_PHOTO, 20);
                Go(2);
            });
            break;
        case 2:
            Say("Un consiglio: con il clic destro la fotocamera ingrandisce, così puoi scattare da lontano. Le creature non amano i lampi in faccia.");
            Opt("Grazie, Jack.", [] { Go(0); });
            break;
        case 4:
            Say(Fmt("%d fotografie! Guarda che dettagli... questa la incornicio. Ecco la tua paga: 150 tappi e il mio vecchio mantello scuro. Ti aiuterà a passare inosservato.", PhotoCount()));
            Opt("Grazie!", [] {
                GiveItem(I_CAPS, 150);
                GiveItem(I_DARKCLOAK, 1);
                CompleteQuest(Q_PHOTO);
                Go(0);
            });
            break;
        case 5:
            Say("Lo Squarciatore. Tre metri di artigli, e un sonno molto leggero. Dorme nella gola a nord-ovest, oltre il Cratere Verde. Nessuno l'ha mai fotografato. L'ultima volta che ci ho provato sono scappato così in fretta che ho perso lo zaino, con dentro il mio diario.");
            Opt("Lo fotograferò io.", [] {
                SetQuestStage(Q_BEAST, 10);
                if (HasPhoto("deathclaw")) SetQuestStage(Q_BEAST, 20);
                Go(6);
            });
            Opt(TagSk(SK_NATURE, 40) + "Come ci si avvicina a una creatura così?", [] { Go(PassSkill(SK_NATURE, 40) ? 10 : 6); });
            Opt("È troppo per me.", [] { End(); });
            break;
        case 6:
            Say("Muoviti accovacciato, non correre e non saltare. Se si sveglia, un'esca di carne lo terrà occupato per un po'. E se ritrovi il mio zaino... ti sarei grato per sempre.");
            Opt("Ci vado.", [] {
                SetQuestStage(Q_BEAST, 10);
                if (HasPhoto("deathclaw")) SetQuestStage(Q_BEAST, 20);
                End();
            });
            break;
        case 10:
            Say("[Successo] Vedo che te ne intendi! Allora sai che il vento conta: avvicinati dal fondo della gola, accovacciato. Tieni, prendi queste esche: sono le mie migliori.");
            Opt("Grazie.", [] {
                GiveItem(I_BAIT, 2);
                SetQuestStage(Q_BEAST, 10);
                if (HasPhoto("deathclaw")) SetQuestStage(Q_BEAST, 20);
                End();
            });
            break;
        case 7:
            Say("Per tutti i... guarda che zampe! E quella cresta! È la fotografia più bella che abbia mai visto. Questa vale ogni tappo che ho: 250. E tienili, non discutere.");
            Opt("Grazie, Jack.", [] {
                GiveItem(I_CAPS, 250);
                G.player.karma += 10;
                CompleteQuest(Q_BEAST);
                Go(0);
            });
            break;
        case 8:
            Say("Nella gola a nord-ovest, oltre il Cratere Verde. Segui il canalone fino in fondo. Lì c'è il nido.");
            Opt("Capito.", [] { Go(0); });
            break;
        case 9:
            Say("Il mio diario! Trent'anni di appunti e schizzi... pensavo fosse perso per sempre. Non so come ringraziarti. Prendi questi tappi, e qualche striscia di carne: agli animali piace più che a me.");
            Opt("È stato un piacere.", [] {
                TakeItem(I_JOURNAL, 1);
                GiveItem(I_CAPS, 100);
                GiveItem(I_MEAT, 3);
                G.player.karma += 10;
                GiveXP(75, "Diario di Jack");
                Go(0);
            });
            break;
    }
}

void DlgRita(int n) {
    int dq = QUESTS[Q_DOG].stage;
    switch (n) {
        case 0:
            if (QUESTS[Q_DOG].done) {
                Say("Come sta Ruggine? Trattamelo bene, eh! E dagli una carezza da parte mia.");
                Opt("Promesso.", [] { End(); });
                break;
            }
            if (dq == 30) {
                Say("RUGGINE! Sei tornato! ...Guardalo, scodinzola come un matto. Grazie, grazie davvero.");
                Opt("Era nel fienile della fattoria, spaventato.", [] { Go(3); });
                break;
            }
            if (dq == 0) {
                Say("Scusa... hai visto un cane? Pelo rossiccio, un orecchio piegato. Si chiama Ruggine. È scappato durante la tempesta di sabbia, tre giorni fa.");
                Opt("Lo cercherò io.", [] { Go(1); });
                Opt("Mi dispiace, non l'ho visto.", [] { End(); });
            } else {
                Say("Hai notizie di Ruggine? Qualcuno l'ha visto verso la vecchia fattoria, a nord-ovest.");
                Opt("Lo sto cercando.", [] { End(); });
            }
            break;
        case 1:
            Say("Davvero? Pino dice di averlo visto verso la Fattoria Abbandonata, a nord-ovest. Ruggine va pazzo per la carne secca: se ha paura, offrigliene un pezzo.");
            Opt("Lo riporterò a casa.", [] {
                SetQuestStage(Q_DOG, 10);
                if (G.flags["dog_friend"]) SetQuestStage(Q_DOG, 20);
                End();
            });
            break;
        case 3:
            Say("Sai cosa? Guarda come ti sta appiccicato. Credo che abbia scelto te. Portalo con te per un po': là fuori ti farà compagnia e annuserà i guai prima di te. Prendi questi, è tutto ciò che ho.");
            Opt("Grazie, Rita. Ne avrò cura.", [] {
                GiveItem(I_CAPS, 80);
                GiveItem(I_MEAT, 3);
                G.player.karma += 20;
                CompleteQuest(Q_DOG);
                End();
            });
            break;
    }
}

void DlgRuggine(int n) {
    switch (n) {
        case 0:
            if (!G.flags["dog_friend"]) {
                Say("Un cane dal pelo rossiccio ti fissa dal fondo del fienile, tremando. Ha un collare consumato con una targhetta: RUGGINE. Ringhia piano, ma la coda si muove appena.");
                if (Has(I_MEAT)) Opt("[Offri una striscia di carne secca]", [] { TakeItem(I_MEAT, 1); Go(1); });
                Opt(TagSk(SK_NATURE, 30) + "Ti accovacci e lo chiami piano per nome.", [] { Go(PassSkill(SK_NATURE, 30) ? 1 : 2); });
                Opt("Lascialo in pace.", [] { End(); });
                break;
            }
            Say(NpcE().companion ? "Ruggine scodinzola e ti guarda, in attesa di ordini." : "Ruggine solleva le orecchie e ti fissa: vuole venire con te.");
            if (NpcE().companion) Opt("Aspettami qui, bello.", [] {
                    Entity& d = NpcE();
                    d.companion = false;
                    d.home = d.pos;
                    d.zone = G.player.zone;
                    End();
                });
            else Opt("Vieni, Ruggine!", [] { NpcE().companion = true; PlaySfx(SFX_BARK); End(); });
            Opt("Bravo cane.", [] { End(); });
            break;
        case 1:
            Say("Ruggine si avvicina, annusa la tua mano e... ti lecca le dita! Ora scodinzola come una girandola. Sembra pronto a seguirti ovunque.");
            Opt("Andiamo a casa, Ruggine.", [] {
                G.flags["dog_friend"] = 1;
                NpcE().companion = true;
                PlaySfx(SFX_BARK);
                GiveXP(40, "Nuovo amico");
                if (QUESTS[Q_DOG].stage == 0) SetQuestStage(Q_DOG, 10);
                SetQuestStage(Q_DOG, 20);
                End();
            });
            break;
        case 2:
            Say("[Fallito] Ruggine indietreggia e abbaia. È troppo spaventato. Forse con qualcosa da mangiare...");
            Opt("Ci riproverò.", [] { End(); });
            break;
    }
}

void DlgScout(int n) {
    int sq = QUESTS[Q_SIGNAL].stage;
    switch (n) {
        case 0:
            if (QUESTS[Q_SIGNAL].done) {
                Say("La mia squadra arriva domani. Grazie ancora, " + Name() + ": senza di te sarei ancora qui a parlare con i cani.");
                Opt("Buona fortuna, Vera.", [] { End(); });
                break;
            }
            if (sq <= 10) {
                Say("Finalmente! Pensavo che quel segnale non l'avrebbe sentito nessuno. Sono Vera, esploratrice di Ferrocittà. Mi sono storta la caviglia scappando dai cani selvatici, e il trasmettitore lassù si è guastato a metà messaggio.");
                Opt("Come posso aiutarti?", [] { Go(1); });
            } else if (sq == 20) {
                Say("Il trasmettitore è sulla piattaforma a metà torre. La scala è sul lato sud. Ti servono un kit di riparazione e un componente meccanico.");
                Opt("Ci sto lavorando.", [] { End(); });
            } else if (sq == 30) {
                Say("Ho sentito il ronzio fin da qui: funziona! Ho già chiamato la mia squadra. Ti devo un favore enorme.");
                Opt("Sono contento di averti aiutata.", [] { Go(5); });
            } else {
                Say("Il mio baule è sul bordo orientale del Cratere Verde, in un capanno di cemento. Porta del Rad-Via.");
                Opt("Ci vado.", [] { End(); });
            }
            if (!G.flags["vera_healed"]) {
                Opt(TagSk(SK_MEDICINE, 30) + "Fammi dare un'occhiata a quella caviglia.", [] { Go(PassSkill(SK_MEDICINE, 30) ? 2 : 3); });
                if (Has(I_MEDKIT)) Opt("[Usa un kit medico] Tieni, questo ti aiuterà.", [] { TakeItem(I_MEDKIT, 1); Go(4); });
            }
            if (G.flags["hint_record"] && !Has(I_RECORD) && FindInterByTag("record") && !FindInterByTag("record")->used)
                Opt("Hai visto per caso un disco d'anteguerra?", [] { Go(6); });
            break;
        case 1:
            Say("Il trasmettitore è sulla piattaforma a metà torre: c'è una scala sul lato sud. Se riesci a ripararlo, chiamo la mia squadra. Ti servirà un kit di riparazione e un componente meccanico. Attento ai cani: un'esca li tiene buoni.");
            Opt("Ci penso io.", [] {
                SetQuestStage(Q_SIGNAL, 10);
                if (G.flags["transmitter_fixed"]) SetQuestStage(Q_SIGNAL, 30);
                else SetQuestStage(Q_SIGNAL, 20);
                End();
            });
            break;
        case 2:
            Say("[Successo] Una fasciatura stretta e un bastone... ehi, riesco ad appoggiarla! Hai le mani di un medico.");
            Opt("Riposati.", [] { G.flags["vera_healed"] = 1; G.player.karma += 10; Go(0); });
            break;
        case 3:
            Say("[Fallito] Ahi! No, no, lascia stare. Aspetterò la mia squadra.");
            Opt("Scusa.", [] { Go(0); });
            break;
        case 4:
            Say("Oh, va molto meglio. Grazie. Prendi queste esche: io non ne avrò più bisogno.");
            Opt("Riposati.", [] { G.flags["vera_healed"] = 1; G.player.karma += 10; GiveItem(I_BAIT, 2); GiveXP(30); Go(0); });
            break;
        case 5:
            Say("Prendi questa chiave: apre il mio baule sul bordo orientale del Cratere Verde. Dentro c'è una tuta antiradiazioni e un po' di scorte. Te le sei meritate.");
            Opt("Grazie, Vera.", [] {
                GiveItem(I_CACHEKEY, 1);
                GiveItem(I_SCOUTNOTE, 1);
                SetQuestStage(Q_SIGNAL, 40);
                End();
            });
            break;
        case 6:
            Say("Quel vecchio 78 giri? È sulla scrivania qui dietro. Prendilo pure: io ascolto solo il vento.");
            Opt("Grazie.", [] { Go(0); });
            break;
    }
}

void GiveKeycard() {
    Interactable* safe = FindInterByTag("boss_safe");
    if (safe) safe->inv.Remove(I_KEYCARD, safe->inv.Count(I_KEYCARD));
    GiveItem(I_KEYCARD, 1);
}

void GivePass() {
    G.flags["raider_pass"] = 1;
    Notify("Lasciapassare dei Teschi: la banda ti lascerà in pace.", Col(255, 230, 140));
}

void DlgRaiderGuard(int n) {
    switch (n) {
        case 0:
            if (G.flags["raider_pass"]) {
                Say("Sei a posto, rifugiato. Teschio sta nella baracca grande. E non toccare niente.");
                Opt("Grazie, Bruto.", [] { End(); });
                break;
            }
            Say("Alt! Questo è il territorio dei Teschi. Chi entra paga il pedaggio: 50 tappi. Chi non paga, esce. Di solito volando.");
            if (Caps() >= 50) Opt("[Paga 50 tappi]", [] { TakeItem(I_CAPS, 50); Go(1); });
            Opt(TagSk(SK_SPEECH, 35) + "Sono qui per affari con Teschio. Affari grossi.", [] { Go(PassSkill(SK_SPEECH, 35) ? 2 : 3); });
            Opt(TagSk(SK_BARTER, 40) + "50 tappi per guardare dei rottami? Facciamo 20.", [] { Go(PassSkill(SK_BARTER, 40) ? 4 : 3); });
            Opt("Ci penserò.", [] { End(); });
            break;
        case 1:
            Say("Bravo. Ecco il tuo lasciapassare: un tappo con un teschio disegnato sopra. Mostralo e nessuno ti darà fastidio.");
            Opt("Grazie.", [] { GivePass(); End(); });
            break;
        case 2:
            Say("[Successo] Affari grossi, eh? Teschio odia aspettare... va bene, passa. Ma se mi hai preso in giro, ti appendo per le bretelle.");
            Opt("Non te ne pentirai.", [] { GivePass(); End(); });
            break;
        case 3:
            Say("[Fallito] Ah ah, bella questa. 50 tappi, o fuori dai piedi.");
            if (Caps() >= 50) Opt("[Paga 50 tappi]", [] { TakeItem(I_CAPS, 50); Go(1); });
            Opt("Me ne vado.", [] { End(); });
            break;
        case 4:
            Say("[Successo] ...Tsk. Va bene, 20. Ma non dirlo a nessuno, ho una reputazione.");
            if (Caps() >= 20) Opt("[Paga 20 tappi]", [] { TakeItem(I_CAPS, 20); Go(1); });
            else Opt("Non li ho...", [] { Go(3); });
            break;
    }
}

void DlgBoss(int n) {
    switch (n) {
        case 0:
            if (Flag("keycard_given") || Has(I_KEYCARD)) {
                Say("Ancora tu? La tessera ce l'hai già. Vai a farti scortare fuori dai robot, e salutameli.");
                Opt("Ciao, Teschio.", [] { End(); });
                break;
            }
            Say("Così tu sei il topo di Rifugio che ha pagato Bruto. Parla, e in fretta: ho una radio da aggiustare.");
            if (QUESTS[Q_MAIN].stage >= 20) Opt("Mi serve la tessera della Base Delta-9.", [] { Go(1); });
            Opt("Perché vi chiamate Teschi?", [] { Go(6); });
            Opt("Niente, me ne vado.", [] { End(); });
            break;
        case 1:
            Say("La tessera militare? È nella mia cassaforte, e lì resta. È l'unica chiave per la base: un giorno la venderò a peso d'oro.");
            Opt(TagSk(SK_SPEECH, 60) + "Dentro la base c'è una cella che tiene in vita un intero Rifugio. A te non serve a niente.", [] { Go(PassSkill(SK_SPEECH, 60) ? 2 : 3); });
            Opt("Quanto vuoi?", [] { Go(4); });
            if (Has(I_RECORD)) Opt("Ho un disco d'anteguerra. Funziona.", [] { Go(5); });
            Opt("Ci sarà qualcosa che ti serve.", [] { Go(7); });
            Opt("Lascia perdere.", [] { End(); });
            break;
        case 2:
            Say("[Successo] ...Mia madre era nata in un Rifugio. Non l'ho mai detto a nessuno. Tieni, prendila. E sparisci prima che cambi idea.");
            Opt("Grazie, Teschio.", [] { G.flags["boss_deal"] = 1; GiveKeycard(); G.player.karma += 10; End(); });
            break;
        case 3:
            Say("[Fallito] Bel discorso. Mi hai quasi commosso. Quasi.");
            Opt("Parliamone ancora.", [] { Go(1); });
            break;
        case 4:
            Say("300 tappi. Non trattabili.");
            if (Caps() >= 300) Opt("[Paga 300 tappi]", [] { TakeItem(I_CAPS, 300); G.flags["boss_deal"] = 2; GiveKeycard(); End(); });
            Opt(TagSk(SK_BARTER, 50) + "300 per un pezzo di plastica? 150, ed è un regalo.", [] { Go(PassSkill(SK_BARTER, 50) ? 8 : 9); });
            Opt("Troppo.", [] { Go(1); });
            break;
        case 5:
            Say("\"Swing sotto la luna atomica\"! Mia nonna me la cantava! La mia radio suona solo fruscio da dieci anni... Affare fatto: il disco per la tessera. E prendi anche questi tappi, per il disturbo.");
            Opt("Buon ascolto.", [] {
                TakeItem(I_RECORD, 1);
                G.flags["boss_deal"] = 3;
                GiveKeycard();
                GiveItem(I_CAPS, 50);
                G.player.karma += 5;
                GiveXP(50, "Un dono gradito");
                End();
            });
            break;
        case 6:
            Say("Perché siamo duri come le ossa! ...In realtà il nome l'ha scelto mio fratello. Aveva sei anni. Non ridere.");
            Opt("Non rido.", [] { Go(0); });
            break;
        case 7:
            Say("La mia radio suona solo fruscio da anni. Portami un disco d'anteguerra, uno vero, e ne riparliamo. Dicono che alla vecchia Torre Radio, a ovest, ce ne fosse ancora qualcuno.");
            Opt("Vedrò cosa posso fare.", [] { G.flags["hint_record"] = 1; End(); });
            break;
        case 8:
            Say("[Successo] Hai la lingua lunga, rifugiato. 150. E non tornare a lamentarti.");
            if (Caps() >= 150) Opt("[Paga 150 tappi]", [] { TakeItem(I_CAPS, 150); G.flags["boss_deal"] = 2; GiveKeycard(); End(); });
            Opt("Ci devo pensare.", [] { End(); });
            break;
        case 9:
            Say("[Fallito] 300. Ho detto non trattabili. Hai la sabbia nelle orecchie?");
            Opt("Va bene.", [] { Go(4); });
            break;
    }
}

void DlgRaider(int) {
    static const char* lines[] = {
        "Bruto dice che sei a posto. Non toccare la mia roba.",
        "Non siamo cattivi, eh. Solo un po'... territoriali.",
        "Teschio è nella baracca grande. Oggi è di buon umore, per una volta.",
        "Hai visto la vedetta? Dorme in piedi. Non dirglielo.",
    };
    Say(lines[(G.dlg.npc * 3 + (int)(G.time / 40)) % 4]);
    Opt("Ciao.", [] { End(); });
}

void DlgSettler(int) {
    static const char* lines[] = {
        "Bella giornata, se ti piace la polvere.",
        "Hai visto la Stazione Atomo? Una volta ci facevo il pieno col mio vecchio.",
        "Non bere l'acqua del cratere. Fidati.",
        "Lo sceriffo è dura, ma senza di lei i Teschi ci avrebbero già svuotato la dispensa.",
        "Un abitante dei Rifugi! Avete davvero l'acqua calda, laggiù?",
        "Di notte si sentono i cani selvatici ululare verso la torre radio.",
    };
    if (Flag("pump_fixed") && (G.dlg.npc % 2 == 0)) Say("Hai riparato la pompa! Stasera da Tano offro io!");
    else Say(lines[(G.dlg.npc * 5 + (int)(G.time / 30)) % 6]);
    Opt("Ciao.", [] { End(); });
}

void DlgGuard(int) {
    static const char* lines[] = {"Niente guai, rifugiato.", "Tengo d'occhio la strada. Tu tieni d'occhio le tue tasche.", "Se vedi i Teschi, girale al largo. Non fare l'eroe."};
    Say(lines[G.dlg.npc % 3]);
    Opt("D'accordo.", [] { End(); });
}
} // namespace

void DialogueStart(int npc) {
    G.dlg = Dialogue();
    G.dlg.npc = npc;
    G.dlg.node = 0;
    G.dlg.speaker = G.ents[npc].name;
    SetGameState(GS_DIALOGUE);
    DialogueBuild();
}

void DialogueBuild() {
    Dialogue& d = G.dlg;
    d.text.clear();
    d.opts.clear();
    d.end = false;
    d.next = d.node;
    d.sel = 0;
    d.typeT = 0;
    const std::string& id = G.ents[d.npc].dlg;
    int n = d.node;
    if (id == "overseer") DlgOverseer(n);
    else if (id == "vault_doc") DlgVaultDoc(n);
    else if (id == "vault_guard") DlgVaultGuard(n);
    else if (id == "vault_dweller") DlgVaultDweller(n);
    else if (id == "sheriff") DlgSheriff(n);
    else if (id == "trader") DlgTrader(n);
    else if (id == "doctor") DlgDoctor(n);
    else if (id == "barman") DlgBarman(n);
    else if (id == "naturalist") DlgNaturalist(n);
    else if (id == "rita") DlgRita(n);
    else if (id == "ruggine") DlgRuggine(n);
    else if (id == "scout") DlgScout(n);
    else if (id == "raider_guard") DlgRaiderGuard(n);
    else if (id == "boss") DlgBoss(n);
    else if (id == "raider") DlgRaider(n);
    else if (id == "guard") DlgGuard(n);
    else DlgSettler(n);
}

// ---------------------------------------------------------------------------
//  Terminali
// ---------------------------------------------------------------------------
void InitTerminals() {
    TERMINALS.assign(TERM_COUNT, TerminalDef());
    TERMINALS[TERM_OVERSEER] = {"ATOMTEC TERMLINK - RIFUGIO 33 - SOVRINTENDENZA", {
        {"Registro: stato del reattore", "Giorno 31.207.\nIl nucleo a fusione mostra microfratture nella schermatura.\nRendimento al 41%. Stima di spegnimento: 30-40 giorni.\nNessuna cella di ricambio disponibile nel Rifugio.", ""},
        {"Archivio: Base Delta-9", "Secondo gli archivi militari, la Base Delta-9 ospitava il\nProgetto Prometeo: celle a fusione portatili ad alta durata.\nUltima comunicazione ricevuta: ottobre 2077.", ""},
        {"Nota personale", "Ho dovuto scegliere un volontario.\nNessuno di noi sa cosa ci sia davvero là fuori.\nSpero di aver scelto bene. Spero che torni presto.", ""},
    }};
    TERMINALS[TERM_GATE] = {"SISTEMA DI ACCESSO PERIMETRALE - BASE DELTA-9", {
        {"Stato del cancello", "CANCELLO PRINCIPALE: CHIUSO\nPROTOCOLLO DI QUARANTENA: ATTIVO\nPersonale autorizzato: usare la tessera d'accesso.", ""},
        {"Apri il cancello principale", "Comando inviato. Sblocco dei servomotori in corso...", "open_gate"},
    }};
    TERMINALS[TERM_SECURITY] = {"SICUREZZA DELTA-9 - SORVEGLIANZA AUTOMATICA", {
        {"Stato della sorveglianza", "Sentinelle di pattuglia: ATTIVE\nTelecamere: ATTIVE\nProcedura per gli intrusi: SCORTA ALL'USCITA\n(Il protocollo letale è stato rimosso nel 2071.)", ""},
        {"Disattiva sentinelle e telecamere di quest'area", "Spegnimento dei dispositivi di sorveglianza in corso...", "security_off"},
        {"Sblocca il magazzino interno", "Serratura magnetica del magazzino disattivata.", "unlock_storage"},
    }};
    TERMINALS[TERM_LAB] = {"PROGETTO PROMETEO - LABORATORIO PRINCIPALE", {
        {"Rapporto 117", "La cella prototipo P-7 ha superato i test di stabilità.\nAutonomia stimata: 140 anni.\nIl prototipo resta sul piedistallo del laboratorio in attesa\ndel trasporto.", ""},
        {"Rapporto 118", "Le sentinelle Mk.V hanno ricevuto il protocollo di quarantena.\nNessuno entra, nessuno esce. Gli intrusi vengono\naccompagnati gentilmente al cancello.", ""},
        {"Ultimo messaggio", "Se qualcuno legge... siamo usciti dall'uscita di emergenza\nprima che le sentinelle chiudessero tutto.\nDite a mia figlia che sto bene e che torno a casa.", ""},
        {"Arresto di manutenzione: Sentinella Mk.V", "Codice accettato. La Sentinella Mk.V entra in modalità\ndi manutenzione.", "mk5_off"},
    }};
    TERMINALS[TERM_RAIDER] = {"TERMINALE PRIVATO DI TESCHIO - NON TOCCARE", {
        {"Bottino", "Carovana di Ferrocittà: 3 casse di roba (le abbiamo \"trovate\").\nTessera militare: NELLA CASSAFORTE. Nessuno la tocca.\nLa chiave di riserva l'ha nascosta Ossobuco in una delle tende.", ""},
        {"Lista dei desideri", "1. Un disco d'anteguerra che funzioni.\n2. Una radio che non gracchi.\n3. Che Bruto smetta di russare.", ""},
        {"Sblocca la cassaforte", "Serratura elettronica aperta.", "unlock_safe"},
    }};
    TERMINALS[TERM_CLINIC] = {"CLINICA DI FERROCITTÀ", {{"Scorte", "Kit medici: 0\nRad-Via: 0\nServono rifornimenti.", ""}}};
}
