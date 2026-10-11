# TDP Windows — version compilée (console Windows native, sans ncurses) Tasks

Spec: `tdp-windows`
Status: implementation-in-progress
Created: 2026-10-11  
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

> Légende validation : **[ici]** = vérifiable sur cette machine (Linux). **[session-Win]** = à valider en session sur Windows (build/run).

## Implementation Plan

Only include implementation tasks after requirements and design are accepted. Keep tasks small, ordered, and traceable.

- [x] 1. Interface UI commune `include/ui.h`
  - ID: T-001
  - Requirement(s): FR-004, FR-008, FR-010, FR-014, FR-015
  - Files/areas: `include/ui.h`
  - Notes: enums `ui_key` (NONE/UP/DOWN/LEFT/RIGHT/SPACE/M/R/Q) ; prototypes `ui_init/shutdown/clear/cursor/text/color/title/poll_key/cols/lines/frame`. Commenté. Aucune dépendance ncurses ni Windows dans le header.
  - Validation: **[ici]** le header compile seul (`gcc -fsyntax-only -Iinclude include/ui.h` équivalent via un TU trivial).

- [x] 2. Refactor Linux : extraire `src/ui_ncurses.c` + `main.c` commun
  - ID: T-002
  - Requirement(s): FR-004, FR-008, FR-010, FR-024..FR-028 (parité V1), NFR-003
  - Files/areas: `src/ui_ncurses.c` (nouveau), `src/main.c` (refactor), `Makefile` (ajout ui_ncurses.c)
  - Notes: déplacer la partie ncurses de l'ancien `main.c` (render, draw_gauge, draw_vu, getch→ui_key, initscr/endwin) dans `ui_ncurses.c` implémentant `ui_*`. `main.c` garde la boucle commune (clavier via `ui_poll_key`, rendu via `ui_*`) et `#ifdef _WIN32` uniquement pour les points d'entrée. **La cible Linux doit rester identique.**
  - Validation: **[ici]** `make && make test && make check` vert ; `make run` (sans TTY) rend la même TUI que la V1 + fallback `ERREUR`. `git diff` du comportement V1 neutre.

- [x] 3. Implémentation console Windows `src/ui_windows.c`
  - ID: T-003
  - Requirement(s): FR-004, FR-005, FR-006, FR-007, FR-008, FR-009, FR-010, NFR-003, NFR-004
  - Files/areas: `src/ui_windows.c` (nouveau)
  - Notes: `#include <windows.h>` (confiné ici). `ui_init` : `SetConsoleOutputCP(CP_UTF8)`, `GetConsoleScreenBufferInfo` (cols/lines), activer ANSI (`SetConsoleMode` ENABLE_VIRTUAL_TERMINAL_PROCESSING), double-buffer. `ui_text/cursor` : `WriteConsoleW`/`SetConsoleCursorPosition`. `ui_color` : séquences ANSI + repli no-op si non supporté. `ui_title` : `SetConsoleTitleW`. `ui_poll_key` : `PeekConsoleInput`/`ReadConsoleInput` → `ui_key` (flèches, espace, M/m, R/r, q/Q, CTRL-C→Q). Rendu en blocs Unicode `█▓░` (mêmes jauges/VU/layout que V1).
  - Validation: **[ici]** revue statique (aucun `#include <ncurses.h>`, appels Windows confinés) + syntaxe (si wine-gcc dispo, sinon revue). **[session-Win]** compile sous MSYS2 sans ncurses, rend la TUI, répond au clavier.

- [x] 4. Makefile : cible Windows MSYS2/MinGW64
  - ID: T-004
  - Requirement(s): FR-014, FR-015, FR-016, NFR-002
  - Files/areas: `Makefile`
  - Notes: cible `windows` (ou autodétection `$(OS)`) : compile `tdp.exe` = `main.c ui_windows.c audio.c dsp.c params.c` + `-lportaudio -lm` (sans ncurses). Garder la cible Linux (ncurses). Flags `-std=c11 -O2 -Wall -Wextra`.
  - Validation: **[ici]** `make` (Linux) toujours vert. **[session-Win]** `make` sous MSYS2 produit `tdp.exe` sans warnings.

- [x] 5. `build_windows.bat` → `dist/`
  - ID: T-005
  - Requirement(s): FR-015, FR-016, FR-017
  - Files/areas: `build_windows.bat` (nouveau)
  - Notes: script MinGW direct : `gcc -std=c11 -O2 -Wall -Wextra -Iinclude -o dist\tdp.exe src\main.c src\ui_windows.c src\audio.c src\dsp.c src\params.c -lportaudio -lm -static-libgcc` ; crée `dist/` ; copie `portaudio.dll`/`libportaudio-0.dll` du toolchain vers `dist/` ; copie/écrit un `dist/README.md` de lancement. Message d'erreur si DLL introuvable dans le toolchain.
  - Validation: **[ici]** revue du script. **[session-Win]** `build_windows.bat` génère `dist\tdp.exe` + `dist\portaudio.dll` + `dist\README.md`.

- [x] 6. Message d'erreur DLL PortAudio absente
  - ID: T-006
  - Requirement(s): FR-017
  - Files/areas: `src/main.c`
  - Notes: au démarrage, si `Pa_Initialize()`/`audio_start()` échoue et `P.pa_ok==false`, afficher (avant ou en pied de page) : `portaudio.dll introuvable ? Vérifiez qu'elle est à côté de tdp.exe.` ; exit non nul si aucun stream possible (selon décision design : TUI reste utilisable ou exit). Conforter la détection.
  - Validation: **[ici]** revue du code. **[session-Win]** supprimer `dist\portaudio.dll` → message lisible, pas de crash.

- [x] 7. README section Windows
  - ID: T-007
  - Requirement(s): FR-018
  - Files/areas: `README.md`
  - Notes: section « Windows (build compilé) » : installation MSYS2 + `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-portaudio` (principal), usage `make` ou `build_windows.bat` (secours), contenu de `dist/`, lancer `dist\tdp.exe`, et rappel de la piste WSL2 (déjà présente).
  - Validation: **[ici]** revue : sections présentes, arbre à jour si nouveau fichier.

- [x] 8. Validation finale — cœur (ici) + Windows (session)
  - ID: T-008
  - Requirement(s): FR-001..FR-018, NFR-001..NFR-005
  - Files/areas: (validation globale)
  - Notes: **[ici]** `make && make test && make check` vert (cœur inchangé) ; `git diff` confirme `dsp.c`/`params.c`/`audio.c` inchangés ; revue statique `ui_windows.c`. **[session-Win]** build (MSYS2 + .bat) → `dist/` ; lancer `tdp.exe` : TUI (blocs Unicode, couleurs, titre), 8 touches, guitare → son distordu, BYPASS sec, MUTE silence (VU IN continue), pied de page `OK`, `q` quitte ; DLL absente → message.
  - Validation: **[ici]** chaîne Linux + diff + revue. **[session-Win]** E2E complète.

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
- **Validation split** : les tâches marquées **[ici]** sont cochées après validation sur cette machine ; celles marquées **[session-Win]** sont cochées seulement après validation en session Windows (ne pas cocher au feeling).
