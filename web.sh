#!/bin/bash
# Lance le serveur web TDP.
# Usage : ./web.sh [port]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-8080}"

# Vérifie que le binaire TDP existe.
if [ ! -f "$SCRIPT_DIR/build/tdp" ]; then
  echo "Binaire TDP introuvable. Lance 'make' d'abord."
  exit 1
fi

echo "Démarrage du serveur web TDP sur http://localhost:$PORT"
python3 "$SCRIPT_DIR/web/server.py" "$PORT"
