// L'Atlante dell'Alba: una campagna di ricerca, ascolto e collaborazione.
// Tutto lo stato persistente vive in G.flags, QUESTS e negli Interactable salvati.
#include "game.h"

namespace {
struct Site {
    const char* tag;
    const char* name;
    int zone;
    float x, z;
    int tool;
};

const Site SITES[] = {
    {"atlas_echo_relay", "Ricevitore Lumen (scanner)", ZONE_WASTE, -265, -20, TOOL_SCANNER},
    {"atlas_echo_voice", "Eco di Ada (registratore)", ZONE_WASTE, -256, -14, TOOL_RECORDER},
    {"atlas_archive_desk", "Registro dell'archivio Lumen", ZONE_BASE, 5, 32, TOOL_HANDS},
    {"atlas_archive_memory", "Memoria di Ada (registratore)", ZONE_BASE, -6, 58, TOOL_RECORDER},
    {"atlas_archive_route", "Ponte dati dell'archivio (multitool)", ZONE_BASE, 6, 77, TOOL_MULTITOOL},
    {"atlas_soil_a", "Sonda del terreno: silo (scanner)", ZONE_WASTE, -155, 156, TOOL_SCANNER},
    {"atlas_soil_b", "Sonda del terreno: sentiero (scanner)", ZONE_WASTE, -127, 150, TOOL_SCANNER},
    {"atlas_soil_c", "Sonda del terreno: campo (scanner)", ZONE_WASTE, -131, 182, TOOL_SCANNER},
    {"atlas_garden_valve", "Valvola d'irrigazione (multitool)", ZONE_WASTE, -146, 176, TOOL_MULTITOOL},
    {"atlas_seedbed", "Vivaio sperimentale di Nora", ZONE_WASTE, -150, 170, TOOL_HANDS},
    {"atlas_sky_west", "Riferimento del tramonto (scanner)", ZONE_WASTE, -211, -63, TOOL_SCANNER},
    {"atlas_sky_east", "Riferimento dell'alba (scanner)", ZONE_WASTE, 168, 196, TOOL_SCANNER},
    {"atlas_sky_north", "Riferimento della stella ferma (scanner)", ZONE_WASTE, -206, -231, TOOL_SCANNER},
    {"atlas_lens", "Ottica della Specola (multitool)", ZONE_WASTE, -218, -216, TOOL_MULTITOOL},
    {"atlas_star_chart", "Tavola delle rotte della Specola", ZONE_WASTE, -214, -216, TOOL_HANDS},
    {"atlas_voice_town", "Taccuino del consiglio di Ferrocitta' (registratore)", ZONE_WASTE, -5, 40, TOOL_RECORDER},
    {"atlas_voice_depot", "Taccuino dei Raccoglitori (registratore)", ZONE_WASTE, 198, -88, TOOL_RECORDER},
    {"atlas_council", "Tavolo del patto di Lumen", ZONE_WASTE, 2, 25, TOOL_HANDS},
    {"atlas_link_radio", "Nodo Lumen: radio (multitool)", ZONE_WASTE, -268, -20, TOOL_MULTITOOL},
    {"atlas_link_farm", "Nodo Lumen: fattoria (multitool)", ZONE_WASTE, -134, 172, TOOL_MULTITOOL},
    {"atlas_link_base", "Nodo Lumen: archivio (multitool)", ZONE_BASE, 8, 64, TOOL_MULTITOOL},
    {"atlas_beacon", "Faro di Lumen (registratore)", ZONE_WASTE, 0, 24, TOOL_RECORDER},
};

const Site* SpecFor(const std::string& tag) {
    for (const auto& s : SITES) if (tag == s.tag) return &s;
    return nullptr;
}

void Read(const char* title, const std::string& text,
          const std::vector<std::pair<std::string, std::function<void()>>>& choices = {}) {
    G.dlg = Dialogue();
    G.dlg.npc = -1;
    G.dlg.speaker = title;
    G.dlg.text = text;
    // Le scelte chiudono il pannello prima di agire: nessun PNG fittizio o DialogueBuild.
    if (choices.empty()) {
        G.dlg.opts.push_back({"Annota nel taccuino e continua.", [] { SetGameState(GS_PLAY); }, true});
    } else {
        for (const auto& c : choices) {
            auto act = c.second;
            G.dlg.opts.push_back({c.first, [act] { SetGameState(GS_PLAY); act(); }, true});
        }
    }
    SetGameState(GS_DIALOGUE);
    G.dlg.typeT = 1000;
}

bool Mark(const char* flag, Interactable& it, const char* message) {
    if (G.flags[flag]) { Notify("Questo appunto e' gia' nel tuo Atlante."); return false; }
    G.flags[flag] = 1;
    it.used = true;
    GiveXP(25, message);
    PlaySfx(SFX_DISCOVER);
    SpawnDynLight(it.zone, it.pos, Col(100, 225, 190), 5, 0.5f);
    return true;
}

bool Needs(int q, int stage) {
    if (QUESTS[q].done) { Notify("Hai gia' completato questo capitolo. Gli appunti restano nel Pip-Boy."); return false; }
    if (QUESTS[q].stage != stage) {
        Notify(QUESTS[q].stage == 0 ? "Lumen non ha ancora indicato questo collegamento. Segui il capitolo attivo nel Pip-Boy."
                                        : "Prima completa gli altri passaggi di questo capitolo: TAB, DIARIO, MISSIONI.", Col(255, 220, 140));
        return false;
    }
    return true;
}

void Finish(int q, int next) {
    CompleteQuest(q);
    if (next >= 0 && !QUESTS[next].done && QUESTS[next].stage == 0) SetQuestStage(next, 10);
}

int Sum(const char* a, const char* b, const char* c) {
    return (G.flags[a] != 0) + (G.flags[b] != 0) + (G.flags[c] != 0);
}

void CouncilSigned() {
    if (auto* it = FindInterByTag("atlas_council")) it->used = true;
    Finish(Q_CONCORD, Q_BEACON);
}

void VoiceChoice() {
    Read("Il patto di Lumen", "Acqua e semi non bastano: nell'archivio ci sono anche i ricordi di persone che non hanno mai scelto un pubblico. Il consiglio si fida del tuo giudizio. Nessuna decisione impedira' di condividere le mappe.", {
        {"Anche le voci: condividiamo solo quelle autorizzate.", [] {
            G.flags["atlas_voice"] = 1;
            CouncilSigned();
            Notify("Il patto e' firmato: conoscenza aperta e consenso per ogni voce.");
        }},
        {"Solo le mappe: custodiamo le memorie personali.", [] {
            G.flags["atlas_voice"] = 2;
            CouncilSigned();
            Notify("Il patto e' firmato: mappe aperte, memorie affidate alle famiglie.");
        }},
        {"Voglio pensarci ancora.", [] {}}
    });
}

void Consent(Interactable& it, const char* flag, const char* group, const char* text) {
    if (G.flags[flag]) { Notify("Il consenso di questa comunita' e' gia' annotato."); return; }
    const std::string tag = it.tag;
    Read(group, text, {
        {"Registriamo il vostro consenso, con le vostre condizioni.", [tag, flag] {
            auto* site = FindInterByTag(tag);
            if (site) Mark(flag, *site, "Una voce nell'Atlante");
            if (G.flags["atlas_consent_town"] && G.flags["atlas_consent_depot"]) SetQuestStage(Q_CONCORD, 20);
        }},
        {"Prima voglio ascoltare ancora: tornero'.", [] {}}
    });
}

bool Act(Interactable& it, int tool) {
    const std::string& t = it.tag;
    if (t == "atlas_echo_relay") {
        if (!Needs(Q_ECHO, 10)) return true;
        if (Mark("atlas_echo_scanned", it, "Frequenza ritrovata")) {
            SetQuestStage(Q_ECHO, 20);
            Read("Una voce nel fruscio", "Lo scanner separa il vento da un segnale ripetuto: tre note, poi una voce. LUMEN non e' il nome di una persona. Il ricevitore vicino conserva la frase completa: avvicinati con il registratore. Sul metallo qualcuno ha inciso: 'Non cercate un tesoro. Cercate chi e' rimasto in ascolto'.");
        }
    } else if (t == "atlas_echo_voice") {
        if (!Needs(Q_ECHO, 20)) return true;
        if (Mark("atlas_echo_recorded", it, "La voce di Ada")) {
            Finish(Q_ECHO, Q_ARCHIVE);
            Read("Registrazione 01 / Ada", "'L'alba non si e' spenta. Ho soltanto chiuso le finestre. Negli archivi di Delta-9 ci sono strade, sementi e previsioni del tempo. Nessuno dovra' possederle da solo. Se vuoi riaprire Lumen, comincia dal registro dei tecnici, oltre il corridoio di sorveglianza'. La voce si interrompe prima di dire perche' Ada e' rimasta qui.");
        }
    } else if (t == "atlas_archive_desk") {
        if (!Needs(Q_ARCHIVE, 10)) return true;
        Mark("atlas_manifest", it, "Il registro dei tecnici");
        SetQuestStage(Q_ARCHIVE, 20);
        Read("Registro di Delta-9", "L.U.M.E.N.: Laboratori Uniti per Memoria, Ecologia e Navigazione. Ultima richiesta: salvare i dati delle comunita', non catalogare proprieta'. Ada ha lasciato una seconda registrazione nell'ala del laboratorio. Le sentinelle seguono ancora i loro giri: osserva le pattuglie, usa i ripari e passa quando guardano altrove. Un'autorizzazione di manutenzione permette anche il passaggio regolare.");
    } else if (t == "atlas_archive_memory") {
        if (!Needs(Q_ARCHIVE, 20)) return true;
        if (Mark("atlas_archive_voice", it, "Il motivo del silenzio")) {
            SetQuestStage(Q_ARCHIVE, 30);
            Read("Registrazione 02 / La scelta di Ada", "'Ci chiedevano l'elenco dei pozzi e dei semi, ma non volevano invitare i villaggi al tavolo. Ho messo la rete in attesa: non per nascondere il mondo, ma perche' potesse essere raccontato insieme. Nora ha conservato il giardino pilota alla fattoria. Ricollega il ponte dati con il multitool: lei ti ha lasciato istruzioni per farlo rifiorire'.");
        }
    } else if (t == "atlas_archive_route") {
        if (!Needs(Q_ARCHIVE, 30)) return true;
        if (Mark("atlas_archive_open", it, "Archivio ricomposto")) {
            G.player.repairs++;
            Finish(Q_ARCHIVE, Q_GARDEN);
            Read("Il sentiero di Nora", "Il ponte dati si riaccende. Tre sonde del terreno sono segnate attorno alla Fattoria Abbandonata: vicino al silo, lungo il sentiero e sul margine del campo. Lo scanner puo' leggerle senza strappare una pianta. Nora aveva scritto: 'Non ricreare il passato. Scopri cosa puo' vivere qui, adesso'.");
        }
    } else if (t == "atlas_soil_a" || t == "atlas_soil_b" || t == "atlas_soil_c") {
        if (!Needs(Q_GARDEN, 10)) return true;
        const char* flag = t == "atlas_soil_a" ? "atlas_soil_a" : t == "atlas_soil_b" ? "atlas_soil_b" : "atlas_soil_c";
        if (Mark(flag, it, "Campione letto senza raccoglierlo")) {
            int n = Sum("atlas_soil_a", "atlas_soil_b", "atlas_soil_c");
            Notify(Fmt("Sonde lette: %d/3. Terreno secco; semi locali resistenti, acqua alle radici e ombra.", n), Col(200, 240, 160));
            if (n == 3) SetQuestStage(Q_GARDEN, 20);
        }
    } else if (t == "atlas_garden_valve") {
        if (!Needs(Q_GARDEN, 20)) return true;
        if (Mark("atlas_garden_water", it, "Irrigazione a goccia")) {
            G.player.repairs++;
            SetQuestStage(Q_GARDEN, 30);
            Read("L'acqua trova una strada", "Il multitool libera la valvola e regola un filo d'acqua, senza svuotare la cisterna. Ora il vivaio puo' accogliere i semi giusti. Il promemoria di Nora dice: 'Qui il sole e' forte. Scegli specie resistenti alla siccita', proteggi le radici, evita di allagare il terreno'.");
        }
    } else if (t == "atlas_seedbed") {
        if (!Needs(Q_GARDEN, 30)) return true;
        Read("Il giardino che aspetta", "Tre vasche vuote, poche gocce e una scatola di semi. Gli appunti delle sonde e il promemoria di Nora permettono di scegliere come iniziare. Puoi riprovare con calma: il vivaio e' un luogo di ricerca.", {
            {"Semi resistenti, radici all'ombra e poca acqua.", [] {
                G.flags["atlas_garden_grown"] = 1;
                if (auto* p = FindInterByTag("atlas_seedbed")) p->used = true;
                Finish(Q_GARDEN, Q_OBSERVATORY);
                Read("Il primo germoglio", "Il coperchio si chiude sulla vasca preparata. Una piccola luce verde conferma: condizioni adatte. Non hai fatto tornare il vecchio mondo; hai dato spazio a qualcosa di nuovo. Nel cassetto di Nora trovi una carta per la Specola dell'Alba: per distribuire l'acqua occorre prevedere le tempeste.");
            }},
            {"Fiori delicati al sole e molta acqua.", [] {
                Notify("Nora suggerisce di rileggere le sonde: siccita', ombra alle radici, acqua a goccia. Puoi riprovare.", Col(255, 225, 150));
            }},
            {"Rileggiamo gli appunti prima di scegliere.", [] {
                Read("Taccuino di Nora", "Le tre sonde concordano: terreno secco ma recuperabile. Semi locali resistenti alla siccita', radici protette dall'ombra e una piccola irrigazione regolare. Il vivaio resta pronto quando vorrai riprovare.");
            }}
        });
    } else if (t == "atlas_sky_west" || t == "atlas_sky_east" || t == "atlas_sky_north") {
        if (!Needs(Q_OBSERVATORY, 10)) return true;
        const char* flag = t == "atlas_sky_west" ? "atlas_sky_west" : t == "atlas_sky_east" ? "atlas_sky_east" : "atlas_sky_north";
        if (Mark(flag, it, "Un punto fermo nel cielo")) {
            int n = Sum("atlas_sky_west", "atlas_sky_east", "atlas_sky_north");
            Notify(Fmt("Riferimenti: %d/3. Tavola: prima il tramonto, poi l'alba, infine la stella ferma.", n), Col(180, 220, 255));
            if (n == 3) SetQuestStage(Q_OBSERVATORY, 20);
        }
    } else if (t == "atlas_lens") {
        if (!Needs(Q_OBSERVATORY, 20)) return true;
        if (Mark("atlas_lens_fixed", it, "Ottica calibrata")) {
            G.player.repairs++;
            SetQuestStage(Q_OBSERVATORY, 30);
            Read("La Specola dell'Alba", "Il multitool riallinea la lente. I tre riferimenti possono diventare una rotta, ma vanno messi nell'ordine della tavola: il punto dove il giorno finisce, quello dove ricomincia, e solo dopo la direzione che resta ferma. La tavola delle rotte e' accanto all'ottica.");
        }
    } else if (t == "atlas_star_chart") {
        if (!Needs(Q_OBSERVATORY, 30)) return true;
        Read("La rotta delle stelle", "Sul quadrante ci sono Ovest, Est e Nord. L'indizio e' inciso sul bordo: 'Il tramonto apre il cammino, l'alba lo rinnova, la stella ferma lo custodisce'. Quale ordine ricompone la carta?", {
            {"Ovest, Est, Nord.", [] {
                G.flags["atlas_route_aligned"] = 1;
                if (auto* p = FindInterByTag("atlas_star_chart")) p->used = true;
                Finish(Q_OBSERVATORY, Q_CONCORD);
                Read("Una carta per chi verra'", "Le linee coincidono: passaggi riparati dal vento, raccolta dell'acqua, sentieri dove le creature possono continuare a vivere. In fondo al foglio Ada ha lasciato una domanda: 'Adesso sai come riaprire Lumen. Hai chiesto a chi abita qui che cosa desidera condividere?'. Ti aspettano i taccuini di Ferrocitta' e dei Raccoglitori.");
            }},
            {"Nord, Ovest, Est.", [] {
                Notify("La carta resta fuori registro. Il tramonto e' a Ovest, l'alba a Est; la stella ferma indica Nord. Riprova senza perdere nulla.");
            }},
            {"Est, Nord, Ovest.", [] {
                Notify("I tre riferimenti non coincidono ancora. Rileggi l'incisione sul quadrante e riprova.");
            }}
        });
    } else if (t == "atlas_voice_town") {
        if (!Needs(Q_CONCORD, 10)) return true;
        Consent(it, "atlas_consent_town", "Il consiglio di Ferrocitta'", "Vance, Rita e Rosi hanno raccolto desideri diversi. La clinica vuole acqua affidabile; i bambini un giardino; le famiglie vogliono scegliere quali ricordi rendere pubblici. Il loro messaggio per Lumen e' semplice: mappe aperte a tutti, memorie soltanto con il consenso di chi le custodisce.");
    } else if (t == "atlas_voice_depot") {
        if (!Needs(Q_CONCORD, 10)) return true;
        Consent(it, "atlas_consent_depot", "I Raccoglitori", "Neri riconosce il nome di Ada: era sua nonna. Il deposito conservava i suoi registri, non un tesoro. I Raccoglitori offrono ricambi e accettano di condividere i sentieri, a una condizione: nessuno potra' chiudere l'acqua o i semi agli altri. 'La sua ultima richiesta era di restare in ascolto', dice Neri.");
    } else if (t == "atlas_council") {
        if (!Needs(Q_CONCORD, 20)) return true;
        if (G.flags["atlas_priority"]) { VoiceChoice(); return true; }
        Read("Un patto per il domani", "I due messaggi sono sul tavolo. Lumen potra' sostenere entrambi i progetti, ma il primo lavoro comune dara' il tono alla nuova rete. Nessuna comunita' perdera' l'accesso ai dati: scegli da dove cominciare.", {
            {"Condividiamo l'acqua: prima una rete di cisterne aperte.", [] { G.flags["atlas_priority"] = 1; VoiceChoice(); }},
            {"Proteggiamo i semi: prima vivai affidati alle comunita'.", [] { G.flags["atlas_priority"] = 2; VoiceChoice(); }},
            {"Voglio ancora esplorare prima di decidere.", [] {}}
        });
    } else if (t == "atlas_link_radio" || t == "atlas_link_farm" || t == "atlas_link_base") {
        if (!Needs(Q_BEACON, 10)) return true;
        const char* flag = t == "atlas_link_radio" ? "atlas_link_radio" : t == "atlas_link_farm" ? "atlas_link_farm" : "atlas_link_base";
        if (Mark(flag, it, "Un nodo torna a comunicare")) {
            G.player.repairs++;
            int n = Sum("atlas_link_radio", "atlas_link_farm", "atlas_link_base");
            Notify(Fmt("Nodi sincronizzati: %d/3. Il multitool riapre il collegamento senza spegnere le sentinelle.", n));
            if (n == 3) SetQuestStage(Q_BEACON, 20);
        }
    } else if (t == "atlas_beacon") {
        if (!Needs(Q_BEACON, 20)) return true;
        Read("Il primo segnale", "Tre luci stabili: archivio, fattoria, radio. Nel registratore ci sono le mappe e il patto delle comunita'. Il messaggio di Ada aspetta una risposta. Quando sarai pronto, il primo segnale potra' partire da Ferrocitta'.", {
            {"Invia l'Atlante: qui c'e' ancora qualcuno in ascolto.", [] {
                G.flags["atlas_beacon_on"] = 1;
                if (auto* p = FindInterByTag("atlas_beacon")) p->used = true;
                Finish(Q_BEACON, -1);
                Read("L'alba risponde", "Per un istante senti soltanto fruscio. Poi, da molto lontano: 'Ricevuto, Lumen. Anche noi'. La mappa del Pip-Boy si riempie di sentieri che nessuno percorreva da anni. Ora puoi riportare la cella al Rifugio: il suo reattore diventera' un nodo della rete, non piu' una luce isolata.");
            }},
            {"Non ancora: voglio controllare gli appunti.", [] {}}
        });
    } else return false;
    return true;
}

std::string Hint(const std::string& tag) {
    if (tag == "atlas_seedbed") return "Le sonde parlano di siccita': semi resistenti, ombra alle radici, acqua a goccia.";
    if (tag == "atlas_star_chart") return "Incisione: prima il tramonto, poi l'alba, infine la stella ferma. Ovest, Est, Nord.";
    if (tag == "atlas_council") return "Ascolta e registra il consenso di entrambe le comunita', poi torna al tavolo.";
    return "Avvicinati, osserva il pannello e usa lo strumento indicato. Il Pip-Boy conserva i passaggi del capitolo attivo.";
}

std::string Remembered(const std::string& tag) {
    if (tag == "atlas_echo_relay") return "Appunto: il ricevitore distingue tre note dal vento. Sul metallo e' inciso: 'Non cercate un tesoro. Cercate chi e' rimasto in ascolto'. La registrazione vicina contiene la voce completa.";
    if (tag == "atlas_echo_voice") return "Registrazione 01 / Ada: 'L'alba non si e' spenta. Ho soltanto chiuso le finestre. Negli archivi di Delta-9 ci sono strade, sementi e previsioni del tempo. Nessuno dovra' possederle da solo'. Il registro dei tecnici e' oltre il corridoio di sorveglianza.";
    if (tag == "atlas_archive_desk") return "L.U.M.E.N.: Laboratori Uniti per Memoria, Ecologia e Navigazione. Ultima richiesta: salvare i dati delle comunita', non catalogare proprieta'. Ada ha lasciato una registrazione nell'ala del laboratorio.";
    if (tag == "atlas_archive_memory") return "Registrazione 02 / Ada: 'Ho messo la rete in attesa perche' chi chiedeva pozzi e semi non voleva invitare i villaggi al tavolo. Non per nascondere il mondo: perche' potesse essere raccontato insieme'. Nora ha lasciato un vivaio pilota alla fattoria.";
    if (tag == "atlas_archive_route") return "Il ponte dati e' collegato. Tre sonde indicano il cammino di Nora: silo, sentiero e margine del campo. 'Non ricreare il passato. Scopri cosa puo' vivere qui, adesso'.";
    if (tag.find("atlas_soil_") == 0 || tag == "atlas_garden_valve" || tag == "atlas_seedbed") return "Taccuino di Nora: il terreno e' secco ma recuperabile. Semi locali resistenti alla siccita', radici protette dall'ombra e acqua a goccia. Il vivaio rispetta lo spazio delle creature e lascia acqua anche alle altre piante.";
    if (tag.find("atlas_sky_") == 0 || tag == "atlas_lens" || tag == "atlas_star_chart") return "La tavola della Specola dice: 'Il tramonto apre il cammino, l'alba lo rinnova, la stella ferma lo custodisce'. Ovest, Est, Nord. La carta indica sentieri riparati dal vento e passaggi che lasciano spazio alle creature.";
    if (tag == "atlas_voice_town") return "Consenso di Ferrocitta': mappe aperte a tutti e memorie personali soltanto con il consenso di chi le custodisce. Vance chiede cisterne affidabili; Rita sogna un giardino; Rosi vuole condividere conoscenze per la clinica.";
    if (tag == "atlas_voice_depot") return "Consenso dei Raccoglitori: ricambi e sentieri saranno condivisi. Nessuno potra' chiudere acqua e semi agli altri. Neri ha riconosciuto in Ada sua nonna: 'La sua ultima richiesta era di restare in ascolto'.";
    if (tag == "atlas_council") return std::string("Il patto e' firmato. Il primo lavoro comune sara' ") + (G.flags["atlas_priority"] == 1 ? "una rete di cisterne aperte. " : "una rete di vivai comunitari. ") + (G.flags["atlas_voice"] == 1 ? "Si condivideranno anche le voci autorizzate, rispettando il consenso di ogni famiglia." : "Le mappe saranno pubbliche; le memorie personali resteranno affidate alle famiglie.");
    if (tag == "atlas_beacon") return "Il primo segnale e' partito da Ferrocitta'. Dopo un lungo fruscio, una voce lontana ha risposto: 'Ricevuto, Lumen. Anche noi'. Il Rifugio 33 puo' diventare il quarto nodo: una luce collegata alle altre.";
    return "Questo nodo e' collegato. Archivio, fattoria e radio possono scambiarsi mappe e conoscenze, con le condizioni concordate dalle comunita'.";
}
} // namespace

