# Pedalboard TUI — Terminal Distortion Pedal (TDP)

Pédale de distorsion numérique pilotée entièrement au clavier, avec une interface TUI (ncurses) et un moteur audio temps réel (PortAudio).

## C'est quoi ?

Une petite application C11 qui :

- Capture le son de ta carte audio via **PortAudio** (mono, 44.1 ou 48 kHz, buffer 128–256, latence ≤ 10 ms).
- Applique un DSP de distorsion : `y = tanh(G·x)` puis master volume, dans un thread temps réel.
- Affiche une interface textuelle (ncurses) avec jauges Gain/Master, VU-mètres RMS+peak d'entrée/sortie, et des toggles ON/BYPASS et MUTE.
- Se pilote entièrement au clavier (voir « Contrôles »).

**Scope V1** : waveform unique (tanh), mono, Gain + Master uniquement (pas de tone, pas de boost).

## Comment ça marche (architecture)

Deux threads communiquent via des variables `stdatomic` (lock-free) :

```
┌──────────────────────────┐      stdatomic (params, vu)      ┌──────────────────────────┐
│  Thread UI (main)        │ ─────────────────────────────────▶│  Thread Audio (RT)       │
│  ncurses + clavier       │ ◀───────────────────────────────── │  PortAudio callback      │
│  ~30-60 fps              │         VU RMS + peak             │  tanh + master + VU      │
└──────────────────────────┘                                   └──────────────────────────┘
```

- **Thread Audio** : jamais bloqué, aucune allocation, aucun I/O. Ne lit que des atomics. Il calcule aussi les VU (RMS + peak-hold 500 ms).
- **Thread UI** : boucle ncurses, `getch` non bloquant, update écran.
- **Bypass** (ESPACE) : atomic `bool` ; le signal passe en raw.
- **Mute** (M) : atomic `bool` distinct ; la sortie est forcée à 0 mais le DSP continue (VU INPUT actifs, VU OUTPUT à -∞).

## Installation

Prérequis (Linux) :

```bash
# Debian/Ubuntu
sudo apt install build-essential libportaudio2 portaudio19-dev libncurses-dev

# Fedora
sudo dnf install gcc portaudio-devel ncurses-devel

# macOS (Homebrew)
brew install portaudio ncurses
```

### Build & Run

```bash
make            # build/tdp
make run        # démarre et prend l'audio par défaut
make clean      # nettoie
make test       # tests unitaires DSP (obligatoires, sans carte son)
make check      # tests sous Valgrind (fuites / accès invalides), sans carte son
make check-app  # app réelle sous Valgrind (optionnel, carte son + TTY requis)
```

Pour `make check` / `make check-app`, installe Valgrind : `sudo apt install valgrind`.

### Windows (build compilé — `tdp.exe`)

Version native Windows de la pédale : **sans ncurses**, l'UI est rendue dans la console Windows (blocs Unicode `█▓░`, couleurs, titre de fenêtre). Le cœur DSP/params/audio est réutilisé à l'identique de la V1.

**Option 1 — MSYS2 (recommandé)**

```bash
# installateur MSYS2, puis dans le shell MSYS2 :
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-portaudio
# dans le repo :
make dist        # compile tdp.exe et assemble dist/ (tdp.exe + portaudio.dll + README)
# ou juste :
make windows     # -> build/tdp.exe
```

**Option 2 — `build_windows.bat` (secours MinGW direct)**

Prérequis : `gcc` (MinGW) + PortAudio (headers/lib) dans le `PATH`. Puis :

```bat
build_windows.bat   # -> dist\tdp.exe + dist\portaudio.dll + dist\README.md
```

**Lancer**

```bat
dist\tdp.exe
```

> Si `portaudio.dll` n'est pas à côté de `tdp.exe`, le lancement affiche un message d'erreur lisible (pas de crash). Les contrôles clavier et l'affichage sont identiques à la V1.

### Lancer sous Windows (WSL2)

La V1 est native Linux (ncurses + PortAudio). Le chemin le plus simple sur une machine Windows est de la faire tourner dans **WSL2** (Ubuntu), pas de porter le code.

```bash
# 1. WSL2 (depuis PowerShell, une fois)
wsl --install -d Ubuntu
```

Puis, **dans WSL2** :

```bash
sudo apt update && sudo apt install -y build-essential portaudio19-dev libncurses-dev valgrind
git clone <repo> && cd pedalboard-tui
make && make test && make run
```

