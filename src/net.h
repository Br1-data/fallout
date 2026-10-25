// Multigiocatore: gli altri viandanti nelle Terre Bruciate (rete peer-to-peer nel browser).
// Ogni giocatore ha il proprio mondo e le proprie missioni; si vedono gli altri giocatori
// della stessa stanza, si chatta, ci si saluta e ci si raggiunge dalla mappa del Pip-Boy.
// Nella versione desktop la rete non c'è e queste funzioni non fanno nulla.
#pragma once
#include "game.h"

struct RemotePlayer {
    std::string id, name = "Viandante", where, say;
    int level = 1, outfit = OF_VAULT, zone = -1, weaponModel = WM_FIST;
    bool female = false, inGame = false, crouch = false, hasState = false, announced = false;
    Vector3 netPos{}, pos{};
    float netYaw = 0, yaw = 0, speed = 0, anim = 0, crouchT = 0;
    int flashN = -1, waveN = -1;
    float flashT = 0, waveT = 0, sayT = 0;
    Look look;
};

void NetInit();
void NetShutdown();
void NetUpdate(float dt);            // invia lo stato, riceve gli eventi, interpola gli avatar
bool NetAvailable();                 // la rete esiste (solo nel browser)
int NetStatus();                     // 0 disattivata, 1 connessione, 2 in rete
const std::vector<RemotePlayer>& NetPlayers();

void NetDraw3D(Camera3D cam);        // avatar degli altri giocatori
void NetDrawHud();                   // nomi, chat, stato della rete
bool NetPlayKeys();                  // tasti del multigiocatore durante il gioco (true = stato cambiato)
float NetLocalWave();                // animazione del saluto del giocatore locale

void ChatUpdate();                   // GS_CHAT
void OnlineOpen(GameState back);     // GS_ONLINE: stanza, invito, elenco dei giocatori
void OnlineUpdate();
void OnlineDraw();

// mappa del Pip-Boy: segnalini degli altri giocatori e viaggio rapido per raggiungerli
void NetMapDraw(const std::function<Vector2(Vector3)>& toMap, std::string& hover);
int NetMapClick(const std::function<Vector2(Vector3)>& toMap); // giocatore cliccato, -1 nessuno
