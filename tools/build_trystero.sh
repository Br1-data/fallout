#!/bin/sh
# Rigenera web/trystero.min.js: la libreria peer-to-peer (Trystero, licenza MIT) usata dal
# multigiocatore, in un unico file senza dipendenze esterne. Richiede Node.js e npm.
set -e
VERSION=0.26.0
OUT="$(cd "$(dirname "$0")/.." && pwd)/web/trystero.min.js"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"
npm init -y >/dev/null
npm install --silent "@trystero-p2p/nostr@$VERSION" "@trystero-p2p/torrent@$VERSION" esbuild@0.25.10
cat > entry.mjs <<'JS'
import * as nostr from '@trystero-p2p/nostr'
import * as torrent from '@trystero-p2p/torrent'
window.DustfallP2P = {nostr, torrent}
JS
npx esbuild entry.mjs --bundle --minify --format=iife --target=es2020 --legal-comments=none --outfile=bundle.js
{
  echo "/*! Trystero $VERSION (strategie Nostr e BitTorrent) - https://github.com/dmotz/trystero - licenza MIT."
  echo "    Include @noble/secp256k1 (licenza MIT). Generato da tools/build_trystero.sh */"
  cat bundle.js
} > "$OUT"
echo "Scritto $OUT"