**Routage audio Windows → WSL2** (pour entendre/voir le son de la guitare) :
- Si ta carte son apparaît dans WSL2 (`aplay -l` ou `hp -l`), PortAudio/ALSA la voit directement → c'est tout.
- Sinon, l'audio WSL2 passe parfois par un backend `PulseAudio`/`pipewire` ; installe-le dans WSL2 : `sudo apt install -y pulseaudio` (ou `pipewire`), et assure-toi que le périphérique d'entrée (micro/jack guitare) et de sortie (casque/HP) sont visibles. Utilise idéalement un **casque** pour éviter le larsen (feedback) si la sortie repasse dans l'entrée.
- Le pied de page de la TUI te dit si PortAudio est `OK` ou `ERREUR` : `ERREUR` = aucune carte détectée dans WSL2, vérifie le branchement + le backend audio.

> Le code n'est **pas** modifié pour WSL2 : c'est la même V1 Ubuntu. Le port natif Windows (`.exe`) et la version navigateur (WASM) restent des pistes V2, hors périmètre ici.

### Dépendances

| Biblio | Rôle |
|--------|------|
| PortAudio ≥ 19.7 | capture/lecture audio temps réel |
| Ncurses ≥ 6.0 | rendu TUI et input clavier |
| libpthread | threads (UI vs Audio) |
| GCC ≥ 9 ou Clang ≥ 11 | C11 + atomics |

## Contrôles clavier

| Touche | Action |
|--------|--------|
| `ESPACE` | Toggle Bypass / ON (signal passe en raw en mode BYPASS) |
| `M` | Toggle Mute global (sortie forcée à 0, DSP continue) |
| `↑` / `↓` | Sélectionner le paramètre (Gain ↔ Master) |
| `←` / `→` | Diminuer / augmenter le paramètre sélectionné (pas de 5 %) |
| `R` | Reset aux défauts (Gain = 0.5, Master = 0.75) |
| `q` ou `CTRL-C` | Quitter proprement (stop audio + endwin) |

**Valeurs par défaut** : Gain = **0.5** (50 %), Master = **0.75** (75 %).

**Pied de page** de l'UI affiche : fréquence d'échantillonnage (Hz), taille du buffer, et statut PortAudio (OK/ERREUR).

## Structure du projet

> ⚠️ Cette section doit **toujours** refléter l'état réel du dépôt. À mettre à jour à chaque ajout/suppression de dossier ou de fichier.

```
pedalboard-tui/
├── README.md          ← ce fichier (gabarit « toujours à jour »)
├── LICENSE
├── Makefile           ← build Linux + cible Windows (make windows / make dist)
├── build_windows.bat  ← build Windows secours (MinGW direct) -> dist/
├── specs/
│   └── specs.md       ← spécifications techniques (DSP, TUI, architecture)
├── src/               ← code C (un fichier par responsabilité)
│   ├── main.c         ← boucle UI commune (clavier + rendu via ui_*), #ifdef _WIN32
│   ├── audio.c        ← thread audio : callback PortAudio (contraintes RT)
│   ├── dsp.c          ← traitement du signal (tanh + master + VU), C pur
│   ├── params.c       ← instance des atomiques partagées + params_init()
│   ├── ui_ncurses.c   ← impl UI Linux (ncurses)
│   └── ui_windows.c   ← impl UI Windows (console native, sans ncurses)
├── include/           ← headers publics (un par module de src/)
│   ├── params.h       ← struct tdp_params (atomiques partagées)
│   ├── dsp.h          ← prototype dsp_process + struct dsp_vu
│   ├── audio.h        ← prototypes audio_start / audio_stop
│   └── ui.h           ← interface UI commune (ui_*)
└── tests/
    └── test_dsp.c     ← tests unitaires DSP (make test / make check)
```

> La section `build/` (artefacts) et `dist/` (binaire Windows) existent après build mais ne sont jamais committés.

### Légende des dossiers

- **`specs/`** — documentation de conception. Uniquement des `.md`.
- **`.specs/`** — specs de développement (brainstorm/requirements/design/tasks).
- **`src/`** — tout le code C. Un fichier par responsabilité : `main.c` (UI), `audio.c` (PortAudio), `dsp.c` (DSP + VU), `params.c` (atomiques).
- **`include/`** — headers publics (`.h`). Un header par module de `src/`.
- **`tests/`** — tests unitaires en C (obligatoires).
- **`build/`** — artefacts de compilation (jamais commit).
- **racine** — `Makefile`, `README.md`, `LICENSE`. (Pas de CMake en V1.)

## Contribuer / Roadmap

Voir [`specs/specs.md`](specs/specs.md) section « Roadmap ».

## Licence

Voir [LICENSE](LICENSE).