void AdventureBuildSites() {
    for (const auto& s : SITES) {
        bool exists = false;
        for (const auto& it : gInterTemplates) if (it.tag == s.tag) { exists = true; break; }
        if (exists) continue;
        Vector3 p = V3(s.x, s.zone == ZONE_WASTE ? TerrainHeight(s.x, s.z) : 0, s.z);
        p.y = GroundHeight(s.zone, p + V3(0, 0.7f, 0), 0.7f);
        // Ogni strumento deve essere raggiungibile a piedi: niente pannelli dentro un muro.
        if (CircleBlocked(s.zone, p, 0.55f, 1.8f, 0.6f)) {
            bool found = false;
            for (int radius = 2; radius <= 8 && !found; radius += 2)
                for (int k = 0; k < 12; k++) {
                    float a = k * PI / 6;
                    Vector3 q = p + V3(cosf(a) * radius, 0, sinf(a) * radius);
                    q.y = s.zone == ZONE_WASTE ? TerrainHeight(q.x, q.z) : 0;
                    q.y = GroundHeight(s.zone, q + V3(0, 0.7f, 0), 0.7f);
                    if (!CircleBlocked(s.zone, q, 0.55f, 1.8f, 0.6f)) { p = q; found = true; break; }
                }
        }
        Interactable it;
        it.type = INT_TERMINAL;
        it.zone = s.zone;
        it.pos = p + V3(0, 0.9f, 0);
        it.name = s.name;
        it.tag = s.tag;
        it.size = V3(0.65f, 0.65f, 0.65f);
        gInterTemplates.push_back(it);
        W.zones[s.zone].lights.push_back({it.pos + V3(0, 0.4f, 0), Col(100, 220, 190), 3.5f, 0.35f});
    }
}

