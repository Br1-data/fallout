# DUSTFALL — Cronache delle Terre Bruciate

Gioco di ruolo d'azione **3D in prima/terza persona**, ambientato nelle Terre Bruciate.
Questa versione riprende il **gioco originale**: combattimento, S.P.A.T., armi diverse,
armature, bottino, predoni, creature e le cinque missioni del Rifugio 33.

Le armi sono **laser stordenti**. Quando la resistenza di un avversario si esaurisce,
rimane **fuori combattimento definitivamente**: non si rialza dopo un conto alla rovescia.
Lo stato resta nei salvataggi e dopo i cambi di zona. Gli avversari vengono neutralizzati,
senza uccisioni o effetti di sangue.

**[Gioca nel browser: br1-data.github.io/fallout](https://br1-data.github.io/fallout/)**

## Inizia una partita

1. Premi **GIOCA**, poi **NUOVA STORIA** per cominciare nel Rifugio 33.
2. Crea il personaggio, parla con il Sovrintendente e recupera l'equipaggiamento.
3. Usa le armi laser per affrontare gli avversari, ricarica le celle con **R** e usa **Q**
   per il puntamento assistito S.P.A.T. Puoi perquisire gli avversari storditi con **E**.

**MONDO APERTO** comincia direttamente a Ferrocittà con l'equipaggiamento iniziale.
Per il ritorno al sistema originale viene usata la cartella di salvataggio `saves-laser`:
le vecchie partite della versione fotografica restano conservate nella loro cartella.
Avvia **NUOVA STORIA** per questa campagna.

## Combattimento laser

Le armi conservano i diversi ruoli del gioco originale: emettitori rapidi, fucili di precisione,
raffiche e impulsi pesanti. Cambiano gittata, precisione, cadenza, celle disponibili e costo
in Punti Azione. Le armature e le abilità influenzano la resistenza e l'efficacia dei colpi.

Gli avversari reagiscono, inseguono, attaccano e cercano riparo. I loro colpi consumano la tua
resistenza: se la esaurisci perdi i sensi e puoi ripartire da un salvataggio. La furtività,
le coperture, i colpi mirati e l'equipaggiamento restano importanti.

Lo **S.P.A.T.** ferma il tempo per scegliere bersaglio e punto da colpire, mostra la probabilità
di successo e permette di mettere in coda una sequenza di impulsi. Gli avversari già storditi
restano a terra, possono essere perquisiti e non tornano a combattere nella stessa partita.

## Le cinque missioni originali

Anno 2161. Il reattore a fusione del **Rifugio 33** si sta spegnendo. Il Sovrintendente ti manda
nelle Terre Bruciate per trovare una cella sperimentale nella **Base Delta-9**.

- **Il cuore del Rifugio** — raggiungi Ferrocittà, recupera la tessera di Teschio oppure viola
  il terminale del cancello, attraversa Delta-9 e riporta la cella di fusione al reattore.
- **Infestazione** — libera la Stazione Atomo dai radscarafaggi e torna da Tano.
- **Medicine per Ferrocittà** — recupera i rifornimenti nell'ospedale delle rovine per la clinica.
- **La bestia della gola** — affronta lo Squarciatore e porta l'artiglio a Jack Falco come prova.
- **Segnale nel deserto** — segui la radio fino alla Torre Radio, trova il messaggio e la chiave
  dell'esploratore e raggiungi il suo deposito vicino al Cratere Verde.

Le missioni avanzano con la neutralizzazione degli avversari e il recupero del bottino.
Il finale dipende dalle scelte, dalle missioni completate e dal karma.

## Multigiocatore e telefono

Premi **Gioca con un amico** nella schermata iniziale e condividi il link della stanza.
Chi apre lo stesso link entra nella stessa stanza. Il menu **Multigiocatore** permette di cambiare
stanza e invitare altri amici. Ogni giocatore ha la propria campagna; nella stanza si vedono
avatar, movimenti e impulsi laser, si chatta e ci si raggiunge dalla mappa del Pip-Boy.

Il collegamento usa WebRTC diretto e broker MQTT pubblici con messaggi cifrati per stanza.
Con `?rete=locale` puoi collegare schede dello stesso browser; `?rete=off` disattiva la rete.

Sul telefono gioca **in orizzontale**: pollice sinistro per muoverti, dito a destra per mirare,
pulsanti per sparare, ricaricare, cambiare arma, usare S.P.A.T., interagire e aprire il Pip-Boy.
Su iPhone puoi usare **Condividi → Aggiungi alla schermata Home** per l'avvio a schermo intero.
I salvataggi restano nel browser del dispositivo.

## Comandi

| Tasto | Azione |
|---|---|
| **W A S D** | Muoversi |
| **Mouse** | Mirare e guardarsi intorno |
| **Clic sinistro** | Sparare un impulso laser / usare l'arma |
| **Clic destro** | Mirare con il mirino |
| **R** | Ricaricare le celle energetiche |
| **1–9 / rotella** | Cambiare arma |
| **E** | Parlare, interagire, raccogliere, perquisire |
| **Q** | S.P.A.T. |
| **TAB** | Pip-Boy: stato, inventario, missioni, mappa, radio |
| **Shift** | Correre |
| **C / Ctrl** | Accovacciarsi; nel browser usa **C** |
| **Spazio** | Saltare |
| **G** | Lanciare un impulso EMP |
| **H** | Usare uno stimpak |
| **V** | Prima / terza persona |
| **L** | Luce del Pip-Boy |
| **F5 / F9** | Salvataggio / caricamento rapido |
| **F11** | Schermo intero |
| **ESC** | Pausa |
| **T / Invio** | Chat nel gioco online |
| **Z** | Saluta gli altri giocatori |
| **P** | Mostra i giocatori della stanza |

Nello **S.P.A.T.**: **A/D** cambia bersaglio, **W/S** cambia punto da colpire,
**E** o clic aggiunge un impulso, **Invio** esegue la sequenza, **Q** o **ESC** esce.

## Compilare dai sorgenti

C++17 e [raylib 5.5](https://www.raylib.com/), scaricata automaticamente da CMake.

**Windows (Visual Studio 2022 + CMake):** esegui `build_windows.bat`, oppure:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```

L'ultimo eseguibile compilato è disponibile tra gli artefatti del
[workflow Build](https://github.com/Br1-data/fallout/actions/workflows/build.yml).

**Linux:**

```sh
sudo apt install build-essential cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/Dustfall
```

**Windows da Linux (MinGW-w64):** `./tools/build_windows_cross.sh`, con il pacchetto `mingw-w64`.

**WebAssembly:** attiva [Emscripten](https://emscripten.org/) 3.1.74 con
`source <emsdk>/emsdk_env.sh`, poi esegui `./tools/build_web.sh`. Il sito è in `build-web/site`.
Per servirlo localmente: `python3 -m http.server 8000 --directory build-web/site`.
`?touch=1` forza i comandi touch; `?mqtt=ws://...` e `?relay=wss://...` selezionano broker propri.

GitHub Actions compila Windows e Linux, esegue i test, costruisce la versione web e pubblica
il ramo `main` su Pages. Per Pages seleziona **Settings → Pages → Source: GitHub Actions**.

### Test automatico

`Dustfall --selftest` esegue la campagna originale e verifica combattimenti, bottino, missioni,
salvataggi e finale. La versione web usa lo stesso test con
`node tools/web_selftest.js build-web/site` (richiede Playwright).
Gli avversari neutralizzati devono restare storditi anche dopo attesa, salvataggio e caricamento.

Per uno screenshot: `Dustfall --newgame --zone 0 --pos 0 60 --yaw 180 --time 15 --shot schermata.png`.

## Struttura

- `src/main.cpp`: ciclo di gioco, transizioni e selftest.
- `src/entities.cpp`, `src/player.cpp`, `src/combat.cpp`: IA, armi laser, S.P.A.T. ed effetti.
- `src/world_build.cpp`, `src/world.cpp`, `src/render.cpp`: mondo, collisioni e rendering.
- `src/quests.cpp`, `src/pipboy.cpp`, `src/menus.cpp`: missioni, inventario, dialoghi e menu.
- `src/save.cpp`, `src/platform.cpp`, `src/net.cpp`: salvataggi, piattaforme e multigiocatore.
- `web/`: pagina, rete peer-to-peer e comandi touch.

## Crediti

- [raylib](https://github.com/raysan5/raylib), licenza zlib/libpng.
- [Emscripten](https://emscripten.org/) per WebAssembly.
- [Trystero](https://github.com/dmotz/trystero) e [@noble/secp256k1](https://github.com/paulmillr/noble-secp256k1), licenza MIT.
- Font Share Tech Mono e VT323, licenza SIL Open Font License (in `assets/fonts`).

*Dustfall* è un progetto amatoriale ispirato a *Fallout*, senza affiliazione con Bethesda o Interplay.
