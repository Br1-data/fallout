// Missioni, dialoghi, catalogo fotografico e contenuti dei terminali.
#include "game.h"

Quest QUESTS[Q_COUNT] = {
    {"Il cuore del Rifugio"},
    {"Infestazione"},
    {"Medicine per Ferrocittà"},
    {"La bestia della gola"},
    {"Segnale nel deserto"},
    {"Il catalogo di Jack"},
    {"Acqua per Ferrocittà"},
    {"Il cane di Rita"},
};

std::vector<TerminalDef> TERMINALS;
std::vector<PhotoSubject> PHOTO_SUBJECTS;

static const int QUEST_XP[Q_COUNT] = {500, 120, 160, 300, 150, 0, 0, 0};

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
    if (q < 0 || q >= CAMPAIGN_QUEST_COUNT) return;
    Quest& Q = QUESTS[q];
    if (Q.done || stage <= Q.stage) return;
    bool isNew = Q.stage == 0;
    Q.stage = stage;
    if (isNew) Notify(std::string("Nuova missione: ") + Q.title, Col(255, 230, 140));
    else Notify(std::string("Missione aggiornata: ") + Q.title, Col(255, 230, 140));
    PlaySfx(SFX_QUEST);
}

void CompleteQuest(int q) {
    if (q < 0 || q >= CAMPAIGN_QUEST_COUNT) return;
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

static std::string RoachFlag(const Entity& e) { return "original_roach_" + std::to_string(e.id); }

static int RoachesRemaining() {
    int n = 0;
    for (const auto& e : G.ents) {
        if (e.tag != "gas_roach") continue;
        auto flag = G.flags.find(RoachFlag(e));
        if (flag == G.flags.end() || flag->second == 0) n++;
    }
    return n;
}

std::vector<QuestObjective> QuestObjectives(int q) {
    std::vector<QuestObjective> out;
    if (q < 0 || q >= CAMPAIGN_QUEST_COUNT) return out;
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
                Outside("Recupera la tessera d'accesso da Teschio al suo covo: stordiscilo e premi E per perquisirlo, oppure trova un'altra soluzione.", EntPos("boss"));
                if (G.flags["hint_terminal"]) Outside("(Facoltativo) Viola il terminale al cancello della Base Delta-9.", V3(197, 0, 212));
            } else if (Q.stage == 45) Outside("Apri il cancello della Base Delta-9.", V3(198, 0, 220));
            else if (Q.stage == 50) Outside("Entra nei Laboratori Delta-9.", V3(272, 0, 220));
            else if (Q.stage == 55) {
                if (pz == ZONE_BASE) Add("Trova la cella di fusione nel laboratorio.", ZONE_BASE, V3(0, 0, 74));
                else Outside("Entra nei Laboratori Delta-9.", V3(272, 0, 220));
            } else if (Q.stage == 60) ToVault("Porta la cella di fusione al reattore del Rifugio 33.", V3(-22, 0, 8));
            break;
        case Q_ROACH:
            if (Q.stage == 10) Outside(Fmt("Neutralizza con il blaster i radscarafaggi alla Stazione Atomo (%d rimasti).", RoachesRemaining()), V3(60, 0, 180));
            else if (Q.stage == 20) Outside("Torna da Tano per la ricompensa.", EntPos(nullptr, "barman"));
            break;
        case Q_MEDS:
            if (Q.stage == 10) Outside("Trova i rifornimenti medici nell'ospedale di Nuova Speranza.", V3(96, 0, -194));
            else if (Q.stage == 20) Outside("Porta i rifornimenti alla dottoressa Rosi.", EntPos(nullptr, "doctor"));
            break;
        case Q_BEAST:
            if (Q.stage == 10) Outside("Affronta e stordisci lo Squarciatore nella gola a nord-ovest.", EntPos("beast"));
            else if (Q.stage == 20) {
                if (G.player.inv.Count(I_CLAW) > 0) Outside("Porta l'artiglio a Jack Falco.", EntPos(nullptr, "naturalist"));
                else Outside("Mentre lo Squarciatore e' stordito, avvicinati e premi E per recuperare un artiglio come prova.", EntPos("beast"));
            }
            break;
        case Q_SIGNAL:
            if (Q.stage == 10) Outside("Segui il segnale di soccorso fino alla Torre Radio, a ovest.", V3(-280, 0, -10));
            else if (Q.stage == 20) Outside("Trova il deposito dell'esploratore sul bordo orientale del Cratere Verde.", V3(-128, 0, -150));
            break;
    }
    return out;
}

