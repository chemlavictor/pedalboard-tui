# Spécifications Techniques : Pédale de Distorsion TUI en C

## 1. Vue d'ensemble du projet
- **Nom du projet :** Terminal Distortion Pedal (TDP)
- **Langage :** C (Standard C11)
- **Interface :** TUI (Terminal User Interface) gérée via `ncurses`
- **Audio :** `PortAudio` (gestion bas niveau, temps réel et ultra-basse latence)
- **Objectif :** Offrir une pédale de distorsion numérique fonctionnelle, pilotable entièrement au clavier, avec un affichage textuel en temps réel des paramètres et des niveaux audio.
- **Plateforme cible (V1) :** Linux ou macOS (le développeur choisit un des deux ; l'autre OS sera adapté plus tard).

---

## 2. Architecture Logicielle
L'application repose sur une architecture **2 threads** pour garantir l'absence de coupures ou de latence dans le son :

1. **Thread Audio (Temps Réel Critique)** — callback PortAudio :
   - Exécute le DSP (traitement du signal) à chaque bloc d'échantillons.
   - **Fréquence d'échantillonnage fixe** : 44 100 Hz **ou** 48 000 Hz (décidée par PortAudio à l'ouverture du stream, pas de hot-swap pour la V1).
   - **Taille de buffer** : 128 ou 256 échantillons → latence cible **≤ 10 ms**.
   - **Contraintes strictes du code RT** :
     - Aucune allocation mémoire (`malloc`/`free`/`new` interdits).
     - Aucun appel bloquant (pas de `printf`, pas d'I/O, pas de lock).
     - Lecture uniquement de variables `stdatomic`.

2. **Thread UI (Interface)** — boucle principale (main) :
   - Gère `ncurses` : rendu ~30–60 fps.
   - Capture clavier non bloquante (`nodelay + getch`).
   - Écrit les paramètres dans des `stdatomic` ; lit les VU-mètres via `stdatomic`.

3. **Communication inter-threads** :
   - **Variables `stdatomic`** (via `<stdatomic.h>`), lock-free.
   - Modèle : thread UI = producteur de paramètres / consommateur de VU ; thread audio = consommateur de paramètres / producteur de VU.
   - Pas de ring buffer de commandes pour la V1 (trop simple, un `atomic_bool` + `atomic_float` suffit).

### Schéma de flux
```
┌──────────────────────────┐      stdatomic (params, vu)      ┌──────────────────────────┐
│  Thread UI (main)        │ ─────────────────────────────────▶│  Thread Audio (RT)       │
│  ncurses + clavier       │ ◀───────────────────────────────── │  PortAudio callback      │
│  ~30-60 fps              │         VU peak / RMS             │  tanh + master           │
└──────────────────────────┘                                   └──────────────────────────┘
```

---

## 3. Spécifications du DSP (Effet de Distorsion)

**Chaîne de traitement (mono)** :
```
input → [Gain G] → [tanh clipping] → [Master V] → output
```

### Paramètres
| Nom | Symbole | Type | Min | Max | Défaut | Description |
|-----|---------|------|-----|-----|--------|-------------|
| Gain / Drive | `G` | `float` | 0.0 | 1.0 | **0.5** | Amplification pré-clipping |
| Master / Volume | `V` | `float` | 0.0 | 1.0 | **0.75** | Atténuation/amplification de sortie |

### Formules
- **Clipping doux (tanh)** :
  $$y = \tanh(G \cdot x)$$
  - `x` : échantillon d'entrée (mono), `y` : échantillon post-saturation.
  - La tanh borne naturellement le signal dans [-1, 1] → **pas de limiteur d'entrée nécessaire en V1**.
- **Master** : `out = V * y`.

### Décisions V1 (figées)
- **Waveform unique** : uniquement `tanh` (soft clipping). Pas de hard clip, pas de rectifier en V1.
- **Pas de tone control** (pas de filtre passe-bas).
- **Pas de post-gain / boost** distinct du Master.
- **Mono uniquement** : un seul canal traité (source guitare). PortAudio ouvre le stream en 1 canal ; si la carte impose 2 canaux, on duplique le traitement sur les 2 mais l'UI ne gère qu'un seul paramètre.
- **Sans normalisation de gain (make-up gain)** : la tanh réduit l'amplitude ; le Master compense manuellement.

### VU-mètres
- Calculés **dans le thread audio** (pas dans l'UI pour ne pas bloquer le RT).
- Métrique : **RMS sur le buffer courant**, avec **peak-hold** de 500 ms décroissant.
- Stockés dans des `atomic_float` (0.0 – 1.0) :
  - `vu_input_rms`, `vu_input_peak`
  - `vu_output_rms`, `vu_output_peak`
- Thread UI lit ces 4 atomics pour afficher les jauges.

---

## 4. Spécifications de l'Interface TUI et Contrôles Clavier

### Mise en page (exemple)
```
┌──────────────────────────────────────────────┐
│  TDP — Terminal Distortion Pedal      [ON]   │
│                                              │
│  > Gain   [=====-----]  50%                  │
│    Master [=========--]  75%                 │
│                                              │
│  INPUT   [######----------] -12.3 dB  peak   │
│  OUTPUT  [############---]  -4.1 dB  peak    │
│                                              │
│  44100 Hz · buffer 256 · PortAudio OK        │
└──────────────────────────────────────────────┘
```

### Règles d'affichage
- **Sélection visible** : préfixe `> ` devant le paramètre actif (ou surlignage de la ligne active).
- **Jauge + valeur numérique** : `[===========--]  75%` (12 cases, 10 % par case).
- **VU-mètres** : jauge de 20 cases + valeur en dB (0 dB = pleine échelle, négatif = sous pleine échelle) + marquage de crête (peak-hold).
- **En-tête** : titre + état `ON` / `BYPASS` (en surbrillance quand BYPASS).
- **Pied de page** : `fréquence d'échantillonnage Hz · buffer N · statut PortAudio` (OK / ERREUR).
- **Échelle des VU** : dB = 20·log10(rms), plafonné à -60 dB (silence).

### Raccourcis clavier
| Touche | Action |
|--------|--------|
| `ESPACE` | Toggle Bypass / ON (signal passe en raw en mode BYPASS) |
| `M` | Toggle Mute global (coupe le son instantanément, **distinct** du bypass : les paramètres restent affichés et les VU d'entrée continuent) |
| `↑` / `↓` | Cycle du paramètre sélectionné (Gain ↔ Master) |
| `←` / `→` | Diminuer / augmenter le paramètre sélectionné (pas de 0.05, soit 5 %) |
| `R` | Réinitialiser Gain et Master aux défauts (0.5 / 0.75) |
| `q` | Quitter proprement (stop PortAudio, `endwin`, sortie 0) |
| `CTRL-C` | Idem `q` (sécurité) |

### Comportement BYPASS vs MUTE
- **BYPASS** : le signal d'entrée est routé directement en sortie (contourne tout le DSP). Les VU INPUT et OUTPUT restent actifs et identiques.
- **MUTE** : la sortie est mise à 0. Les VU OUTPUT tombent à -∞ dB, les VU INPUT continuent. Le DSP continue de s'exécuter (paramètres inchangés).

---

## 5. Prérequis Techniques (Dépendances)

| Dépendance | Version min. | Rôle |
|------------|--------------|------|
| GCC ≥ 9 ou Clang ≥ 11 | — | C11 + atomics |
| PortAudio ≥ 19.7 | — | Audio in/out temps réel |
| Ncurses ≥ 6.0 | — | TUI |
| libpthread | — | Threads (UI vs Audio) |

### Installation (résumé — voir README.md)
```bash
# Debian/Ubuntu
sudo apt install build-essential libportaudio2 portaudio19-dev libncurses-dev
# Fedora
sudo dnf install gcc portaudio-devel ncurses-devel
# macOS
brew install portaudio ncurses
```

### Build
- **Système de build : Makefile** (pas de CMake en V1).
- Cible unique : `make` → `build/tdp`.
- Cibles utiles : `make clean`, `make run`, `make test` (optionnel).
- Flags : `-std=c11 -O2 -Wall -Wextra -pthread -lportaudio -lncurses -lm`.

### Tests unitaires (optionnels en V1)
- Un petit `tests/test_dsp.c` vérifiant :
  - `tanh(0) == 0`
  - `tanh(∞) → 1` (bornes)
  - pas de NaN pour des entrées extrêmes (±10, ±1e6).
- Lancement via `make test` si présent, sinon ignoré.

---

## 6. Règles de documentation (OBLIGATOIRE)

- **`README.md`** (racine du repo) doit **toujours** être à jour à chaque commit touchant la structure, l'install, les contrôles ou l'architecture.
  - Sections obligatoires : *C'est quoi ?*, *Comment ça marche*, *Installation*, *Contrôles clavier*, *Structure du projet*, *Légende des dossiers*, *Roadmap*.
  - La section « Structure du projet » est un arbre du dépôt ; à chaque ajout/suppression de fichier ou de dossier, cet arbre est mis à jour dans **le même commit**.
- **`specs/specs.md`** (ce fichier) est la source de vérité technique. Tout changement de DSP, de thread, de contrôle clavier ou de dépendance doit d'abord être documenté ici, puis reflété dans le README.
- **Changelog** : tout ajout de fonctionnalité majeur est noté dans la section « Roadmap » ci-dessous avec une date.

---

## 7. Roadmap

> Format : `- [ ] <date> — <entrée>` pour les ajouts non faits, `- [x]` pour les faits.

### Fait
- [x] 2025-07-09 — spécifications initiales (DSP tanh, 2 threads, TUI ncurses)
- [x] 2025-07-09 — ajout README.md avec section « toujours à jour »
- [x] 2025-07-09 — décisions V1 figées : tanh unique, mono, Gain+Master seulement, stdatomic, 44.1/48 kHz, buffer 128/256, Makefile, Linux/macOS, VU RMS + peak-hold, touches M (mute) et R (reset), pied de page avec sr/buffer, sélection `>` visible, valeurs par défaut Gain=0.5 / Master=0.75

### À faire (implementation)
- [ ] squelette Makefile + `src/main.c` (ncurses loop, clavier)
- [ ] `src/audio.c` (PortAudio callback, stream mono)
- [ ] `src/dsp.c` (tanh + master + VU)
- [ ] `src/params.c` + `include/params.h` (atomiques partagées)
- [ ] tests unitaires optionnels (`tests/test_dsp.c`)

### Hors périmètre V1 (possibles V2)
- [ ] hot-swap de la fréquence d'échantillonnage
- [ ] multi-waveform (hard clip, rectifier)
- [ ] tone control (filtre passe-bas)
- [ ] post-gain / boost distinct
- [ ] stéréo avec paramètres par canal
- [ ] presets sauvegardés (fichier JSON/TOML)
- [ ] CMake (si le projet grossit)
- [ ] CI (build + smoke test)
