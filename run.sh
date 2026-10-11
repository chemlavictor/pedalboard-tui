#!/usr/bin/env bash
# ============================================================
#  TDP - LANCER sur Linux (macOS aussi)
#  - Si build/tdp n'existe pas : build d'abord.
#  - Puis démarre la TUI.
# ============================================================
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

if [ ! -x build/tdp ]; then
  echo "[run.sh] build/tdp introuvable -> compilation..."
  make
fi

exec ./build/tdp
