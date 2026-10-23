// Missioni, dialoghi e contenuti dei terminali.
#include "game.h"

Quest QUESTS[Q_COUNT] = {
    {"Il cuore del Rifugio"},
    {"Infestazione"},
    {"Medicine per Ferrocittà"},
    {"La bestia della gola"},
    {"Segnale nel deserto"},
};

std::vector<TerminalDef> TERMINALS;

static const int QUEST_XP[Q_COUNT] = {500, 120, 160, 300, 150};

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

static int AliveWithTag(const char* tag) {
    int n = 0;
    for (auto& e : G.ents)
        if (e.tag == tag && !e.dead) n++;
    return n;
}

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
                Outside("Recupera la tessera d'accesso da Teschio, al Covo dei Teschi.", EntPos("boss"));
                if (G.flags["hint_terminal"]) Outside("(Facoltativo) Viola il terminale al cancello della Base Delta-9.", V3(197, 0, 212));
            } else if (Q.stage == 45) Outside("Apri il cancello della Base Delta-9.", V3(198, 0, 220));
            else if (Q.stage == 50) Outside("Entra nei Laboratori Delta-9.", V3(272, 0, 220));
            else if (Q.stage == 55) {
                if (pz == ZONE_BASE) Add("Trova la cella di fusione nel laboratorio.", ZONE_BASE, V3(0, 0, 74));
                else Outside("Entra nei Laboratori Delta-9.", V3(272, 0, 220));
            } else if (Q.stage == 60) ToVault("Porta la cella di fusione al reattore del Rifugio 33.", V3(-22, 0, 8));
            break;
        case Q_ROACH:
            if (Q.stage == 10) Outside(Fmt("Elimina i radscarafaggi alla Stazione Atomo (%d rimasti).", AliveWithTag("gas_roach")), V3(60, 0, 180));
            else if (Q.stage == 20) Outside("Torna da Tano per la ricompensa.", EntPos(nullptr, "barman"));
            break;
        case Q_MEDS:
            if (Q.stage == 10) Outside("Trova i rifornimenti medici nell'ospedale di Nuova Speranza.", V3(96, 0, -194));
            else if (Q.stage == 20) Outside("Porta i rifornimenti alla dottoressa Rosi.", EntPos(nullptr, "doctor"));
            break;
        case Q_BEAST:
            if (Q.stage == 10) Outside("Uccidi lo Squarciatore nella gola a nord-ovest.", EntPos("beast"));
            else if (Q.stage == 20) {
                if (G.player.inv.Count(I_CLAW) > 0) Outside("Porta l'artiglio a Jack Falco.", EntPos(nullptr, "hunter"));
                else Outside("Recupera l'artiglio dal corpo dello Squarciatore.", EntPos("beast"));
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
        case Q_BEAST: return "Jack Falco, un cacciatore, offre una taglia per l'uccisione dello Squarciatore che infesta la gola a nord-ovest.";
        case Q_SIGNAL: return "La radio del Pip-Boy capta un vecchio segnale di soccorso proveniente da ovest.";
    }
    return "";
}

void QuestsOnKill(Entity& e) {
    if (e.tag == "beast" && QUESTS[Q_BEAST].stage == 10) SetQuestStage(Q_BEAST, 20);
    if (e.tag == "boss") {
        G.flags["boss_dead"] = 1;
        if (QUESTS[Q_MAIN].stage == 40) Notify("Teschio è morto. Prendi la tessera dal suo corpo.", Col(255, 230, 140));
    }
}