void AdventureBegin() {
    if (G.flags["atlas_started"]) return;
    G.flags["atlas_started"] = 1;
    for (int id : {I_SCANNER, I_MULTITOOL, I_RECORDER})
        if (G.player.inv.Count(id) == 0) GiveItem(id);
    SetQuestStage(Q_ECHO, 10);
    Notify("Nell'Atlante: una voce nel fruscio. Lo scanner cerca un ricevitore vicino alla Torre Radio.", Col(180, 230, 255));
}

bool AdventureReadyForEnding() {
    for (int q = Q_ECHO; q <= Q_BEACON; q++) if (!QUESTS[q].done) return false;
    return G.flags["atlas_beacon_on"] != 0;
}

bool AdventureInteract(int index) {
    if (index < 0 || index >= (int)G.inters.size()) return false;
    Interactable& it = G.inters[index];
    if (it.type == INT_REACTOR && !QUESTS[Q_MAIN].done && !AdventureReadyForEnding()) {
        Notify("Il Rifugio puo' aspettare: prima ricomponi l'Atlante e invia il primo segnale. TAB mostra il capitolo attivo.", Col(255, 230, 140));
        return true;
    }
    const Site* s = SpecFor(it.tag);
    if (!s) return false;
    Vector3 delta = it.pos - PlayerEye();
    float distance = Vector3Length(delta);
    if (it.zone != G.player.zone || distance > 5.0f ||
        !LineOfSight(it.zone, PlayerEye(), it.pos)) {
        Notify("Avvicinati al pannello da un passaggio libero.");
        return true;
    }
    if (!G.flags["atlas_started"]) AdventureBegin();
    if (it.used) {
        Read(s->name, Remembered(it.tag));
        return true;
    }
    if (s->tool != TOOL_HANDS) {
        Read(s->name, Hint(it.tag) + std::string(" Strumento richiesto: ") + ToolName(s->tool) + ". Equipaggialo con i tasti 1-7 o la rotella, poi usa il clic sinistro. Questi strumenti osservano, registrano o riparano; non hanno effetto sulle persone.");
        return true;
    }
    return Act(it, TOOL_HANDS);
}