std::string QuestDescription(int q) {
    switch (q) {
        case Q_MAIN: return "Il reattore a fusione del Rifugio 33 si sta spegnendo. Il Sovrintendente mi ha mandato a cercare una cella di fusione sperimentale nella Base Delta-9.";
        case Q_ROACH: return "Tano, il barista di Ferrocittà, vuole che liberi la sua vecchia stazione di servizio dai radscarafaggi.";
        case Q_MEDS: return "La clinica di Ferrocittà ha finito le medicine. La dottoressa Rosi spera in un vecchio deposito nell'ospedale delle rovine.";
        case Q_BEAST: return "Jack Falco, un cacciatore, offre una taglia per neutralizzare lo Squarciatore che infesta la gola a nord-ovest e riportarne un artiglio come prova.";
        case Q_SIGNAL: return "La radio del Pip-Boy capta un vecchio segnale di soccorso proveniente da ovest.";
    }
    return "";
}

void QuestsOnPhoto(const std::string&) {
    // Il catalogo e le sfide multigiocatore restano attivita' facoltative, non missioni della campagna.
}

void QuestsOnStun(Entity& e) {
    if (e.tag == "gas_roach") {
        bool first = G.flags[RoachFlag(e)] == 0;
        G.flags[RoachFlag(e)] = 1;
        if (first && QUESTS[Q_ROACH].stage == 10) {
            int remaining = RoachesRemaining();
            Notify(Fmt("Infestazione: %d radscarafaggi da neutralizzare.", remaining), Col(255, 230, 140));
            if (remaining == 0) SetQuestStage(Q_ROACH, 20);
        }
    }
    if (e.tag == "beast") {
        G.flags["original_beast_stunned"] = 1;
        if (QUESTS[Q_BEAST].stage == 10) SetQuestStage(Q_BEAST, 20);
    }
    if (e.tag == "boss") {
        G.flags["original_boss_stunned"] = 1;
        if (!G.flags["keycard_given"]) Notify("Teschio e' stordito. Avvicinati e premi E per prendere la tessera.", Col(255, 230, 140));
    }
}

