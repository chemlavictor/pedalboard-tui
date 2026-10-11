# TDP CLI — shell de commandes (remplace le TUI) Brainstorm

Spec: `tdp-cli`  
Status: brainstorm-complete  
Created: 2026-10-11

## User Intent

## Contexte
La V1 TDP est une TUI ncurses (Linux) / console (Windows) avec UNE seule pédale (tanh distortion, Gain+Master). Le TUI a engendré beaucoup de bugs (clavier Windows, Unicode, device selection). L'utilisateur veut un **changement de paradigme** : tout en **CLI / shell de commandes**, beaucoup plus simple, 0 bug d'interface.

## Décisions prises (brainstorm)
1. **Mode CLI seul** : le TUI est **remplacé** (option A) par un shell interactif `tdp>`. Plus de ncurses, plus de console Windows custom. L'UI = stdout/stdin texte simple.
2. **Double mode d'exécution** : `./tdp.exe [script]`.
   - Sans arg : mode **interactif** (prompt `tdp>`, on tape les commandes).
   - Avec arg : **lecture d'un script** (fichier, ligne par ligne, on exécute chaque commande) — non interactif, pour l'automatisation.
3. **Cycle de vie audio** :
   - `in` / `out` : lister ou choisir l'entrée / la sortie audio.
   - `start` : démarre le flux audio (la chaîne de pédales traite le signal).
   - `stop` : arrête le flux.
   - Le prompt reste utilisable pendant que ça sonne (on peut `record`, changer params, etc. en live).
4. **Enregistrement** :
   - `record` : commence à capturer le signal **après les pédales** (sortie de la chaîne).
   - **Entrée (Enter)** (ligne vide) : stoppe l'enregistrement **et** sauvegarde le contenu.
   - Format : **WAV** (encodeur maison, non compressé).
   - Fichier : `recordings/YYYY-MM-DD_HH-MM-SS.wav` (horodaté).
5. **Pédales (V1 CLI)** — chaîne en série, une pédale par ligne dans un fichier `.tdp` :
   - `booster` : gain linéaire propre (type TS9 level à fond). Param : `level` (0..2, défaut 1.5).
   - `distortion` : le `tanh(G·x)` actuel. Param : `gain` (0..1, défaut 0.5).
   - `noise_gate` : coupe le signal si RMS < seuil (chugs nets). Param : `threshold` (dB, défaut -45).
   - `volume` : master volume de sortie. Param : `level` (0..1, défaut 0.75).
   - `mute` : silenced total (toggle).
   - **Ampli High-Gain** : exclu de la V1 (trop lourd à simuler fidèlement) — l'utilisateur utilise son vrai ampli / simu externe.
6. **Format du fichier de pédales** (`.tdp`) :
   - Une pédale par ligne (l'ordre = l'ordre en série).
   - Params en `nom=valeur` après le nom.
   - `#` en début de ligne = commentaire (ignoré).
   - Ligne vide = ignorée.
   - Exemple `guitar.tdp` :
     ```
     # chaîne chug
     booster level=2.0
     distortion gain=0.6
     noise_gate threshold=-40
     volume level=0.8
     ```
7. **Catalogue de pédales** (`include/pedals.h` + `src/pedals.c`) :
   - Code C : tableau de structures `pedal_def { name, description, params[], process_fn }` + les fonctions DSP.
   - **Lu au runtime** : `tdp> pedals` (liste), `tdp> info <nom>` (détails + params + défauts).
   - Sert de **source de vérité pour le prompt LLM** : l'utilisateur donne `src/pedals.c` à un LLM pour générer une nouvelle pédale (entrée de catalogue + fonction DSP).
8. **Ajout de pédale = recompiler** (pas de plugin .so / interpréteur). Workflow : LLM génère un bloc de C → on le colle dans `pedals.c` → `make`. Simple, fiable, performant.

## Architecture cible
- **Thread Audio (RT)** : callback PortAudio, applique la chaîne de pédales (list de `pedal_def`) sur le buffer, calcule le VU, écrit dans un ring buffer si `record` actif. Pas de malloc/I/O/locks. stdatomic.
- **Thread main (CLI)** : boucle de lecture des commandes (stdin). Interprète, modifie les atomics (params, record, start/stop), liste les devices, charge les `.tdp`, encode le WAV à l'arrêt de l'enregistrement.
- **Pas de TUI** : l'affichage = `printf` sur stdout. Le rendu "live" (VU) est optionnel (commande `status` ou affichage périodique) — à trancher au design.

## Questions ouvertes (à trancher dans requirements/design)
- Affichage VU live en mode CLI : commandé explicitement (`status`) ou périodique ? (Le shell est texte, pas de refresh d'écran.)
- Gestion des erreurs audio (aucune carte, device absent) : message + état, sans crash.
- Le `record` en mémoire : ring buffer de taille fixe (mémoire bornée, RT-safe) — durée max ? (ex. ~30 s à 48 kHz mono).
- `start` sans `in`/`out` choisis : utilise les devices par défaut PortAudio.
- Compatibilité Windows + Linux : le shell utilise `readline`-like simple (fgets), pas de ncurses. PortAudio identique.

## Hors périmètre V1 CLI
- Ampli High-Gain (simu), Wah, Delay, Reverb (pistes V2+).
- Plugin dynamique / interpréteur DSP (recompile uniquement).
- TUI ncurses (remplacé, conservé en git history).
- WASM / navigateur.

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.