bool AdventureUseTool(int tool) {
    if (tool != TOOL_SCANNER && tool != TOOL_MULTITOOL && tool != TOOL_RECORDER) return false;
    if (G.player.inv.Count(ToolItem(tool)) == 0 || G.player.tool != tool) {
        Notify("Equipaggia lo strumento richiesto prima di usarlo.", Col(255, 220, 140));
        return true;
    }
    const Site* bestSpec = nullptr;
    Interactable* best = nullptr;
    float score = 1e9f;
    const Vector3 eye = PlayerEye(), forward = PlayerForward();
    for (auto& it : G.inters) {
        if (it.zone != G.player.zone) continue;
        const Site* s = SpecFor(it.tag);
        if (!s || s->tool != tool) continue;
        Vector3 delta = it.pos - eye;
        float dist = Vector3Length(delta);
        if (dist > (tool == TOOL_SCANNER ? 7.0f : 4.5f)) continue;
        float facing = dist > 0.01f ? Vector3DotProduct(delta / dist, forward) : 1;
        if (facing < 0.5f || !LineOfSight(it.zone, eye, it.pos)) continue;
        float value = dist + (1 - facing) * 4;
        if (value < score) { score = value; best = &it; bestSpec = s; }
    }
    if (!best || !bestSpec) return false;
    if (!G.flags["atlas_started"]) AdventureBegin();
    return Act(*best, tool);
}

