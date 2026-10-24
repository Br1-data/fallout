// Differenze tra la versione desktop e quella web (browser, WebAssembly).
#pragma once

#include "raylib.h"
#include <string>

#if defined(__EMSCRIPTEN__)
#define DUSTFALL_WEB 1
#else
#define DUSTFALL_WEB 0
#endif

void PlatformInit();
void PlatformFrame();               // da chiamare all'inizio di ogni fotogramma
std::string DataDir();              // cartella di salvataggi e impostazioni (con '/' finale)
void PlatformSyncStorage();         // web: copia i salvataggi in IndexedDB
Vector2 LookDelta();                // movimento del mouse in questo fotogramma (per guardarsi intorno)
bool PlatformPointerLocked();
void PlatformSetFullscreen(bool on);
bool PlatformIsFullscreen();
void PlatformStatus(const char* text); // web: testo della schermata di caricamento
void PlatformReady();               // web: il gioco è pronto, nasconde il caricamento
const char* GlslHeader();           // intestazione degli shader (GLSL 330 o GLSL ES 300)

// telefoni e tablet (web/touch.js)
bool PlatformTouch();                // comandi touch attivi
enum TouchMode { TM_NONE, TM_MENU, TM_PLAY, TM_SCAN, TM_LOCKPICK, TM_PIPBOY };
void PlatformSetTouchMode(int mode); // quali comandi mostrare
// finestra per scrivere un testo (tastiera del telefono): la risposta arriva in un fotogramma successivo
void PlatformAskText(const char* kind, const char* title, const std::string& initial, int maxLen);
bool PlatformTakeText(const char* kind, std::string& out); // true se è arrivata una risposta (vuota = annullato)
