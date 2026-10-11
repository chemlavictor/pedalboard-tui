# TDP V1 — Terminal Distortion Pedal (C11, Ubuntu, ncurses + PortAudio) Requirements

Spec: `tdp-v1`  
Status: requirements-approved  
Created: 2026-10-11  
Brainstorm: `./brainstorm.md`  
Source de vérité technique: `specs.md`

## Overview

Implémenter la V1 complète du projet TDP (Terminal Distortion Pedal) décrite dans `specs.md` : application C11 sur Ubuntu, 2 threads (audio temps réel PortAudio + UI ncurses), DSP tanh mono (Gain + Master), VU-mètres RMS/peak-hold, contrôles clavier complets, build Makefile. Succès = `make` compile, `make test` passe, `make run` produit le son traité et la TUI répond correctement.

## Scope

### In Scope

- Système de build Makefile avec cibles `make` (→ `build/tdp`), `clean`, `run`, `test`.
- Partage d'état lock-free entre threads via `stdatomic` (`src/params.c`, `include/params.h`).
- Chaîne DSP mono : `input → Gain G → tanh(G·x) → Master V → output` (`src/dsp.c`).
- VU-mètres INPUT/OUTPUT (RMS + peak-hold) calculés dans le thread audio.
- Thread audio temps réel : callback PortAudio, stream mono, 44100/48000 Hz, buffer 128/256 (`src/audio.c`).
- Thread UI : boucle ncurses, rendu ~30–60 fps, capture clavier non bloquante (`src/main.c`).
- Contrôles clavier : bypass, mute, navigation paramètre, inc/déc, reset, quit.
- TUI : sélection visible, jauges de paramètres, jauges VU + dB, en-tête ON/BYPASS, pied de page sr/buffer/statut PortAudio.
- Tests unitaires DSP obligatoires, exécutables sans carte son (`tests/test_dsp.c`, `make test`).
- README.md conforme à la règle de documentation `specs.md` §6.

### Out of Scope

