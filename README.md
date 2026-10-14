# Pedalboard TUI — Terminal Pedal Board (TPD) — **CLI**

[![Build & Release](https://github.com/chemlavictor/pedalboard-tui/actions/workflows/build.yml/badge.svg)](https://github.com/chemlavictor/pedalboard-tui/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/chemlavictor/pedalboard-tui?label=release)](https://github.com/chemlavictor/pedalboard-tui/releases/latest)

## ⬇️ Télécharger le binaire

Dernière release : **[chemlavictor/pedalboard-tui/releases/latest](https://github.com/chemlavictor/pedalboard-tui/releases/latest)**

**Linux** (x86_64) :
```bash
# 1. Dépendance runtime (une fois)
apt install -y libportaudio2
# 2. Télécharger + dézipper la dernière release
curl -LO https://github.com/chemlavictor/pedalboard-tui/releases/latest/download/tdp-linux.tar.gz
tar xzf tdp-linux.tar.gz
# 3. Lancer
./tdp
```

**Windows** (x86_64) :
```powershell
# 1. Télécharger la dernière release (PowerShell)
Invoke-WebRequest -Uri "https://github.com/chemlavictor/pedalboard-tui/releases/latest/download/tdp-windows.zip" -OutFile tdp-windows.zip
# 2. Dézipper
Expand-Archive tdp-windows.zip -DestinationPath .\tdp
# 3. Lancer
.\tdp\tdp.exe
```

> Pas de MSYS2 ni de compiler requis. Le binaire affiche sa version au démarrage : `[build YYYY.MM.DD-hash]`.

### Versioning

Les releases sont versionnées par **date** : `YYYY.MM.DD.XX` (date + séquence du jour). Exemple : `2026.10.14.01` = 1ère release du 14 octobre 2026. Chaque tag déclenche le build CI et publie une release GitHub avec les binaires Linux + Windows.

### Releases

| Version | Date | Contenu |
|---------|------|---------|
| [`2026.10.14.01`](https://github.com/chemlavictor/pedalboard-tui/releases/tag/2026.10.14.01) | 2026-10-14 | Première release binaire : TPD CLI (Linux + Windows), 5 pédales, accordeur, enregistrement WAV, mode serveur TCP, page web. |

**Supporte Windows ET Linux avec le même code.**

## Démarrage rapide

### Option 1 — Binaire précompilé (sans compiler)

Chaque tag `v*` publie une **release GitHub** avec les binaires prêts à l'emploi :

| Plateforme | Archive | Contenu |
|------------|---------|---------|
| Linux (x86_64) | `tdp-linux.tar.gz` | `tdp` (binaire ELF, lien dynamique `libportaudio.so`) |
| Windows (x86_64) | `tdp-windows.zip` | `tdp.exe` + `portaudio.dll` + `README.md` |

**Linux** :
```bash
apt install -y libportaudio2   # dépendance runtime (une fois)
curl -LO <url-release>/tdp-linux.tar.gz
tar xzf tdp-linux.tar.gz
./tdp                          # jouer / enregistrer
```

**Windows** :
```
1. Télécharger tdp-windows.zip depuis la release, le dézipper dans un dossier.
2. Double-cliquer tdp.exe (ou l'ouvrir dans un terminal) :
   cd C:\chemin\vers\le\dossier
   tdp.exe
```

> Pas de MSYS2 ni de compiler requis pour l'option 1. Le binaire est celui du tag courant ; `help` affiche le hash git du build.

### Option 2 — Compiler depuis le source

### Linux / macOS (ou WSL)
```bash
# 1. Dépendances (une fois)
sudo apt install -y build-essential portaudio19-dev valgrind git
# (ncurses n'est plus nécessaire)

# 2. Compile + lance
make && make run
# ou : ./build/tdp            (mode interactif)
# ou : ./build/tdp script.txt (mode script, ligne par ligne)
```

### Windows (natif, sans WSL)

**1. Installer MSYS2** (une fois) : https://www.msys2.org/ → `C:\msys64`.

**2. Ouvre le shell « MSYS2 MINGW64 »** (menu démarrer) — PAS « MSYS2 » normal.

**3. Installe les dépendances** (une fois) :
```bash
pacman -S --noconfirm \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-portaudio \
  make \
  git \
  unzip curl
```
> Si tu es en **UCRT64** (pas MINGW64) : utilise `mingw-w64-ucrt-x86_64-gcc` et `mingw-w64-ucrt-x86_64-portaudio`.

**4. Compile** :
```bash
cd /c/Users/utilisateur/Documents/pedalboard-tui
make dist          # compile tdp.exe + copie portaudio.dll -> dist/
```

**5. Lancer** :
```bash
dist/tdp.exe
```
Ou double-clique **`run.bat`**.

> **Version** : `help` affiche le hash git du build (`v<hash>`). Si le shell ne part pas, vérifie que `portaudio.dll` est dans `dist/` (côte à côte de `tdp.exe`).

## C'est quoi ?

Une application C11 à **2 threads** qui :
- Capture le son via **PortAudio** (mono, 48 kHz, buffer 256), applique une **chaîne de pédales en série** dans un thread temps réel.
- S'exécute dans un **shell de commandes** : `tdp> load guitar.tdp`, `tdp> start`, `tdp> record`, etc.
- **Enregistre** le signal post-pédales en **WAV horodaté** (`record` + Entrée).
- Se pilote entièrement au clavier, portable Windows + Linux.

## Commandes

| Commande | Effet |
|----------|-------|
| `in` / `out` | Lister les entrées / sorties audio |
| `in N` / `out N` | Sélectionner le périphérique N |
| `load <fichier>` | Charger une chaîne de pédales depuis un `.tdp` |
| `chain` | Afficher la chaîne chargée |
| `ls` | Alias de `chain` |
| `start` | Démarrer le flux audio (pédales appliquées) |
| `stop` | Arrêter le flux audio |
| `record` | Commencer l'enregistrement (post-pédales) |
| `<Entrée>` | Arrêter l'enregistrement + sauvegarde `recordings/<date>.wav` |
| `status` | Snapshot du niveau (RMS/peak in/out) + état |
| `s` | Alias de `status` |
| `watch` | Affichage continu du niveau (on/off) |
| `pedals` | Lister les pédales disponibles |
| `info <nom>` | Détails d'une pédale (params, min/max/défaut) |
| `reset` | Reset les params de la chaîne aux défauts |
| `help` | Vue d'ensemble (commandes + pédales + exemple) |
| `quit` | Quitter |

## Confort clavier

| Touche | Action |
|--------|--------|
| `Tab` | Auto-complétion : commandes (1er mot), pédales (`info <Tab>`), fichiers `.tdp` (`load <Tab>`) |
| `↑` / `↓` | Historique des commandes (32 dernières) |
| `Ctrl+C` | Quitter (équivalent `quit`) |
| `Backspace` | Effacer |
| `<Entrée>` | Exécuter la commande (ou stopper l'enregistrement) |

> Le shell est du texte stdout/stdin simple (pas de ncurses), donc portable Windows + Linux sans dépendance.

## Pédales (V1)

| Pédale | Paramètre | Défaut | Rôle |
|--------|-----------|--------|------|
| `booster` | `level` (0..2) | 1.5 | Gain linéaire propre (type TS9 level) |
| `distortion` | `gain` (0..1) | 0.5 | Distorsion `tanh(G·x)` |
| `noise_gate` | `threshold` (dB) | -45 | Coupe le bruit de fond si RMS < seuil |
| `volume` | `level` (0..1) | 0.75 | Master volume de sortie |
| `mute` | — | off | Silenced total |

## Fichier de pédales (`.tdp`)

Une pédale par ligne (l'ordre = l'ordre en série), `param=valeur`, `#` = commentaire.

Exemple `tdp/guitar_zombies_metal.tdp` (son métal, inclus dans le repo) :
```
booster level=2.0
distortion gain=0.6
noise_gate threshold=-40
volume level=0.8
```

Charge : `tdp> load tdp/guitar_zombies_metal.tdp` (Tab complète les `.tdp` du dossier courant ; lance `tdp` depuis `tdp/` ou passe le chemin relatif).

### Presets inclus (dossier `tdp/`)

| Fichier | Son |
|---------|-----|
| `tdp/son_metal.tdp` | Metal / chug (gain élevé, tight) |
| `tdp/son_rock.tdp` | Rock / crunch (gain moyen) |
| `tdp/son_clean.tdp` | Clean (pas de distorsion) |
| `tdp/son_jazz.tdp` | Jazz (gain faible, doux) |
| `tdp/son_blues.tdp` | Blues (legato, gate) |
| `tdp/clean_boost.tdp` | Clean boost (gain à fond, pas de disto) |
| `tdp/guitar_zombies_metal.tdp` | Metal « Zombies » (booster + high-gain + gate) |

> Un **skill** `.pi/skills/tdp-presets/` aide un LLM à générer un `.tdp` à partir d'une description de son (ex. « son metal »).

## Mode script (automatisation)

`./tdp.exe script.txt` exécute les commandes ligne par ligne, sans prompt, puis se termine.
Utile pour les tests batch / CI.

## Ajouter une nouvelle pédale (avec un LLM)

Le catalogue `src/pedals.c` est la **source de vérité** :
1. Donne `src/pedals.c` à un LLM, demande-lui une nouvelle pédale (param + def + fonction DSP).
2. Colle le bloc généré dans `src/pedals.c` (tableau de params + def + fonction `process`).
3. `make` → la pédale est dispo (`pedals`, `info`, `load`).

Recompile (pas de plugin dynamique) — simple et fiable.

## Build & tests

```bash
make            # build/tdp
make run        # ./build/tdp
make test       # tests unitaires (DSP + catalogue + parser + WAV), sans carte son
make check      # tests sous Valgrind (0 fuite), sans carte son
make dist       # Windows : dist/tdp.exe + portaudio.dll
make clean      # nettoie
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Thread main (CLI)                                          │
│  Boucle : lire commande (stdin/script) -> interpreter       │
│    in/out, load, start/stop, record, status/watch, pedals  │
│    Entrée (vide) -> stop record + encoder WAV               │
└───────────▲─────────────────────────────────────────────────┘
            │ stdatomic (running, recording, vu, chain_idx)
┌───────────▼─────────────────────────────────────────────────┐
│  Thread audio (RT) — callback PortAudio                     │
│  input -> [pedale1 -> pedale2 -> ... -> pedaleN] -> output  │
│  VU in/out -> atomiques                                     │
│  si recording : ecrire dans ring buffer (statique, ~60 s)   │
│  CONTRAINTES : ni malloc, ni I/O, ni locks.                 │
└─────────────────────────────────────────────────────────────┘
```

- **Thread Audio** : ne fait ni `malloc`/`free`, ni I/O, ni locks. La chaîne de pédales est dans une mémoire statique (index atomique). Le ring buffer d'enregistrement est pré-alloué.
- **Thread main** : lit les commandes, modifie les atomiques, encode le WAV à l'arrêt du record.

## Dépendances

| Dépendance | Rôle | Linux (apt) | Windows (pacman) |
|------------|------|-------------|------------------|
| GCC / Clang ≥ 9 | compiler C11 | `build-essential` | `mingw-w64-x86_64-gcc` |
| PortAudio ≥ 19.7 | audio temps réel | `portaudio19-dev` | `mingw-w64-x86_64-portaudio` |
| make | build | `make` | `make` |
| git | hash version | `git` | `git` |
| valgrind (optionnel) | check mémoire | `valgrind` | — |

> ncurses n'est **plus** nécessaire (le shell est du texte stdout/stdin simple).

## Structure du projet

```
pedalboard-tui/
├── README.md            ← ce fichier
├── Makefile             ← build Linux + Windows (make / make dist)
├── run.sh / run.bat     ← lanceurs
├── include/
│   ├── pedals.h         ← catalogue de pédales (def, params, chain_t)
│   ├── shell.h          ← interface du shell (shell_run, parse_tdp_line)
│   ├── audio.h          ← thread audio (audio_shared, rec_ring)
│   ├── wav.h            ← encodeur WAV + nom horodaté
│   ├── dsp.h            ← (legacy V1, encore compilé)
│   └── params.h         ← (legacy V1, encore compilé)
├── src/
│   ├── main.c           ← point d'entrée (./tdp [script])
│   ├── shell.c          ← shell de commandes (boucle + dispatch)
│   ├── pedals.c         ← catalogue + fonctions DSP des 5 pédales
│   ├── audio.c          ← thread audio RT (chaîne + VU + ring buffer)
│   ├── wav.c            ← encodeur WAV PCM 16-bit + nom horodaté
│   ├── dsp.c            ← (legacy V1)
│   └── params.c         ← (legacy V1)
├── tests/
│   ├── test_dsp.c       ← tests DSP + catalogue + chaîne
│   ├── test_tdp.c       ← tests parser .tdp
│   └── test_wav.c       ← tests encodeur WAV
├── tdp/                 ← presets de pédales (.tdp)
├── specs/               ← documentation de conception
└── recordings/          ← (créé à l'usage) WAV horodatés
```

## Contribuer / Roadmap

Voir [`specs/specs.md`](specs/specs.md). Pistes V2+ : ampli high-gain (simu), wah, delay, reverb, plugin dynamique, multi-pistes.

## Licence

Voir [LICENSE](LICENSE).
