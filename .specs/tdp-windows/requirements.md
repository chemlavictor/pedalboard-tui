# TDP Windows — version compilée (console Windows native, sans ncurses) Requirements

Spec: `tdp-windows`  
Status: requirements-approved  
Created: 2026-10-11  
Brainstorm: `./brainstorm.md`  
Source de vérité technique: `specs.md` + spec sœur `tdp-v1`

## Overview

Produire une **version Windows compilée** du TDP : un `tdp.exe` de terminal natif qui réutilise le cœur DSP/params déjà portable (`src/dsp.c`, `src/params.c` **inchangés**) et **remplace ncurses** par une couche UI console Windows (`ui_windows.c`) avec blocs Unicode, couleurs et titre de fenêtre. Le build (MSYS2 principal + `build_windows.bat` secours MinGW) produit un dossier `dist/` contenant `tdp.exe`, `portaudio.dll` et un README. Le comportement (DSP, VU, contrôles clavier, bypass/mute, pied de page) doit reproduire la V1 (`tdp-v1`).

## Scope

### In Scope

- Couche UI Windows **sans ncurses** : `src/ui_windows.c` + `include/ui.h` — rendu console Windows, blocs Unicode, couleurs, titre de fenêtre, capture clavier.
- `src/main.c` : branchement de l'UI via une interface `ui_*` derrière `#ifdef _WIN32` (la logique clavier/rendu reste commune, seule l'implémentation UI diffère ncurses vs Windows).
- Réutilisation **sans modification** de `src/dsp.c`, `src/params.c` et de `src/audio.c` (PortAudio).
- Build Windows : `Makefile` MSYS2/MinGW64 (principal) + `build_windows.bat` (secours MinGW direct) → `dist/` (`tdp.exe`, `portaudio.dll`, `README.md`).
- README : section Windows (build MSYS2 + `.bat` + run), en complément de la section WSL2 déjà présente.
- Préservation des contraintes RT (pas de malloc/I-O/lock dans le callback audio).

### Out of Scope

- ncurses sous Windows (rejeté au profit de la console native).
- WASM / browser.
- CI, installeur, empaquetage/distribution au-delà du dossier `dist/`.
- Toute évolution du DSP (nouveaux waveforms, tone, post-gain, stéréo, presets) — hors V1.
- Portage/modification de `src/dsp.c` et `src/params.c` (déjà portables).
- Validation de la compilation/exécution **sur cette machine** (pas de Windows/MinGW ici) : le build et le run sont validés **en session sur Windows**.

## User Stories

- En tant que guitariste sur Windows, je veux lancer `tdp.exe` dans un terminal et jouer, afin d'avoir ma distorsion sans installer WSL2.
- En tant que guitariste sur Windows, je veux la même interface que la version Linux (jauges, VU, ON/BYPASS, pied de page), afin d'être dans mon confort.
- En tant que développeur, je veux `build_windows.bat` (ou MSYS2) qui me sort un `dist/` avec `tdp.exe` + `portaudio.dll`, afin de lancer la pédale en un clic.
- En tant que développeur, je veux que le cœur DSP/params soit réutilisé à l'identique, afin de garantir le même son que la V1 Linux.

## Functional Requirements

Use stable IDs and testable EARS-style statements.

### Cœur réutilisé