void QuestsOnRepair(const std::string& tag) {
    // Riparazioni ambientali: non sostituiscono gli obiettivi delle cinque missioni originali.
    if (tag == "pump") {
        G.flags["pump_fixed"] = 1;
        Notify("L'acqua torna a scorrere verso Ferrocitta'!", Col(170, 220, 255));
    } else if (tag == "transmitter") {
        G.flags["transmitter_fixed"] = 1;
        Notify("Il trasmettitore torna a funzionare.", Col(170, 220, 255));
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
    if (item == I_CLAW && G.flags["original_beast_stunned"] && QUESTS[Q_BEAST].stage == 10) SetQuestStage(Q_BEAST, 20);
    if ((item == I_SCOUTNOTE || item == I_CACHEKEY) && !QUESTS[Q_SIGNAL].done) {
        if (QUESTS[Q_SIGNAL].stage == 0) SetQuestStage(Q_SIGNAL, 10);
        SetQuestStage(Q_SIGNAL, 20);
    }
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

void QuestsUpdate(float dt) {
    static float acc = 0;
    acc += dt;
    if (acc < 0.5f) return;
    acc = 0;
    LocationsUpdate();
    if (QUESTS[Q_MAIN].stage == 20 && W.locations.size() > 1 && W.locations[1].discovered) SetQuestStage(Q_MAIN, 30);
    if (G.flags["raider_pass"])
        for (auto& e : G.ents)
            if (e.fac == FAC_RAIDER && e.hostile) {
                e.hostile = false;
                e.aware = false;
                e.ai = AI_IDLE;
                e.alert = 0;
            }
    if (QUESTS[Q_ROACH].stage == 10 && RoachesRemaining() == 0) SetQuestStage(Q_ROACH, 20);
    if (QUESTS[Q_SIGNAL].stage >= 20 && !QUESTS[Q_SIGNAL].done) {
        Interactable* cache = FindInterByTag("cache");
        if (cache && !cache->locked) CompleteQuest(Q_SIGNAL);
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
            Say("Prendi questo blaster: stordisce senza uccidere. Ti servirà contro i predoni e i guardiani della base. Porta anche la fotocamera, le esche, i kit medici e qualche tappo. Il Pip-Boy ti mostrerà mappa e obiettivi. La porta principale è in fondo al corridoio nord. E... torna a casa, " + Name() + ".");
            Opt("[Prendi l'equipaggiamento] Tornerò.", [] {
                GiveItem(I_CAMERA, 1);
                if (!Has(I_BLASTER)) GiveItem(I_BLASTER, 1);
                GiveItem(I_BAIT, 3);
                GiveItem(I_NOISE, 3);
                GiveItem(I_MEDKIT, 3);
                GiveItem(I_RADAWAY, 1);
                GiveItem(I_BOBBYPIN, 4);
                GiveItem(I_REPAIRKIT, 1);
                GiveItem(I_PARTS, 1);
                GiveItem(I_CAPS, 60);
                EquipTool(TOOL_BLASTER);
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
    bool bossSubdued = G.flags["original_boss_stunned"] != 0;
    switch (n) {
        case 0:
            if (!G.flags["met_sheriff"]) Say("Un abitante dei Rifugi, eh? Si vede da quella tuta. Benvenuto a Ferrocittà. Tieni il blaster nella fondina e andremo d'accordo.");
            else Say("Di nuovo tu. Cosa ti serve?");
            G.flags["met_sheriff"] = 1;
            if (st >= 20 && st <= 30) Opt("Cerco la Base Delta-9.", [] { Go(1); });
            if (st == 40 && !bossSubdued) Opt("Dove trovo Teschio?", [] { Go(7); });
            if (bossSubdued && !G.flags["bounty_paid"]) Opt("Ho neutralizzato Teschio.", [] { Go(8); });
            Opt("Che posto è Ferrocittà?", [] { Go(5); });
            Opt("Arrivederci.", [] { End(); });
            break;
        case 1:
            Say("Delta-9? Nessuno ci entra da decenni. Il cancello è sigillato e serve una tessera d'accesso militare. L'unica che conosco ce l'ha Teschio, il capo dei predoni accampati al vecchio deposito a nord-est. Ci ha portato via tre carovane quest'anno.");
            Opt("Allora gliela prenderò io.", [] { Go(2); });
            Opt(TagSk(SK_SPEECH, 40) + "Una sceriffa come te avrà qualche asso nella manica.", [] { Go(PassSkill(SK_SPEECH, 40) ? 3 : 4); });
            break;
        case 2:
            Say("Mi piace il tuo fegato. Se neutralizzi Teschio ti pagherò 200 tappi di taglia. E la tessera... è tutta tua.");
            Opt("Affare fatto.", [] { SetQuestStage(Q_MAIN, 40); End(); });
            break;
        case 3:
            Say("[Successo] ...D'accordo. Il mio predecessore teneva un diario: il terminale al cancello di Delta-9 usa una parola d'accesso di sette lettere. Chi se ne intende di computer potrebbe forzarlo. Ma la taglia su Teschio resta valida.");
            Opt("Grazie, sceriffa.", [] { G.flags["hint_terminal"] = 1; SetQuestStage(Q_MAIN, 40); End(); });
            break;
        case 4:
            Say("[Fallito] Assi nella manica? Ho un distintivo e una pistola. Se vuoi la tessera, vai a prendertela.");
            Opt("Va bene.", [] { SetQuestStage(Q_MAIN, 40); End(); });
            break;
        case 5:
            Say("Ferrocittà è nata attorno al vecchio deposito ferroviario. Abbiamo acqua pulita, una dottoressa, l'emporio di Gus e il bar di Tano. E me, che tengo l'ordine.");
            Opt("Capito.", [] { Go(0); });
            break;
        case 7:
            Say("Il Covo dei Teschi è a nord-est: segui la strada che esce dal cancello est. Sono almeno una decina, con una torretta di guardia. Non andarci a mani vuote.");
            Opt("Grazie.", [] { Go(0); });
            break;
        case 8:
            Say("Ho sentito gli spari fin da qui. Hai davvero messo fuori combattimento Teschio? Ecco la taglia: 200 tappi. Ferrocittà ti è debitrice.");
            Opt("È stato un piacere.", [] {
                GiveItem(I_CAPS, 200);
                G.player.karma += 20;
                G.flags["bounty_paid"] = 1;
                GiveXP(100, "Taglia su Teschio");
                End();
            });
            break;
    }
}

void DlgTrader(int n) {
    switch (n) {
        case 0:
            Say("Benvenuto all'Emporio di Gus! Blaster, ricambi, rottami e tutto ciò che il deserto non si è ancora mangiato.");
            Opt("Fammi vedere la merce.", [] { End(); BarterOpen(0, G.dlg.npc); });
            Opt("Novità in giro?", [] { Go(1); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1: {
            static const char* r[] = {
                "Nella gola a nord-ovest vive una bestia enorme. Jack Falco ne sa qualcosa: lo trovi vicino al fuoco in piazza.",
                "Il Cratere Verde a ovest brilla di notte. Se ci vai, porta Rad-Via. Tanto Rad-Via.",
                "Sulle frequenze radio c'è un vecchio segnale di soccorso. Viene da ovest, dalla torre. Accendi la radio del tuo aggeggio da polso.",
                "Le armi a energia sono rare, ma potenti. Se ne trovi, so io chi te le paga bene.",
            };
            Say(r[(int)(G.time * 3) % 4]);
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
            if (rq == 20) Opt("La Stazione Atomo e' ripulita.", [] { Go(3); });
            Opt("Raccontami di questo posto.", [] { Go(4); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1:
            Say("Avevo una stazione di servizio a sud, la Stazione Atomo. Ora e' un nido di radscarafaggi grossi come cani. Neutralizzali con il blaster e ti daro' cento tappi... e qualcosa di speciale dal retro.");
            Opt("Affare fatto.", [] {
                SetQuestStage(Q_ROACH, 10);
                if (RoachesRemaining() == 0) SetQuestStage(Q_ROACH, 20);
                End();
            });
            Opt("Non mi interessa.", [] { Go(0); });
            break;
        case 3:
            Say("Li hai messi tutti fuori combattimento? Ecco i tuoi cento tappi, e questi kit dal retro. Un lavoro pulito.");
            Opt("Grazie, Tano.", [] {
                GiveItem(I_CAPS, 100);
                GiveItem(I_MEDKIT, 2);
                CompleteQuest(Q_ROACH);
                End();
            });
            break;
        case 4:
            Say("A ovest c'e' il Cratere Verde: una bomba caduta li' non e' mai esplosa del tutto e i ghoul ci sguazzano. A nord ci sono le rovine di Nuova Speranza, piene di mutanti. E a sud-est c'e' la Base Delta-9, da dove nessuno torna.");
            Opt("Allegro.", [] { Go(0); });
            break;
    }
}

void DlgNaturalist(int n) {
    int bq = QUESTS[Q_BEAST].stage;
    bool hasClaw = G.player.inv.Count(I_CLAW) > 0;
    auto Start = [] {
        SetQuestStage(Q_BEAST, 10);
        Entity* b = FindEntityByTag("beast");
        if (b && G.flags["original_beast_stunned"]) SetQuestStage(Q_BEAST, 20);
    };
    switch (n) {
        case 0:
            Say("Hai la faccia di chi cerca guai. Io ne ho uno grosso per te.");
            if (bq == 0) Opt("Che guaio?", [] { Go(1); });
            if (bq > 0 && !QUESTS[Q_BEAST].done && hasClaw && G.flags["original_beast_stunned"]) Opt("Ho l'artiglio dello Squarciatore.", [] { Go(4); });
            if (bq > 0 && !QUESTS[Q_BEAST].done && !hasClaw) Opt("Dove trovo la bestia?", [] { Go(6); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1:
            Say("Nella gola a nord-ovest c'è uno Squarciatore. Tre metri di artigli e cattiveria. Ha sbranato meta' della mia squadra. Stordiscilo col blaster e portami un suo artiglio: avrai 300 tappi.");
            Opt("Lo farò.", [Start] { Start(); End(); });
            Opt(TagSk(SK_SPEECH, 50) + "300 tappi per affrontare un mostro simile? Facciamo 450.", [] { Go(PassSkill(SK_SPEECH, 50) ? 2 : 3); });
            Opt("È troppo per me.", [] { End(); });
            break;
        case 2:
            Say("[Successo] Hai ragione, è praticamente un suicidio. 450 tappi. Ma voglio vedere quell'artiglio.");
            Opt("Affare fatto.", [Start] { G.flags["beast_bonus"] = 1; Start(); End(); });
            break;
        case 3:
            Say("[Fallito] 300. Prendere o lasciare.");
            Opt("Va bene, 300.", [Start] { Start(); End(); });
            Opt("Lascio.", [] { End(); });
            break;
        case 4:
            Say("Per tutti i... l'hai fatto davvero! Ecco i tuoi tappi. E prendi anche un mantello ottico e dei kit medici: io con la caccia ho chiuso.");
            Opt("Grazie, Jack.", [] {
                TakeItem(I_CLAW, 1);
                GiveItem(I_CAPS, G.flags["beast_bonus"] ? 450 : 300);
                GiveItem(I_STEALTHBOY, 1);
                GiveItem(I_MEDKIT, 2);
                G.player.karma += 10;
                CompleteQuest(Q_BEAST);
                End();
            });
            break;
        case 6:
            Say("La gola è a nord-ovest, oltre il Cratere Verde. Segui il canalone fino in fondo. E porta il blaster carico.");
            Opt("Capito.", [] { End(); });
            break;
    }
}

void DlgRita(int) {
    Say("Le Terre Bruciate sono pericolose. Se vai fuori citta', porta il blaster e bada a te.");
    Opt("Lo faro'.", [] { End(); });
}

void DlgRuggine(int) {
    Say("Il cane inclina la testa e ti annusa la mano.");
    Opt("Ciao, bello.", [] { End(); });
}

void DlgScout(int) {
    Say("Quel segnale viene da un vecchio trasmettitore. Controlla le scorte dell'esploratore nella baracca: forse ha lasciato un indizio.");
    Opt("Darò un'occhiata.", [] { End(); });
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
    if (G.ents[npc].stunT > 0) {
        Notify(G.ents[npc].name + " e' stordito. Attendi che si riprenda.", Col(110, 235, 255));
        return;
    }
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
    else if (id == "naturalist" || id == "hunter") DlgNaturalist(n);
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