void AdventureUpdate(float dt) {
    // Nessun timer narrativo, risveglio o perdita di progresso: si esplora con calma.
    // Chi ha trovato un indizio prima dell'incarico puo' ricominciare dalla radio.
    if (G.flags["atlas_started"] && QUESTS[Q_ECHO].stage == 0) SetQuestStage(Q_ECHO, 10);
}

std::string AdventureDescription(int quest) {
    switch (quest) {
        case Q_ECHO: return "Una frase riaffiora dal fruscio della Torre Radio: 'L'alba non si e' spenta'. Lo scanner puo' isolare la frequenza; il registratore custodira' la voce di Ada, prima custode di Lumen. Che cosa ha scelto di lasciare in silenzio?";
        case Q_ARCHIVE: return "Lumen non era un tesoro nascosto: era una rete civile di memorie, sementi, acqua e rotte sicure. Negli archivi di Delta-9 le sentinelle continuano i loro giri. Leggi i registri, ascolta Ada e ricollega il ponte dati; passa inosservato o usa un'autorizzazione di manutenzione.";
        case Q_GARDEN: return "Nora ha lasciato un vivaio e tre sonde nella vecchia fattoria. Leggi il terreno senza raccogliere campioni, ripristina l'irrigazione con il multitool e scegli le condizioni giuste per un primo germoglio. Appunti: semi resistenti alla siccita', ombra alle radici e acqua a goccia.";
        case Q_OBSERVATORY: return "Per condividere l'acqua occorre conoscere il cielo. Tre riferimenti nelle Terre Bruciate permettono di calibrare la Specola dell'Alba. La tavola indica l'ordine: prima il tramonto (Ovest), poi l'alba (Est), infine la stella ferma (Nord). Gli errori si possono correggere senza perdere materiali.";
        case Q_CONCORD: return "Ada mise la rete in attesa perche' nessuna comunita' la possedesse da sola. Ascolta Ferrocitta' e i Raccoglitori e registra il loro consenso; poi scegli il primo progetto comune e come custodire le memorie personali. Acqua condivisa o vivai comunitari: entrambe le scelte costruiscono un futuro.";
        case Q_BEACON: return "L'Atlante e' completo e le comunita' hanno firmato il patto. Ricollega con il multitool i nodi della radio, della fattoria e dell'archivio, poi usa il registratore al faro di Ferrocitta' per inviare il primo messaggio. Dopo la risposta, il Rifugio 33 potra' diventare il quarto nodo.";
        default: return "";
    }
}