- **FR-001**: THE system SHALL réutiliser `src/dsp.c` et `src/params.c` **sans modification** (même chaîne `tanh(G·x)` · `V`, mêmes VU RMS/peak-hold, mêmes atomics).
- **FR-002**: THE system SHALL réutiliser `src/audio.c` (PortAudio) avec la même logique de fallback sr/buffer (48000/256 → 44100/256 → 48000/128) et les mêmes contraintes RT.
- **FR-003**: THE thread audio callback SHALL respecter les contraintes RT (pas de malloc/free, pas d'I-O/printf, pas de lock, lecture params via `stdatomic`), identiquement à la V1.

### UI console Windows (sans ncurses)

- **FR-004**: THE system SHALL implémenter le rendu TUI dans `src/ui_windows.c` **sans ncurses**, en utilisant les API console Windows (`SetConsoleCursorPosition`, écriture, et/ou `SetConsoleTextAttribute`).
- **FR-005**: THE system SHALL activer la sortie Unicode via `SetConsoleOutputCP(CP_UTF8)` et afficher les jauges/VU en **blocs Unicode** (`█`, `▓`, `░`).
- **FR-006**: THE system SHALL utiliser les couleurs (couche textuelle de Windows 10+, séquences ANSI ou `SetConsoleTextAttribute`) pour surligner l'état `BYPASS` et le paramètre sélectionné ; si le terminal ne supporte pas la couleur, il SHALL dégrader proprement en rendu monochrome.
- **FR-007**: THE system SHALL mettre à jour le **titre de la fenêtre** console via `SetConsoleTitle` (ex. `TDP — Gain 50% Master 75% [ON]` / `[BYPASS]`).
- **FR-008**: THE system SHALL afficher la même mise en page que la V1 : en-tête `TDP - Terminal Distortion Pedal` + `[ON]`/`[BYPASS]`, ligne du paramètre sélectionné avec préfixe `> `, jauges de 12 cases + `%`, VU INPUT/OUTPUT de 20 cases + `dB` (floor -60 dB) + marquage de crête, et pied de page `<sr> Hz · buffer <N> · PortAudio OK/ERREUR`.
- **FR-009**: THE system SHALL capturer le clavier de façon **non bloquante** (ex. `ReadConsoleInput` avec `PeekConsoleInput` / délai), à une cadence d'environ 30–60 fps.
- **FR-010**: THE system SHALL supporter les mêmes raccourcis que la V1 : `ESPACE` (bypass), `M` (mute), `↑`/`↓` (cycle Gain↔Master), `←`/`→` (±0.05, clamp [0,1]), `R` (reset 0.5/0.75), `q`/`CTRL-C` (quit propre).

### Comportement audio (parité V1)

- **FR-011**: IF BYPASS est actif, THE system SHALL router l'entrée directement en sortie (signal sec).
- **FR-012**: IF MUTE est actif, THE system SHALL mettre la sortie à 0 (le DSP continue, le VU d'entrée continue).
- **FR-013**: THE system SHALL calculer les VU (RMS + peak-hold 500 ms) dans le thread audio et les publier via les atomics, comme la V1.

### Build Windows

- **FR-014**: THE system SHALL fournir un `Makefile` (ou équivalent) pour **MSYS2/MinGW64** qui compile `tdp.exe` avec les flags C11 + pthread/atomics + PortAudio + m.
- **FR-015**: THE system SHALL fournir un `build_windows.bat` (secours MinGW direct) qui compile `tdp.exe` et assemble le dossier `dist/`.
- **FR-016**: THE `dist/` target SHALL contenir `tdp.exe`, `portaudio.dll` (la DLL PortAudio du toolchain) et un `README.md` de lancement.
- **FR-017**: IF la DLL PortAudio n'est pas trouvée à côté de `tdp.exe`, THE lancement SHALL échouer proprement avec un message d'erreur lisible (pas de crash silencieux).

### Documentation

- **FR-018**: THE `README.md` SHALL documenter le build et le lancement Windows : installation MSYS2 + PortAudio (principal), utilisation de `build_windows.bat` (secours), contenu de `dist/`, et rappel de la piste WSL2 (déjà présente).

## Non-Functional Requirements

- **NFR-001**: THE system SHALL viser une latence audio ≤ 10 ms (buffer 128/256), à valider en session selon le driver (WASAPI/MME).
- **NFR-002**: THE system SHALL compiler sans warnings avec `-Wall -Wextra` sous MinGW/MSYS2.
- **NFR-003**: THE system SHALL rester C11 portable (pas d'appel POSIX exotique dans le cœur DSP) ; les appels Windows sont confinés à `ui_windows.c`.
- **NFR-004**: THE system SHALL fonctionner sur Windows 10/11 (couleurs ANSI + Unicode) ; sur Windows 7, repli sans couleurs.
- **NFR-005**: LE build Windows ne peut PAS être validé sur la machine de développement (pas de Windows/MinGW) : la validation de compilation/exécution est faite **en session sur Windows** ; ici, seule la cohérence du code et le cœur DSP (`make test`/`make check` de la V1) sont vérifiables.

## Acceptance Criteria

- [ ] `src/dsp.c` et `src/params.c` sont byte-identiques à la V1 (aucune modification).
- [ ] `ui_windows.c` compile sous MSYS2/MinGW64 **sans ncurses** et produit `tdp.exe`.
- [ ] `build_windows.bat` (ou le Makefile MSYS2) génère `dist/tdp.exe` + `dist/portaudio.dll` + `dist/README.md`.
- [ ] Sur Windows : lancer `dist\tdp.exe` ouvre la TUI console (blocs Unicode, couleurs, titre fenêtre) et répond aux 8 raccourcis clavier.
- [ ] Sur Windows avec carte son : brancher la guitare → jouer → son distordu en sortie ; BYPASS = signal sec ; MUTE = silence (VU IN continue).
- [ ] Le pied de page affiche le sr/buffer réel + `PortAudio OK`.
- [ ] Absence de la DLL PortAudio → message d'erreur lisible (FR-017).
- [ ] README section Windows à jour (build MSYS2 + .bat + dist + WSL2).
- [ ] Audit RT du callback inchangé (FR-003).

## Edge Cases

- Terminal Windows sans support ANSI/couleur (Windows 7, ou console désactivée) : repli monochrome, pas de crash.
- Console trop petite pour la mise en page : rendu tronqué proprement (comme la V1).
- Absence de `portaudio.dll` à côté de l'exe : erreur lisible au démarrage.
- Carte son absente/multiple : PortAudio choisit le périphérique par défaut ; pied de page `ERREUR` si aucun.
- `CTRL-C` en cours de stream : fermeture ordonnée (stop stream + restaurer console), exit 0.
- Blocs Unicode non affichés (police/terminal) : repli ASCII acceptable, l'app reste utilisable.

## Dependencies and Constraints

- MSYS2 + `mingw-w64-x86_64-gcc` + `mingw-w64-x86_64-portaudio` (principal), ou MinGW `gcc` + PortAudio headers/lib (secours `.bat`).
- PortAudio Windows : DLL `portaudio.dll` / `libportaudio-0.dll` (nom à confirmer au design) à copier dans `dist/`.
- `src/dsp.c`, `src/params.c`, `src/audio.c` issus de la V1 (spec `tdp-v1`), réutilisés sans modification.
- Séquencing : interface `ui.h` → `ui_windows.c` → branchement `main.c` (`#ifdef _WIN32`) → Makefile MSYS2 → `build_windows.bat` → README.
- **Contrainte de validation** : compilation et run Windows validés uniquement en session sur Windows.

## Open Questions

- [ ] `main.c` : un seul fichier avec `#ifdef _WIN32` (recommandé) ou `main_windows.c` séparé ? → design.
- [ ] Surface de l'interface `ui_*` (init/shutdown/clear/cursor/addstr/setcolor/settitle/pollkey) ? → design.
- [ ] Nom exact de la DLL PortAudio sous MSYS2/MinGW (`portaudio.dll` vs `libportaudio-0.dll`) et mécanisme de copie dans `dist/` ? → design/build.
- [ ] Mécanisme de clavier non bloquant : `ReadConsoleInput`+`PeekConsoleInput` vs `SetConsoleMode`+`ReadConsole` en thread ? → design.
