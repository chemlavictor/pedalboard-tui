#!/usr/bin/env bash
# ============================================================
#  TPD - Installation du compilateur Windows (MSYS2) + build
#  A lancer DEPUIS le shell "MSYS2 MINGW64".
#
#  Ce script :
#   1. Installe gcc (MinGW64) + PortAudio via pacman
#   2. Verifie que gcc est present
#   3. Compile tdp.exe et remplit dist/ (tdp.exe + portaudio.dll + README)
#
#  Si MSYS2 n'est PAS encore installe :
#    - telecharge-le sur https://www.msys2.org/
#    - installe-le par defaut (C:\msys64)
#    - OUVRE le shell "MSYS2 MINGW64" (menu demarrer)
#    - puis :  cd /c/chemla/.../pedalboard-tui && ./install_windows.sh
# ============================================================
set -e

echo "=================================================="
echo " TPD - Installation Windows (MSYS2 MINGW64)"
echo "=================================================="

# 0. On doit etre dans le bon shell (MINGW64), sinon gcc n'existera pas.
if [ -z "$(command -v pacman 2>/dev/null)" ]; then
  echo "[ERREUR] 'pacman' introuvable."
  echo "  Tu n'es pas dans le shell MSYS2."
  echo "  Ouvre le shell 'MSYS2 MINGW64' (menu demarrer) et relance ce script."
  exit 1
fi

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
echo "[info] repo : $ROOT"

# 1. Installer gcc + PortAudio (idempotent : relançable sans risque).
echo ""
echo "[1/3] Installation de gcc (MinGW64) + PortAudio via pacman..."
pacman -S --noconfirm --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-portaudio

# 2. Verifier gcc.
echo ""
echo "[2/3] Verification de gcc..."
if ! command -v gcc >/dev/null 2>&1; then
  echo "[ERREUR] gcc toujours introuvable apres installation."
  echo "  Referme et rouvre le shell 'MSYS2 MINGW64', puis relance ce script."
  exit 1
fi
gcc --version | head -1
# Verifier la presence de la DLL PortAudio (copiee dans dist/).
PA_DLL=""
for d in /mingw64/bin /mingw32/bin; do
  for n in libportaudio-0.dll portaudio.dll; do
    if [ -f "$d/$n" ]; then PA_DLL="$d/$n"; break 2; fi
  done
done
if [ -z "$PA_DLL" ]; then
  echo "[AVERTISSEMENT] DLL PortAudio introuvable dans /mingw64/bin."
  echo "  tdp.exe ne demarrera pas sans elle. Reinstalle :"
  echo "    pacman -S --noconfirm mingw-w64-x86_64-portaudio"
else
  echo "[info] DLL PortAudio : $PA_DLL"
fi

# 3. Build -> dist/
echo ""
echo "[3/3] Compilation de tdp.exe -> dist/ ..."
make dist

echo ""
echo "=================================================="
echo " OK ! dist/ contient : tdp.exe + portaudio.dll + README.md"
echo " Lancer :  dist/tdp.exe"
echo "=================================================="
