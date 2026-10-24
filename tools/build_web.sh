#!/bin/sh
# Compila la versione web (WebAssembly) in build-web/ e prepara il sito in build-web/site/.
# Richiede Emscripten (emsdk) attivo nella shell: source <emsdk>/emsdk_env.sh
# Per provarlo: cd build-web/site && python3 -m http.server 8000, poi http://localhost:8000
set -e
cd "$(dirname "$0")/.."
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --parallel
rm -rf build-web/site
mkdir -p build-web/site
for f in index.html dustfall.js dustfall.wasm net.js touch.js trystero.min.js manifest.webmanifest \
         icon-192.png icon-512.png apple-touch-icon.png VT323-Regular.ttf; do
    cp "build-web/$f" build-web/site/
done
echo "Sito pronto in build-web/site"
