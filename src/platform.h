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
