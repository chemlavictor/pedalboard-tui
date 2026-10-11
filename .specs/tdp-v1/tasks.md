# TDP V1 — Terminal Distortion Pedal (C11, Ubuntu, ncurses + PortAudio) Tasks

Spec: `tdp-v1`
Status: implementation-in-progress
Created: 2026-10-11  
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

## Implementation Plan

Only include implementation tasks after requirements and design are accepted. Keep tasks small, ordered, and traceable.

- [x] 1. Squelette du build : Makefile + cible `all`/`clean`/`run`
  - ID: T-001
  - Requirement(s): FR-001, FR-002, FR-003
  - Files/areas: `Makefile`
  - Notes: flags `-std=c11 -O2 -Wall -Wextra -pthread -lportaudio -lncurses -lm` ; `all` → `build/tdp` (compile `src/*.c`) ; `clean` → `rm -rf build` ; `run` → `./build/tdp`. Créer `build/` via `mkdir -p`. Le binaire final dépendra de tous les `src/*.c` ; pour que `make` passe avant l'étape 5, prévoir soit un `src/main.c` minimal, soit une règle qui ne compile que les `.c` présents.
  - Validation: `make` compile sans warnings (`-Wall -Wextra`) ; `make clean` vide `build/`.

- [x] 2. État partagé atomique : `params.h` + `params.c`
  - ID: T-002
  - Requirement(s): FR-005, FR-006, FR-007, FR-008, FR-009, NFR-003
  - Files/areas: `include/params.h`, `src/params.c`
  - Notes: struct `tdp_params` (gain, master, bypass, mute, 4× vu_*, sr, frame_buffer, pa_ok) tous `_Atomic` ; instance unique `P` ; `params_init()` pose défauts (0.5 / 0.75). Aucune dépendance PortAudio/ncurses.
  - Validation: `make` compile ; un `main` de test trivial (ou compilation seule) confirme l'initialisation des défauts.

- [x] 3. Cœur DSP : `dsp.h` + `dsp.c` (C pur, portable)
  - ID: T-003
  - Requirement(s): FR-010, FR-011, FR-012, FR-013, FR-014, FR-015, FR-016, NFR-003
  - Files/areas: `include/dsp.h`, `src/dsp.c`
  - Notes: `dsp_process(in, out, n, G, V, bypass, mute, sr, *vu)` — `bypass`→`out=in` ; sinon `y=tanh(G*x)`, `out=V*y` ; `mute`→`out=0` (DSP exécuté) ; VU_in = RMS(`in`), VU_out = RMS(`out`) ; peak-hold 500 ms décroissant (état passé via la struct VU). `math.h` uniquement.
  - Validation: `make` compile sans warnings.

- [x] 4. Tests unitaires DSP + cible `make test` (sans carte son)
  - ID: T-004
  - Requirement(s): FR-004, FR-010, FR-011, FR-012, FR-013, FR-014, FR-015, FR-016, NFR-005
  - Files/areas: `tests/test_dsp.c`, `Makefile` (cible `test`)
  - Notes: asserts — `tanh(0)=0` (in=[0]→out≈0) ; bornes ±10/±1e6 → out∈[-1,1] sans NaN ; bypass → out==in ; mute → out==0 et VU_out=0 ; RMS sinusoïde ≈ amp/√2 ; peak-hold décroît linéairement sur 500 ms. Cible `test` compile `tests/test_dsp.c` SANS PortAudio/ncurses, exit non nul si échec.
  - Validation: `make test` passe, sans carte son ni TTY.

- [x] 5. Vérification mémoire : cible `make check` (Valgrind)
  - ID: T-005
  - Requirement(s): FR-004b, NFR-006
  - Files/areas: `Makefile` (cibles `check`, `check-app`)
  - Notes: `check` → `valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 ./build/test_dsp` (exige `valgrind` installé). `check-app` (optionnelle) → app réelle sous valgrind avec `timeout 5`. Documenter `sudo apt install valgrind` dans le README.
  - Validation: `make check` → `definitely lost: 0`, aucun invalid access.