void QuestsOnPickup(int item) {
    if (item == I_KEYCARD && QUESTS[Q_MAIN].stage >= 20 && QUESTS[Q_MAIN].stage < 45) SetQuestStage(Q_MAIN, 45);
    if (item == I_FUSIONCELL && QUESTS[Q_MAIN].stage < 60) {
        if (QUESTS[Q_MAIN].stage < 20) QUESTS[Q_MAIN].stage = 20;
        SetQuestStage(Q_MAIN, 60);
    }
    if (item == I_MEDSUPPLIES && QUESTS[Q_MEDS].stage == 10) SetQuestStage(Q_MEDS, 20);
    if ((item == I_SCOUTNOTE || item == I_CACHEKEY) && QUESTS[Q_SIGNAL].stage < 20 && !QUESTS[Q_SIGNAL].done) {
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
            GiveXP(20);
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
    if (QUESTS[Q_ROACH].stage == 10 && AliveWithTag("gas_roach") == 0) SetQuestStage(Q_ROACH, 20);
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
const std::string& Name() { return G.player.name; }
Entity& NpcE() { return G.ents[G.dlg.npc]; }

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
                Opt("Sto andando.", [] { End(); });
                break;
            case 10:
                Say("Era un laboratorio dell'esercito. Gli ultimi messaggi parlavano di robot di sicurezza fuori controllo. Ferrocittà è a nord-est della nostra uscita: chiedi lì.");
                Opt("Capito.", [] { Go(0); });
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
            Say("Hai ottenuto il punteggio più alto al Test di Attitudine Generale. E nessun altro si è offerto volontario. Non prenderla sul personale.");
            Opt("Capisco. Cosa devo fare?", [] { Go(1); });
            break;
        case 3:
            Say("[Successo] Ci abbiamo provato. Il nucleo è crepato e la schermatura cede: serve una cella nuova. Ma mi piace come ragioni. Prendi anche questi, ti serviranno.");
            Opt("Grazie. Dove trovo una cella nuova?", [] { GiveItem(I_STIMPAK, 2); Go(1); });
            break;
        case 5:
            Say("[Fallito] Apprezzo l'interesse, ma lascia la fisica ai tecnici. Il nucleo è andato.");
            Opt("D'accordo. Cosa devo fare?", [] { Go(1); });
            break;
        case 6:
            Say("Allora fra un mese saremo tutti morti, te compreso. Ma non credo che tu sia quel tipo di persona.");
            Opt("...Va bene. Andrò.", [] { Go(4); });
            break;
        case 4:
            Say("Prendi questa pistola 10mm, delle munizioni e qualche medicina. Il Pip-Boy al tuo polso ti mostrerà mappa e obiettivi. La porta principale è in fondo al corridoio nord. E... torna vivo, " + Name() + ".");
            Opt("[Prendi l'equipaggiamento] Tornerò.", [] {
                GiveItem(I_PISTOL, 1);
                GiveItem(I_AMMO_10, 36);
                GiveItem(I_STIMPAK, 3);
                GiveItem(I_RADAWAY, 1);
                GiveItem(I_BOBBYPIN, 4);
                GiveItem(I_CAPS, 60);
                if (G.player.weapon < 0) EquipWeapon(I_PISTOL);
                SetQuestStage(Q_MAIN, 20);
                End();
            });
            break;
    }
}

void DlgVaultDoc(int n) {
    switch (n) {
        case 0:
            Say(G.player.hp < HPCap() - 1 || G.player.rads > 1 ? "Ferito? Fatti vedere, " + Name() + "." : "Tutto a posto? Ricordati di bere acqua, là fuori.");
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
            Say("Ecco fatto. Cerca di non farti sparare.");
            Opt("Ci proverò.", [] { End(); });
            break;
    }
}

void DlgVaultGuard(int n) {
    if (QUESTS[Q_MAIN].stage < 20) Say("Il Sovrintendente ti sta cercando. Il suo ufficio è a est dell'atrio.");
    else if (G.flags["reactor_fixed"]) Say("Le luci non tremano più. Sei un eroe, " + Name() + ".");
    else Say("Non avrei mai pensato di vedere quella porta aperta. Buona fortuna là fuori.");
    Opt("Grazie.", [] { End(); });
}

void DlgVaultDweller(int n) {
    static const char* lines[] = {
        "Davvero uscirai là fuori? Io non ce la farei mai.",
        "Si dice che fuori il cielo sia arancione. È vero?",
        "Le luci del reattore tremano da settimane. Ho paura.",
        "Mio nonno diceva che prima della guerra c'erano gli alberi verdi. Che strano.",
    };
    if (G.flags["reactor_fixed"]) Say("Il reattore funziona di nuovo! Grazie, " + Name() + "!");
    else Say(lines[(G.dlg.npc * 7) % 4]);
    Opt("Ciao.", [] { End(); });
}

