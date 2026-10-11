# Design: TDP CLI — shell de commandes

## Overview
TDP CLI est une application C11 à deux threads qui remplace le TUI par un **shell de commandes**. Le thread main lit les commandes (stdin, interactif ou script) et pilote l'état (périphériques, chaîne de pédales, record, flux) via des variables `stdatomic`. Le thread audio (callback PortAudio, temps réel) applique la chaîne de pédales au signal et, si `record` est actif, écrit le signal post-pédales dans un ring buffer pré-alloué. À la fin de l'enregistrement, le thread main encode le ring buffer en WAV.

**Changement de paradigme** : plus de ncurses, plus de console API custom. L'UI = `printf`/`fgets` sur stdout/stdin. Portable Windows + Linux sans branchement conditionnel d'UI.

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Thread main (CLI)                                                       │
│  Boucle : lire commande (stdin) -> interpreter -> modifier atomics       │
│    in / out        : lister/choisir devices (audio_list_devices)         │
│    start / stop    : atomic running + audio_start/stop                   │
│    load <f>        : parser .tdp -> construit la chaine (array atomique) │
│    record          : atomic recording = true                             │
│    Enter (vide)    : atomic recording = false + encoder WAV (ring->fichier)│
│    status / watch  : lit vu (atomic) + affiche                           │
│    pedals / info   : lit le catalogue (pedals.c) + affiche               │
│    help / quit / reset / chain                                           │
└───────────────▲──────────────────────────────────────────────────────────┘
                │ stdatomic (params, running, recording, vu)