- [x] 6. Thread audio : `audio.h` + `audio.c` (callback PortAudio)
  - ID: T-006
  - Requirement(s): FR-017, FR-018, FR-019, FR-020, FR-021, FR-022, FR-023, NFR-001
  - Files/areas: `include/audio.h`, `src/audio.c`
  - Notes: `audio_start()` ouvre stream PortAudio mono in+out, demande 48000/256, fallback en cascade 48k→44k→(default,256) ; stocke sr/buffer/pa_ok dans `P` ; callback → `dsp_process` (lit params via `atomic_load`, écrit VU). **Contraintes RT strictes** : pas de malloc/free, pas de I/O/printf/lock dans le callback. `audio_stop()` → `paCloseStream`.
  - Validation: `make` compile ; audit visuel du callback (aucun malloc/printf/fopen/lock) ; `make check` reste vert.

- [x] 7. Thread UI : `main.c` (ncurses, clavier, rendu, shutdown)
  - ID: T-007
  - Requirement(s): FR-024, FR-025, FR-026, FR-027, FR-028, FR-029, FR-030, FR-031, FR-032, FR-033, FR-034, FR-035, FR-005, FR-006, FR-036(partial)
  - Files/areas: `src/main.c`
  - Notes: `initscr/cbreak/noecho/nodelay` ; boucle ~30–60 fps ; rendu : en-tête ON/BYPASS (surligné), sélection `> `, jauges 12 cases + %, VU 20 cases + dB (floor -60) + peak, pied de page `sr · buffer · OK/ERREUR` ; clavier : ESPACE/M/↑/↓/←/→(±0.05, clamp)/R/q/CTRL-C ; `selected` local ; shutdown : `audio_stop()` + `endwin()`, exit 0.
  - Validation: `make` compile sans warnings ; `make run` rend la TUI et répond au clavier (voir T-008).

- [ ] 8. Validation E2E audio + TUI (`make run`)
  - ID: T-008
  - Requirement(s): NFR-001, NFR-004, FR-015, FR-016, FR-019, FR-024..FR-035
  - Files/areas: (validation intégrée, pas de nouveau code si pas de bug)
  - Notes: brancher guitare → jouer → entendre le son traité en sortie. Vérifier à l'oreille : distorsion audible (G élevé), BYPASS = signal sec, MUTE = silence (VU IN continue). Vérifier visuellement : sélection, jauges, VU, en-tête, pied de page. Chaque touche → effet attendu.
  - Validation: `make run` — audio + TUI conformes ; pied de page affiche sr/buffer réels + `OK`.

- [x] 9. README.md conforme §6 + arbre du projet
  - ID: T-009
  - Requirement(s): FR-036, NFR-004
  - Files/areas: `README.md`
  - Notes: sections obligatoires : *C'est quoi ?*, *Comment ça marche*, *Installation* (apt/fedora/brew + valgrind), *Contrôles clavier*, *Structure du projet* (arbre à jour), *Légende des dossiers*, *Roadmap*.
  - Validation: revue du fichier — toutes les sections présentes, arbre du dépôt à jour.

- [ ] 10. Validation finale complète
  - ID: T-010
  - Requirement(s): FR-001..FR-036, NFR-001..NFR-006
  - Files/areas: (validation globale)
  - Notes: exécuter la chaîne complète et confirmer l'absence de warnings et de fuites.
  - Validation: `make && make test && make check && make run` — tout passe, 0 warnings, 0 fuite.

## Verification Checklist

- [ ] Requirements IDs are referenced by tasks.
- [ ] Design testing strategy is represented by tasks.
- [ ] Each task has a clear validation command or manual check.
- [ ] Rollback/migration tasks are included when applicable.

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before coding.
- When coding starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering.
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