- Hot-swap de la fréquence d'échantillonnage.
- Multi-waveform (hard clip, rectifier) ; tanh seul en V1.
- Tone control (filtre passe-bas).
- Post-gain / boost distinct du Master.
- Stéréo avec paramètres par canal (si la carte impose 2 canaux, on duplique le traitement, l'UI reste mono).
- Presets sauvegardés (JSON/TOML).
- CMake ; Makefile uniquement.
- CI (build + smoke test).
- Cible WASM / browser (Web Audio API + UI web).
- Cible Windows (le code reste « portable C11 standard » pour faciliter un port futur, sans build/test Windows en V1).
- Normalisation de gain (make-up gain) automatique.
- Ring buffer de commandes inter-threads.

## User Stories

- En tant que guitariste jouant en direct, je veux lancer la pédale et régler Gain/Master au clavier, afin de façonner ma distorsion sans toucher à l'audio.
- En tant que guitariste, je veux voir les niveaux d'entrée et de sortie en temps réel (VU + dB), afin de doser le signal sans clipper ou couper.
- En tant que guitariste, je veux basculer en BYPASS d'un clavier, afin d'entendre mon signal sec instantanément.
- En tant que guitariste, je veux couper le son d'un clavier (MUTE) sans perdre mes réglages, afin de me faire oublier pendant une prise.
- En tant que développeur, je veux `make && make test && make run` qui fonctionne sur Ubuntu, afin de valider chaque itération rapidement.

## Functional Requirements

Use stable IDs and testable EARS-style statements.

### Build

- **FR-001**: THE system SHALL provide a Makefile avec une cible `make` qui compile `build/tdp` avec les flags `-std=c11 -O2 -Wall -Wextra -pthread -lportaudio -lncurses -lm`.
- **FR-002**: THE system SHALL provide a cible `make clean` qui supprime l'artefact `build/`.
- **FR-003**: THE system SHALL provide a cible `make run` qui exécute `build/tdp`.
- **FR-004**: THE system SHALL provide a cible `make test` qui compile et exécute `tests/test_dsp.c` SANS requérir de carte son, et qui échoue (code de retour non nul) si un assert échoue.
- **FR-004b**: THE system SHALL provide a cible `make check` qui exécute les tests sous Valgrind (`memcheck`, `--leak-check=full`, `--error-exitcode` non nul) SANS carte son, et qui échoue si une fuite, un accès invalide ou un out-of-bounds est signalé. Une cible `make check-app` (optionnelle, carte son + TTY) exécute l'app réelle sous Valgrind avec durée limitée.

### Paramètres & état partagé

- **FR-005**: THE system SHALL expose le paramètre `Gain` (G), `float` 0.0–1.0, défaut **0.5**, stocké dans un `stdatomic`.
- **FR-006**: THE system SHALL expose le paramètre `Master` (V), `float` 0.0–1.0, défaut **0.75**, stocké dans un `stdatomic`.
- **FR-007**: THE system SHALL expose un `atomic_bool` `bypass` et un `atomic_bool` `mute` pour l'état de la pédale.
- **FR-008**: THE system SHALL expose 4 `atomic_float` de VU bornés 0.0–1.0 : `vu_input_rms`, `vu_input_peak`, `vu_output_rms`, `vu_output_peak`.
- **FR-009**: WHEN le thread UI modifie un paramètre, THE thread audio SHALL le lire sans lock via `stdatomic` (aucun mutex/lock).

### DSP

- **FR-010**: FOR each échantillon d'entrée `x` (mono), THE system SHALL calculer `y = tanh(G · x)` puis `out = V · y`.
- **FR-011**: THE system SHALL borner la sortie tanh naturellement dans [-1, 1] (pas de limiteur d'entrée en V1).
- **FR-012**: THE system SHALL calculer le VU d'entrée (RMS du buffer courant) sur le signal AVANT le DSP.
- **FR-013**: THE system SHALL calculer le VU de sortie (RMS du buffer courant) sur le signal APRÈS le DSP.
- **FR-014**: THE system SHALL maintenir un peak-hold de 500 ms en décroissance sur `vu_input_peak` et `vu_output_peak`.
- **FR-015**: IF le mode BYPASS est actif, THE system SHALL router l'entrée directement en sortie (contourner le DSP) et calculer les VU de sortie sur ce signal brut.
- **FR-016**: IF le mode MUTE est actif, THE system SHALL mettre la sortie à 0 (VU de sortie → silence) TANDIS QUE le DSP continue de s'exécuter (paramètres inchangés) et que le VU d'entrée continue.

### Thread audio

- **FR-017**: THE system SHALL ouvrir un stream PortAudio mono à 44100 Hz ou 48000 Hz, buffer 128 ou 256 échantillons.
- **FR-018**: THE system SHALL exécuter le DSP et le calcul des VU dans le callback PortAudio (thread audio), pas dans le thread UI.
- **FR-019**: THE system SHALL afficher dans le pied de page la fréquence d'échantillonnation et la taille de buffer RÉELLES du stream ouvert (pas supposées).
- **FR-020**: IF PortAudio échoue à ouvrir le stream, THE system SHALL afficher le statut `ERREUR` dans le pied de page au lieu de `OK`.

### Contraintes temps réel (STRICTES)

- **FR-021**: THE thread audio callback SHALL NOT effectuer d'allocation mémoire (`malloc`/`free`/`new`).
- **FR-022**: THE thread audio callback SHALL NOT effectuer d'appel bloquant (aucun `printf`/I/O, aucun lock/mutex).
- **FR-023**: THE thread audio callback SHALL lire les paramètres uniquement via `stdatomic` (aucune variable partagée non atomique).

### Interface TUI & clavier

- **FR-024**: THE system SHALL afficher un en-tête `TDP — Terminal Distortion Pedal` avec l'état `ON` (défaut) ou `BYPASS` (surligné quand BYPASS).
- **FR-025**: THE system SHALL afficher la ligne du paramètre sélectionné avec le préfixe `> ` (sélection visible).
- **FR-026**: THE system SHALL afficher pour chaque paramètre une jauge de 12 cases (10 % par case) suivie de la valeur en % (ex. `[=====-----]  50%`).
- **FR-027**: THE system SHALL afficher les VU INPUT et OUTPUT en jauge de 20 cases + valeur en dB (`dB = 20·log10(rms)`, plafonnée à -60 dB pour le silence) + marquage de crête (peak-hold).
- **FR-028**: THE system SHALL afficher un pied de page `fréquence d'échantillonnation Hz · buffer N · PortAudio OK/ERREUR`.
- **FR-029**: THE system SHALL capturer le clavier en non bloquant (`nodelay` + `getch`) dans la boucle UI (~30–60 fps).
- **FR-030**: WHEN the user presses `ESPACE`, THE system SHALL basculer BYPASS ↔ ON.
- **FR-031**: WHEN the user presses `M`, THE system SHALL basculer le MUTE global.
- **FR-032**: WHEN the user presses `↑` ou `↓`, THE system SHALL faire cycler le paramètre sélectionné (Gain ↔ Master).
- **FR-033**: WHEN the user presses `←` ou `→`, THE system SHALL diminuer/augmenter le paramètre sélectionné par pas de 0.05, borné à [0.0, 1.0].
- **FR-034**: WHEN the user presses `R`, THE system SHALL réinitialiser Gain à 0.5 et Master à 0.75.
- **FR-035**: WHEN the user presses `q` ou `CTRL-C`, THE system SHALL quitter proprement (stop du stream PortAudio, `endwin`, code de sortie 0).

### Documentation

- **FR-036**: THE system SHALL fournir un `README.md` à la racine avec les sections obligatoires : *C'est quoi ?*, *Comment ça marche*, *Installation*, *Contrôles clavier*, *Structure du projet* (arbre du dépôt), *Légende des dossiers*, *Roadmap*.

## Non-Functional Requirements

- **NFR-001**: THE system SHALL viser une latence audio totale ≤ 10 ms (buffer 128 ou 256 @ 44.1/48 kHz).
- **NFR-002**: THE system SHALL s'exécuter en C11 strict, compilé sans warnings avec `-Wall -Wextra`.
- **NFR-003**: THE system SHALL être portable C11 standard (pas d'appels POSIX exotiques dans le cœur DSP) pour faciliter un port futur (macOS/Windows/WASM).
- **NFR-004**: THE system SHALL tourner sur Ubuntu (PipeWire/PulseAudio/ALSA) avec PortAudio ≥ 19.7 et ncurses ≥ 6.0.
- **NFR-005**: THE `make test` target SHALL s'exécuter sans carte son et sans affichage (tests DSP purs).
- **NFR-006**: THE system SHALL viser zéro fuite mémoire (`definitely lost: 0`) et zéro accès invalide, vérifiables via `make check` (Valgrind) et/ou AddressSanitizer en développement.

## Acceptance Criteria

- [ ] `make` produit `build/tdp` sans warnings (`-Wall -Wextra`).
- [ ] `make test` compile et exécute `tests/test_dsp.c` sans carte son et passe (tanh(0)=0, bornes, pas de NaN sur ±10 / ±1e6).
- [ ] `make check` (Valgrind sur les tests) ne signale ni fuite (`definitely lost: 0`) ni accès invalide.
- [ ] `make run` ouvre un stream audio mono et fait passer le signal d'entrée traité en sortie (vérifié à l'oreille).
- [ ] La TUI affiche sélection `> `, jauges paramètres, VU INPUT/OUTPUT + dB, en-tête ON/BYPASS, pied de page sr/buffer/statut.
- [ ] Chaque touche (ESPACE, M, ↑/↓, ←/→, R, q, CTRL-C) produit l'action décrite dans FR-030..FR-035.
- [ ] BYPASS route le signal sec ; MUTE coupe la sortie sans altérer les réglages ni le VU d'entrée.
- [ ] Le callback audio est exempt de malloc/I/O/lock (audit de code, FR-021..FR-023).
- [ ] README.md contient toutes les sections obligatoires et l'arbre du projet est à jour.

## Edge Cases

- Carte son imposant 2 canaux alors que l'app est mono : dupliquer le traitement sur les 2 canaux, l'UI reste mono.
- PortAudio refusant le sr/buffer demandé : fallback au sr/buffer le plus proche, pied de page reflétant les valeurs réelles ; échec total → statut `ERREUR`.
- VU à silence (rms ≈ 0) : affichage plafonné à -60 dB, pas de `log10(0)`.
- Paramètre aux bornes (0.0 / 1.0) : ←/→ ne dépasse pas [0.0, 1.0].
- `q`/`CTRL-C` en cours de stream : fermeture ordonnée (stop stream puis `endwin`), pas de plantage.
- Buffer/sr réel différent de la demande : l'UI ne ment pas (affiche l'état réel).

## Dependencies and Constraints

- PortAudio ≥ 19.7 (`-lportaudio`), ncurses ≥ 6.0 (`-lncurses`), libpthread, libm.
- GCC ≥ 9 ou Clang ≥ 11 (C11 + `<stdatomic.h>`).
- OS cible V1 : Ubuntu (PipeWire/PulseAudio/ALSA).
- Séquencing suggéré : params/atomics → dsp+tests → audio → main/UI → README → validation finale.

## Open Questions

- [ ] Taille de buffer par défaut : 128 ou 256 (ou laisser PortAudio choisir dans la plage) → trancher au design.
- [ ] Fallback exact si PortAudio refuse le sr/buffer demandé (quel sr/buffer retenu, quel message) → trancher au design.
- [ ] Où stocker l'état « paramètre sélectionné » (Gain vs Master) et l'indice de sélection → au design.