std::vector<QuestObjective> AdventureObjectives(int quest) {
    std::vector<QuestObjective> out;
    auto Add = [&](const char* tag, const std::string& text) {
        auto* it = FindInterByTag(tag);
        if (!it) return;
        QuestObjective obj;
        obj.text = text;
        obj.zone = it->zone;
        obj.pos = it->pos;
        obj.hasPos = true;
        // La bussola indica l'uscita quando l'obiettivo si trova in un'altra zona.
        if (G.player.zone == ZONE_VAULT && it->zone != ZONE_VAULT) { obj.zone = ZONE_VAULT; obj.pos = V3(0, 0, -30); }
        else if (G.player.zone == ZONE_BASE && it->zone != ZONE_BASE) { obj.zone = ZONE_BASE; obj.pos = V3(0, 0, -5); }
        else if (G.player.zone == ZONE_WASTE && it->zone == ZONE_BASE) { obj.zone = ZONE_WASTE; obj.pos = V3(272, 0, 220); }
        out.push_back(obj);
    };
    int st = QUESTS[quest].stage;
    switch (quest) {
        case Q_ECHO:
            if (st == 10) Add("atlas_echo_relay", "Isola il segnale Lumen con lo scanner, accanto alla Torre Radio.");
            else if (st == 20) Add("atlas_echo_voice", "Ascolta l'eco di Ada con il registratore.");
            break;
        case Q_ARCHIVE:
            if (st == 10) Add("atlas_archive_desk", "Leggi il registro dell'archivio oltre il corridoio di Delta-9 (E).");
            else if (st == 20) Add("atlas_archive_memory", "Recupera la memoria di Ada con il registratore, evitando le pattuglie.");
            else if (st == 30) Add("atlas_archive_route", "Ricollega il ponte dati con il multitool, nell'ala del laboratorio.");
            break;
        case Q_GARDEN:
            if (st == 10) {
                if (!G.flags["atlas_soil_a"]) Add("atlas_soil_a", "Leggi con lo scanner la sonda vicino al silo.");
                if (!G.flags["atlas_soil_b"]) Add("atlas_soil_b", "Leggi con lo scanner la sonda lungo il sentiero.");
                if (!G.flags["atlas_soil_c"]) Add("atlas_soil_c", "Leggi con lo scanner la sonda al margine del campo.");
            } else if (st == 20) Add("atlas_garden_valve", "Ripristina l'irrigazione a goccia con il multitool.");
            else if (st == 30) Add("atlas_seedbed", "Prepara il vivaio seguendo le letture delle sonde (E).");
            break;
        case Q_OBSERVATORY:
            if (st == 10) {
                if (!G.flags["atlas_sky_west"]) Add("atlas_sky_west", "Misura con lo scanner il riferimento del tramonto, presso il Cratere.");
                if (!G.flags["atlas_sky_east"]) Add("atlas_sky_east", "Misura con lo scanner il riferimento dell'alba, verso Delta-9.");
                if (!G.flags["atlas_sky_north"]) Add("atlas_sky_north", "Misura con lo scanner la stella ferma, sul sentiero della gola.");
            } else if (st == 20) Add("atlas_lens", "Calibra l'ottica della Specola dell'Alba con il multitool.");
            else if (st == 30) Add("atlas_star_chart", "Ricomponi la rotta sulla tavola della Specola (E).");
            break;
        case Q_CONCORD:
            if (st == 10) {
                if (!G.flags["atlas_consent_town"]) Add("atlas_voice_town", "Ascolta Ferrocitta' e registra il suo consenso con il registratore.");
                if (!G.flags["atlas_consent_depot"]) Add("atlas_voice_depot", "Ascolta i Raccoglitori e registra il loro consenso con il registratore.");
            } else if (st == 20) Add("atlas_council", "Scegli il primo progetto comune e come custodire le memorie (E).");
            break;
        case Q_BEACON:
            if (st == 10) {
                if (!G.flags["atlas_link_radio"]) Add("atlas_link_radio", "Ricollega il nodo radio con il multitool.");
                if (!G.flags["atlas_link_farm"]) Add("atlas_link_farm", "Ricollega il nodo della fattoria con il multitool.");
                if (!G.flags["atlas_link_base"]) Add("atlas_link_base", "Ricollega il nodo dell'archivio con il multitool.");
            } else if (st == 20) Add("atlas_beacon", "Invia l'Atlante con il registratore dal faro di Ferrocitta'.");
            break;
    }
    return out;
}
