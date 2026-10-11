# TDP V1 — Terminal Distortion Pedal (C11, Ubuntu, ncurses + PortAudio) Brainstorm

Spec: `tdp-v1`  
Status: brainstorm-complete  
Created: 2026-10-11

## User Intent

Brainstorm pour la V1 du projet TDP (Terminal Distortion Pedal), source de vérité = specs.md.

## User intent
Implémenter la V1 complète décrite dans specs.md, à partir d'un repo vide, via UNE SEULE spec `tdp-v1`. Application C11 sur Ubuntu : 2 threads (audio RT + UI ncurses), DSP tanh mono (Gain + Master), VU-mètres RMS/peak-hold, contrôles clavier complets, build Makefile. Succès = `make` compile, `make test` passe, `make run` produit le son traité et la TUI répond.

## Target users/stakeholders
L'auteur du projet (développeur). Utilisateur final = guitariste/synth qui joue en direct, pilotant la pédale au clavier depuis un terminal.

## Problem and desired outcome
Aucun code n'existe. Objectif : une application C11 fonctionnelle sur Ubuntu — thread audio temps réel (PortAudio) + thread UI (ncurses), DSP tanh mono (Gain 0..1 défaut 0.5, Master 0..1 défaut 0.75), VU INPUT/OUTPUT (RMS + peak-hold 500 ms) calculés dans le thread audio, contrôles clavier (ESPACE bypass, M mute, ↑/↓ cycle param, ←/→ ±0.05, R reset, q/CTRL-C quit), pied de page sr/buffer/statut PortAudio.

## In scope
- Makefile : cibles `make`, `clean`, `run`, `test` ; flags `-std=c11 -O2 -Wall -Wextra -pthread -lportaudio -lncurses -lm`.
- `src/main.c` : boucle ncurses, rendu TUI ~30-60 fps, capture clavier non bloquante (nodelay+getch).
- `src/audio.c` : callback PortAudio, stream mono, sr 44100/48000 Hz, buffer 128/256, latence cible ≤ 10 ms.
- `src/dsp.c` : chaîne `input → Gain G → tanh(G·x) → Master V → output` + calcul VU (RMS + peak-hold) DANS le thread audio.
- `src/params.c` + `include/params.h` : variables `stdatomic` partagées (params, VU 4 atomics : vu_input_rms, vu_input_peak, vu_output_rms, vu_output_peak).
- `tests/test_dsp.c` : tests OBLIGATOIRES (`make test`), sans carte son : tanh(0)=0, bornes (tanh→±1), pas de NaN sur entrées extrêmes (±10, ±1e6).
- Contrôles clavier : ESPACE (BYPASS signal raw), M (MUTE sortie=0, VU OUT→-inf, VU IN continue), ↑/↓ (cycle Gain↔Master), ←/→ (±0.05), R (reset 0.5/0.75), q / CTRL-C (quit propre).
- TUI : sélection `> `, jauges 12 cases (10%/case) + valeur %, VU 20 cases + dB (0 dB pleine échelle, floor -60 dB) + peak-hold, en-tête titre + ON/BYPASS, pied de page `sr Hz · buffer N · PortAudio OK/ERREUR`.
- Contraintes RT STRICTES (exigences durables, avec audit) : thread audio sans malloc/free, sans I/O/bloquant, sans lock, lecture uniquement de stdatomic.
- README.md conforme §6 specs.md (sections obligatoires, arbre du projet, à jour à chaque commit de structure).

## Out of scope (V2+)
- hot-swap de fréquence, multi-waveform (hard clip, rectifier), tone control (filtre passe-bas), post-gain/boost distinct, stéréo avec params par canal, presets (JSON/TOML), CMake, CI.
- WASM/browser (Web Audio API + UI web) — repoussé.
- Windows — repoussé (le code reste "portable C11 standard" pour faciliter le port futur, sans build/test Windows en V1).
- normalisation de gain (make-up gain) auto.
- ring buffer de commandes.

## Constraints/risks
- Contraintes RT strictes → exigence formelle + audit dans les tâches.
- Dépendances : PortAudio ≥ 19.7, ncurses ≥ 6.0, GCC ≥ 9 ou Clang ≥ 11, libpthread, sur Ubuntu (PipeWire/PulseAudio).
- Risque : sr/buffer réel imposé par la carte → l'UI affiche l'état réel (pas supposé), statut PortAudio OK/ERREUR en pied de page.
- Risque : latence ≤ 10 ms dépend du buffer (128 vs 256) et du backend audio → à vérifier à make run.
- make test doit tourner SANS carte son (tests DSP purs).
- Si la carte impose 2 canaux : on duplique le traitement sur les 2 mais l'UI ne gère qu'un seul paramètre.

## Proposed approach
1. Squelette Makefile + include/params.h (atomics) + src/params.c.
2. src/dsp.c (chaîne tanh+master+VU) + tests/test_dsp.c (validés par make test).
3. src/audio.c (callback PortAudio mono, contraintes RT).
4. src/main.c (boucle ncurses, clavier, rendu, VU, pied de page).
5. README.md conforme §6 + validation finale `make && make test && make run`.

## Assumptions
- specs.md est la source de vérité technique et est à jour.
- Développement sur Ubuntu (PipeWire/PulseAudio), carte son mono ou stéréo (stéréo → duplication du traitement).
- Tests DSP s'exécutent sans carte son.
- Une seule spec pour toute la V1.

## Remaining doubts
- 128 vs 256 samples : à trancher au design (probablement laisser PortAudio choisir dans la plage, ou 256 par défaut).
- Fallback exact si PortAudio refuse le sr/buffer demandé (message pied de page) → au design.

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