┌───────────────▼──────────────────────────────────────────────────────────┐
│  Thread audio (RT) — callback PortAudio                                  │
│  Pour chaque echantillon du buffer :                                     │
│    1. Lecture (input)                                                    │
│    2. Pour chaque pedale de la chaine (dans l'ordre) : pedale->process() │
│       - booster:  x *= level                                             │
│       - distortion: x = tanh(gain * x)                                   │
│       - noise_gate: if rms < threshold -> x = 0                          │
│       - volume:  x *= level                                              │
│       - mute:     x = 0                                                  │
│    3. VU (RMS + peak) in/out  -> atomics                                 │
│    4. Si recording: ecrire x dans ring buffer (wrap)                      │
│    5. Sortie (output) = x                                                │
│  CONTRAINTE: ni malloc/free, ni I/O, ni locks. Memoire pre-allouee.       │
└──────────────────────────────────────────────────────────────────────────┘
```

## Components

### Component 1: `src/shell.c` (nouveau) — le shell de commandes
- Boucle de lecture : `fgets` sur stdin (interactif) ou sur un `FILE*` (script).
- Parse de la ligne : `commande [arg1] [arg2] ...` (scission sur espaces).
- Dispatch vers les handlers (voir `Commands` ci-dessous).
- Mode interactif : affiche `tdp> ` avant chaque lecture. Mode script : pas de prompt, termine à la fin du fichier.
- **Pas de readline avancé** : une simple `fgets` suffit (pas d'historique, pas d'auto-complétion — V1 CLI).

### Component 2: `src/pedals.c` + `include/pedals.h` (nouveau) — catalogue de pédales
- Structure du catalogue :
  ```c
  typedef struct pedal_param {
    const char *name;      /* "gain", "level", "threshold" */
    const char *desc;      /* description courte */
    float minv, maxv, def; /* plage + defaut */
    const char *unit;      /* "", "dB", "%" */
  } pedal_param;

  typedef struct pedal {
    const char *name;         /* "distortion" */
    const char *desc;         /* "Distorsion tanh (type TS9)" */
    const pedal_param *params; /* array, terminee par name==NULL */
    int n_params;
    /* Applique la pedale au buffer. ctx = etat de la pedale (params). */
    void (*process)(float *buf, int n, const pedal_state *st);
  } pedal_def;

  /* La chaine chargee : array de (def + state), taille max fixe (ex. 16). */
  typedef struct chain {
    const pedal_def *defs[MAX_CHAIN];
    pedal_state state[MAX_CHAIN];
    int len;
  } chain_t;
  ```
- `pedal_def` est un tableau `const` (le catalogue). `pedal_state` contient les valeurs courantes des params (clonées au chargement).
- Fonctions DSP : `pedal_booster_process`, `pedal_distortion_process`, `pedal_noise_gate_process`, `pedal_volume_process`, `pedal_mute_process`.
- Lookup : `pedal_find(name)` → `const pedal_def*` ou NULL.
- **Source de vérité LLM** : `src/pedals.c` contient les descriptions + params + DSP ; l'utilisateur le donne au LLM pour générer une nouvelle pédale (bloc de C à coller).

### Component 3: `src/audio.c` (modifié) — callback qui applique la chaîne
- Le callback lit l'`atomic` pointeur vers la `chain_t` courante (ou un index) et applique chaque pédale.
- **RT-safe** : la chaîne est dans une mémoire pré-allouée (pas de malloc dans le callback). Le thread main construit la chaîne dans un tampon statique, puis atomiquement active l'index.
- Enregistrement : ring buffer pré-alloué (statique) de taille fixe ; le callback y écrit si `recording`.
- `audio_set_input` / `audio_set_output` : sélection de device (déjà partiellement en V1, à adapter).
- `audio_start` / `audio_stop` : open/close du stream PortAudio.

### Component 4: `src/wav.c` (nouveau) — encodeur WAV
- `wav_write(path, samples, n, sample_rate)` : écrit un WAV PCM 16-bit mono (en-tête + données).
- Convertit les `float` (post-pédales) en `int16` (clamp + scale).
- Appelé par le thread main (pas le RT) après l'arrêt du record.

### Component 5: `src/tdp_params.h` / atomiques partagés
- `atomic_bool running` (start/stop)
- `atomic_bool recording`
- `atomic_int chain_idx` (index de la chaîne active)
- `atomic_float vu_in_rms, vu_in_peak, vu_out_rms, vu_out_peak`
- (Plus besoin de bypass/mute atomiques séparés — c'est dans la chaîne de pédales.)

## Commands (référence)

| Commande | Arg | Effet | FR |
|----------|-----|-------|-----|
| `in` / `out` | — | Lister les entrées / sorties (index + nom) | FR-002 |
| `in` / `out` | `N` | Sélectionner le périphérique N | FR-002 |
| `load` | `fichier` | Charger la chaîne depuis un `.tdp` | FR-004 |
| `start` | — | Démarrer le flux (pédales appliquées) | FR-003 |
| `stop` | — | Arrêter le flux | FR-003 |
| `record` | — | Commencer l'enregistrement (post-pédales) | FR-006 |
| *(ligne vide)* | — | Si record actif : stopper + sauvegarder WAV | FR-006 |
| `status` | — | Snapshot RMS/peak (in/out) + pédales + état | FR-008 |
| `watch` | — | Toggle affichage périodique du niveau | FR-008 |
| `pedals` | — | Lister le catalogue (nom + desc) | FR-007 |
| `info` | `nom` | Détails d'une pédale (params, min/max/défaut) | FR-007 |
| `chain` | — | Afficher la chaîne chargée (ordre + params) | FR-009 |
| `reset` | — | Reset les params de la chaîne aux défauts | FR-009 |
| `help` | — | Vue d'ensemble : intro, commandes, exemple, pédales | FR-009 |
| `quit` / `exit` | — | Quitter proprement (stop audio) | FR-009 |

## Data Flow
1. **Chargement** : `load x.tdp` → parser → pour chaque ligne, `pedal_find(name)` + parse des params → remplit `chain_t` (tampon statique) → `chain_idx` atomique pointe dessus.
2. **Flux** : `start` → `audio_start(input, output)` → le callback PortAudio lit `chain_idx`, applique les pédales, calcule VU, écrit le ring si `recording`.
3. **Enregistrement** : `record` → `recording=true`. Le callback écrit dans le ring (wrap). Entrée (vide) → `recording=false` → le thread main lit le ring → `wav_write(recordings/<ts>.wav)`.
4. **Affichage** : `status` → lit les `vu_*` atomiques → `printf`. `watch on` → une sous-boucle (ou un timer) ré-affiche le niveau périodiquement (toutes les ~100 ms) via `\r` (retour chariot) pour ne pas polluer le shell ; `watch off` arrête.

## Error Handling
- **Aucune carte / device absent** : `audio_start` échoue → message lisible (`TDP: audio indisponible`), `running` reste false, pas de crash.
- **`.tdp` absent** : `load` → erreur `TDP: fichier introuvable: x.tdp`.
- **Pédale inconnue** : `load` → erreur `TDP: pedale inconnue 'foo' (ligne 3)`, chaîne non chargée.
- **Param hors plage** : clampé au [min,max] + avertissement `TDP: gain=5 clampé a 1.0`.
- **`record` sans flux** : avertissement `TDP: rien a enregistrer (flux non demarre)`.
- **Double `start`** : avertissement `TDP: deja en cours`.
- **Commande inconnue** : `TDP: commande inconnue 'xyz' (help pour la liste)`. En mode script : **continuer** (pas d'arrêt) — plus tolérant pour l'automatisation.
- **Entrée (Enter) sans record** : ignorée (pas de faux enregistrement).
- **Ring buffer plein** (enregistrement > capacité) : wrap (on écrase le plus ancien) — durée max limitée (ex. ~60 s à 48 kHz mono 16-bit ≈ 5,7 Mo). Avertissement au chargement du WAV si overflow.

## Security
- Pas de saisie de code, pas d'exécution de commandes système. Le shell interprète un **ensemble fixe** de commandes internes (pas de `system()`/`exec`).
- Les noms de fichier (`.tdp`, WAV) sont utilisés en lecture/écriture simple ; pas de risque d'injection (pas de shell OS).
- Le ring buffer est de taille fixe (pas de débordement : wrap contrôlé).
- Les params sont clampés (pas de NaN/inf dans le DSP : `tanh` est borné, le gate est borné).

## Testing Strategy
- **Unit (sans carte son)** : `make test` étendu.
  - **Catalogue** : `pedal_find` trouve chaque pédale ; les params ont min<max et def dans [min,max] ; `info` produit la bonne sortie.
  - **Parser .tdp** : un `.tdp` valide → chaîne correcte (ordre, params) ; une ligne invalide → erreur ; commentaires/lignes vides ignorés ; param hors plage clampé.
  - **DSP pédales** : `booster` (x*level), `distortion` (tanh), `noise_gate` (silence si RMS<seuil), `volume`, `mute` (0). Testées sur des buffers synthétiques.
  - **WAV** : `wav_write` sur un buffer de test → vérifier l'en-tête (format PCM, sample rate, taille) + quelques samples.
  - **Chaîne complète** : un buffer → booster → distortion → volume → vérifier la sortie attendue.
- **Valgrind** : `make check` sur le binaire de test → 0 fuite, 0 erreur (le ring buffer et la chaîne sont statiques → pas de fuite).
- **Manuel (carte son)** : `make run` → `in`/`out`/`load`/`start`/`record`+Enter/`status`/`watch`/`quit`. Vérifier le WAV produit (durée, contenu).
- **Script** : un `script.txt` (load, start, record, vide, stop, quit) exécuté en non-interactif → termine proprement, produit le WAV.

## Commande `help` (vue d'ensemble)

La commande `help` affiche une vue d'ensemble complète (pas juste une liste) :

```
TDP — Terminal Distortion Pedal (CLI)
Chaine de pedales audio temps reel, pilotee par commandes.

Commandes :
  in [N]        Listes les entrees / choisit l'entree N
  out [N]       Listes les sorties / choisit la sortie N
  load <fichier> Charge une chaine de pedales (.tdp)
  chain         Affiche la chaine chargee
  start         Demarre le flux audio (pedales appliquees)
  stop          Arrete le flux audio
  record        Enregistre le signal (post-pedales)
  <Entree>      Arrete l'enregistrement + sauvegarde WAV horodate
  status        Snapshot du niveau (RMS/peak in/out, etat)
  watch         Affichage continu du niveau (on/off)
  pedals        Liste les pedales disponibles
  info <nom>    Details d'une pedale (params, min/max/defaut)
  reset         Reset les params de la chaine aux defauts
  help          Cette aide
  quit          Quitter

Pedales (V1) :
  booster      Gain lineaire propre (level)
  distortion   Distorsion tanh (gain)
  noise_gate   Coupe le bruit de fond (threshold)
  volume       Master volume de sortie (level)
  mute         Silenced total

Exemple :
  tdp> in
  tdp> out
  tdp> load guitar.tdp
  tdp> start
  tdp> record
  ... (jouer) ...
  tdp>            <Entree> -> sauvegarde recordings/2026-07-09_15-42-33.wav
  tdp> stop
  tdp> quit
```

Le contenu est généré depuis le catalogue (`pedals_all()`) pour la liste des pédales, donc il reste à jour si on ajoute une pédale.

## Requirements Traceability
| Requirement | Design Coverage |
|-------------|-----------------|
| FR-001 | Component 1 (shell.c) : boucle fgets interactif/script, dispatch |
| FR-002 | Commandes `in`/`out` → `audio_list_devices` + sélection atomique |
| FR-003 | `start`/`stop` → `audio_start`/`audio_stop` + `running` atomique |
| FR-004 | `load` → parser .tdp → construit `chain_t` (tampon statique) |
| FR-005 | Component 2 (pedals.c) : 5 pédales + fonctions DSP |
| FR-006 | `record`+Enter → ring buffer (RT) + `wav_write` (main) |
| FR-007 | Component 2 : catalogue `pedal_def` + `pedals`/`info` |
| FR-008 | `status`/`watch` → lit `vu_*` atomiques |
| FR-009 | `help`/`quit`/`reset`/`chain` (shell.c) |
| FR-010 | Même code C, `make` (Linux) + `make dist` (Windows, portaudio.dll) |

## Open Questions
- **OQ-001** : `watch` utilise un timer/sous-boucle dans le thread main → comment ne pas bloquer la lecture de la commande suivante ? (Solution probable : `watch` est un mode qui, à chaque tour de boucle, ré-affiche si le niveau a changé + `usleep` court ; à valider.)
- **OQ-002** : Le ring buffer d'enregistrement est-il mono ou stéréo ? (V1 : mono — l'entrée est mono. À confirmer.)
- **OQ-003** : Durée max d'enregistrement (taille du ring) — 60 s ? (À trancher.)
- **OQ-004** : En mode script, une erreur de commande fait-elle continuer ou arrêter ? (Choix actuel : continuer.)
- **OQ-005** : `reset` reset les params de la chaîne aux défauts du catalogue, ou recharge le `.tdp` ? (Choix actuel : reset aux défauts du catalogue.)
