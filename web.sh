#!/bin/bash
# Lance le serveur web TPD.
# Usage : ./web.sh [port]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-8080}"

# Vérifie que le binaire TPD existe.
if [ ! -f "$SCRIPT_DIR/build/tdp" ]; then
  echo "Binaire TPD introuvable. Lance 'make' d'abord."
  exit 1
fi

echo "Démarrage du serveur web TPD sur http://localhost:$PORT"
python3 "$SCRIPT_DIR/web/server.py" "$PORT"
