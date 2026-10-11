#!/usr/bin/env bash
# ============================================================
#  Télécharge portaudio.dll (Windows 64-bit) dans dist/
#  Si la DLL n'est pas trouvée localement par make dist.
#
#  Usage : ./download_portaudio_dll.sh
#  La DLL sera dans : dist/portaudio.dll
# ============================================================
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
mkdir -p dist

# Cherche d'abord localement (pacman l'a peut-être mise).
for f in /mingw64/bin/libportaudio-0.dll /mingw32/bin/libportaudio-0.dll \
         /ucrt64/bin/libportaudio-0.dll /mingw64/bin/portaudio.dll \
         /mingw32/bin/portaudio.dll /ucrt64/bin/portaudio.dll; do
  if [ -f "$f" ]; then
    cp "$f" dist/portaudio.dll
    echo "DLL copiée depuis : $f"
    exit 0
  fi
done

# Sinon : télécharge depuis un mirror fiable.
echo "DLL introuvable localement -> téléchargement..."

# PortAudio DLL (64-bit) depuis les releases GitHub.
# On utilise la DLL de la release la plus récente.
URL="https://github.com/PortAudio/PortAudio/releases/download/v19.7.0/portaudio-bin-197.0.zip"
TMP="$(mktemp -d)"

if command -v curl >/dev/null 2>&1; then
  curl -L -o "$TMP/portaudio.zip" "$URL" || { echo "Échec du téléchargement (curl)."; rm -rf "$TMP"; exit 1; }
elif command -v wget >/dev/null 2>&1; then
  wget -O "$TMP/portaudio.zip" "$URL" || { echo "Échec du téléchargement (wget)."; rm -rf "$TMP"; exit 1; }
else
  echo "Ni curl ni wget disponible. Installe : pacman -S --noconfirm curl"
  rm -rf "$TMP"
  exit 1
fi

# Extrait la DLL du zip.
if command -v unzip >/dev/null 2>&1; then
  cd "$TMP"
  unzip -o portaudio.zip
  # La DLL est généralement dans : portaudio-bin-197.0/bin/portaudio.dll
  DLL=$(find . -name "portaudio.dll" -o -name "libportaudio-0.dll" | head -1)
  if [ -n "$DLL" ]; then
    cp "$DLL" "$ROOT/dist/portaudio.dll"
    echo "DLL téléchargée et extraite : dist/portaudio.dll"
  else
    echo "DLL introuvable dans le zip."
    ls -laR "$TMP"
  fi
else
  echo "unzip introuvable. Installe : pacman -S --noconfirm unzip"
fi

rm -rf "$TMP"

# Vérifie
if [ -f "$ROOT/dist/portaudio.dll" ]; then
  echo "OK : dist/portaudio.dll est en place."
else
  echo "ÉCHEC : portaudio.dll n'est toujours pas dans dist/."
  echo "  Solution manuelle :"
  echo "  1. Télécharge https://github.com/PortAudio/PortAudio/releases"
  echo "  2. Extrait portaudio.dll"
  echo "  3. Copie-la dans dist/"
fi