void DlgSheriff(int n) {
    int st = QUESTS[Q_MAIN].stage;
    bool bossDead = G.flags["boss_dead"] != 0;
    switch (n) {
        case 0:
            if (!G.flags["met_sheriff"]) Say("Un abitante dei Rifugi, eh? Si vede da quella tuta. Benvenuto a Ferrocittà. Tieni le mani lontane dalla fondina e andremo d'accordo.");
            else Say("Di nuovo tu. Cosa ti serve?");
            G.flags["met_sheriff"] = 1;
            if (st >= 20 && st <= 30) Opt("Cerco la Base Delta-9.", [] { Go(1); });
            if (st == 40 && !bossDead) Opt("Dove trovo Teschio?", [] { Go(7); });
            if (bossDead && !G.flags["bounty_paid"]) Opt("Teschio è morto.", [] { Go(8); });
            Opt("Che posto è Ferrocittà?", [] { Go(5); });
            Opt("Arrivederci.", [] { End(); });
            break;
        case 1:
            Say("Delta-9? Nessuno ci entra da decenni. Il cancello è sigillato e serve una tessera d'accesso militare. L'unica che conosco ce l'ha Teschio, il capo dei predoni accampati al vecchio deposito a nord-est. Ci ha portato via tre carovane quest'anno.");
            Opt("Allora gliela prenderò io.", [] { Go(2); });
            Opt(TagSk(SK_SPEECH, 40) + "Una sceriffa come te avrà qualche asso nella manica.", [] { Go(PassSkill(SK_SPEECH, 40) ? 3 : 4); });
            break;
        case 2:
            Say("Mi piace il tuo fegato. Se fai fuori Teschio ti pagherò 200 tappi di taglia. E la tessera... è tutta tua.");
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
            Say("Ho sentito gli spari fin da qui. Hai davvero sistemato Teschio? Ecco la taglia: 200 tappi. Ferrocittà ti è debitrice.");
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
            Say("Benvenuto all'Emporio di Gus! Armi, munizioni, rottami e tutto ciò che il deserto non si è ancora mangiato.");
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
    switch (n) {
        case 0:
            Say("Ferito? Irradiato? Siediti e non toccare niente.");
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
            if (!QUESTS[Q_MEDS].done && mq > 0 && G.player.inv.Count(I_MEDSUPPLIES) > 0) Opt("Ho trovato i rifornimenti medici.", [] { Go(6); });
            Opt("Arrivederci.", [] { End(); });
            break;
        case 2:
            Say("Ecco fatto. Cerca di non tornare qui a pezzi.");
            Opt("Grazie.", [] { Go(0); });
            break;
        case 3:
            Say("Niente tappi, niente cure. Le medicine non crescono sugli alberi. Gli alberi, poi, non crescono proprio.");
            Opt("Capisco.", [] { Go(0); });
            break;
        case 4:
            Say("Le scorte sono finite. L'ospedale delle rovine di Nuova Speranza, a nord, aveva un deposito medico. Se me lo porti ti ricompenserò bene. Attento però: quel posto è pieno di ghoul.");
            Opt("Ci penso io.", [] {
                SetQuestStage(Q_MEDS, 10);
                if (G.player.inv.Count(I_MEDSUPPLIES) > 0) SetQuestStage(Q_MEDS, 20);
                End();
            });
            Opt(TagSk(SK_BARTER, 35) + "Quanto 'bene', esattamente?", [] { Go(PassSkill(SK_BARTER, 35) ? 5 : 7); });
            break;
        case 5:
            Say("[Successo] Sei un osso duro. Va bene: aggiungo cento tappi alla ricompensa.");
            Opt("Affare fatto.", [] {
                G.flags["meds_bonus"] = 1;
                SetQuestStage(Q_MEDS, 10);
                if (G.player.inv.Count(I_MEDSUPPLIES) > 0) SetQuestStage(Q_MEDS, 20);
                End();
            });
            break;
        case 7:
            Say("[Fallito] Abbastanza, fidati. Qui la gente muore di febbre, non ho tempo per contrattare.");
            Opt("Va bene, ci vado.", [] {
                SetQuestStage(Q_MEDS, 10);
                if (G.player.inv.Count(I_MEDSUPPLIES) > 0) SetQuestStage(Q_MEDS, 20);
                End();
            });
            break;
        case 6:
            Say("Questi... questi salveranno decine di vite. Grazie, davvero. Prendi questi stimpak e un po' di tappi.");
            Opt("Felice di aiutare.", [] {
                TakeItem(I_MEDSUPPLIES, 1);
                GiveItem(I_STIMPAK, 4);
                GiveItem(I_CAPS, G.flags["meds_bonus"] ? 200 : 100);
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
            if (rq == 20) Opt("La Stazione Atomo è ripulita.", [] { Go(3); });
            Opt("Raccontami di questo posto.", [] { Go(4); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1:
            Say("Avevo una stazione di servizio a sud, la Stazione Atomo. Ora è un nido di radscarafaggi grossi come cani. Ripuliscila e ti darò cento tappi... e qualcosa di speciale dal retro.");
            Opt("Affare fatto.", [] {
                SetQuestStage(Q_ROACH, 10);
                int alive = 0;
                for (auto& e : G.ents)
                    if (e.tag == "gas_roach" && !e.dead) alive++;
                if (alive == 0) SetQuestStage(Q_ROACH, 20);
                End();
            });
            Opt("Non mi interessa.", [] { Go(0); });
            break;
        case 3:
            Say("Li hai schiacciati tutti? Ah! Ecco i tuoi tappi. E questa è la mia vecchia doppietta, con qualche cartuccia. Trattala bene.");
            Opt("Grazie, Tano.", [] {
                GiveItem(I_CAPS, 100);
                GiveItem(I_SHOTGUN, 1);
                GiveItem(I_AMMO_SHELL, 12);
                CompleteQuest(Q_ROACH);
                End();
            });
            break;
        case 4:
            Say("A ovest c'è il Cratere Verde: una bomba caduta lì non è mai esplosa del tutto e i ghoul ci sguazzano. A nord ci sono le rovine di Nuova Speranza, piene di mutanti. E a sud-est c'è la Base Delta-9, da dove nessuno torna.");
            Opt("Allegro.", [] { Go(0); });
            break;
    }
}

void DlgHunter(int n) {
    int bq = QUESTS[Q_BEAST].stage;
    bool hasClaw = G.player.inv.Count(I_CLAW) > 0;
    auto Start = [] {
        SetQuestStage(Q_BEAST, 10);
        Entity* b = FindEntityByTag("beast");
        if (b && b->dead) SetQuestStage(Q_BEAST, 20);
    };
    switch (n) {
        case 0:
            Say("Hai la faccia di chi cerca guai. Io ne ho uno grosso per te.");
            if (bq == 0) Opt("Che guaio?", [] { Go(1); });
            if (bq > 0 && !QUESTS[Q_BEAST].done && hasClaw) Opt("Ho l'artiglio dello Squarciatore.", [] { Go(4); });
            if (bq > 0 && !QUESTS[Q_BEAST].done && !hasClaw) Opt("Dov'è la bestia?", [] { Go(6); });
            Opt("Ciao.", [] { End(); });
            break;
        case 1:
            Say("Nella gola a nord-ovest c'è uno Squarciatore. Tre metri di artigli e cattiveria. Ha sbranato metà della mia squadra. Portami il suo artiglio e avrai 300 tappi.");
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
            Say("Per tutti i... l'hai fatto davvero! Ecco i tuoi tappi. E prendi anche il mio vecchio fucile: io con la caccia ho chiuso.");
            Opt("Grazie, Jack.", [] {
                TakeItem(I_CLAW, 1);
                GiveItem(I_CAPS, G.flags["beast_bonus"] ? 450 : 300);
                GiveItem(I_HUNTRIFLE, 1);
                GiveItem(I_AMMO_308, 20);
                G.player.karma += 10;
                CompleteQuest(Q_BEAST);
                End();
            });
            break;
        case 6:
            Say("La gola è a nord-ovest, oltre il Cratere Verde. Segui il canalone fino in fondo. E porta tante munizioni.");
            Opt("Capito.", [] { End(); });
            break;
    }
}

void DlgSettler(int n) {
    static const char* lines[] = {
        "Bella giornata, se ti piace la polvere.",
        "Hai visto la Stazione Atomo? Una volta ci facevo il pieno col mio vecchio.",
        "Non bere l'acqua del cratere. Fidati.",
        "Lo sceriffo è dura, ma senza di lei i predoni ci avrebbero già spazzati via.",
        "Un abitante dei Rifugi! Avete davvero l'acqua calda, laggiù?",
        "Di notte si sentono i cani selvatici ululare verso la torre radio.",
    };
    if (G.flags["boss_dead"] && (G.dlg.npc % 2 == 0)) Say("Dicono che hai fatto fuori Teschio! Stasera da Tano offro io!");
    else Say(lines[(G.dlg.npc * 5 + (int)(G.time / 30)) % 6]);
    Opt("Ciao.", [] { End(); });
}

void DlgGuard(int n) {
    static const char* lines[] = {"Niente guai, rifugiato.", "Tengo d'occhio la strada. Tu tieni d'occhio le tue tasche.", "Se vedi predoni, corri da noi. Non fare l'eroe."};
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
    else if (id == "hunter") DlgHunter(n);
    else if (id == "guard") DlgGuard(n);
    else DlgSettler(n);
    (void)NpcE;
}

// ---------------------------------------------------------------------------
//  Terminali
// ---------------------------------------------------------------------------
void InitTerminals() {
    TERMINALS.assign(TERM_COUNT, TerminalDef());
    TERMINALS[TERM_OVERSEER] = {"ATOMTEC TERMLINK - RIFUGIO 33 - SOVRINTENDENZA", {
        {"Registro: stato del reattore", "Giorno 31.207.\nIl nucleo a fusione mostra microfratture nella schermatura.\nRendimento al 41%. Stima di spegnimento: 30-40 giorni.\nNessuna cella di ricambio disponibile nel Rifugio.", ""},
        {"Archivio: Base Delta-9", "Secondo gli archivi militari, la Base Delta-9 ospitava il\nProgetto Prometeo: celle a fusione portatili ad alta durata.\nUltima comunicazione ricevuta: ottobre 2077.", ""},
        {"Nota personale", "Ho dovuto scegliere un volontario.\nNessuno di noi sa cosa ci sia davvero là fuori.\nChe Dio mi perdoni se non dovesse tornare.", ""},
    }};
    TERMINALS[TERM_GATE] = {"SISTEMA DI ACCESSO PERIMETRALE - BASE DELTA-9", {
        {"Stato del cancello", "CANCELLO PRINCIPALE: CHIUSO\nPROTOCOLLO DI QUARANTENA: ATTIVO\nPersonale autorizzato: usare la tessera d'accesso.", ""},
        {"Apri il cancello principale", "Comando inviato. Sblocco dei servomotori in corso...", "open_gate"},
    }};
    TERMINALS[TERM_ARMORY] = {"SICUREZZA DELTA-9 - CONTROLLO ARMERIA", {
        {"Inventario armeria", "1x Armatura potenziata Mk.II\n1x Fucile al plasma sperimentale\nCelle a microfusione: 40", ""},
        {"Sblocca la porta dell'armeria", "Serratura magnetica disattivata.", "unlock_armory"},
    }};
    TERMINALS[TERM_LAB] = {"PROGETTO PROMETEO - LABORATORIO PRINCIPALE", {
        {"Rapporto 117", "La cella prototipo P-7 ha superato i test di stabilità.\nAutonomia stimata: 140 anni.\nIl prototipo resta sul piedistallo del laboratorio in attesa\ndel trasporto.", ""},
        {"Rapporto 118", "Le sentinelle Mk.V hanno ricevuto il protocollo di quarantena.\nNessuno deve uscire. Nessuno deve entrare.\nRipeto: nessuno.", ""},
        {"Ultimo messaggio", "Se qualcuno legge... le bombe sono cadute.\nI robot non ci lasciano uscire.\nDite a mia figlia che le voglio bene.", ""},
    }};
    TERMINALS[TERM_RAIDER] = {"TERMINALE PRIVATO DI TESCHIO - NON TOCCARE", {
        {"Bottino", "Carovana di Ferrocittà: 3 casse di roba.\nTessera militare: LA TENGO IO. Nessuno la tocca.\nChi tocca la mia cassaforte finisce sulle picche.", ""},
        {"Sblocca la cassaforte", "Serratura elettronica aperta.", "unlock_safe"},
    }};
    TERMINALS[TERM_CLINIC] = {"CLINICA DI FERROCITTÀ", {{"Scorte", "Stimpak: 0\nRad-Via: 0\nServono rifornimenti.", ""}}};
}
