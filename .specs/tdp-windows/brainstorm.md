# TDP Windows — version compilée (console Windows native, sans ncurses) Brainstorm

Spec: `tdp-windows`  
Status: brainstorm-complete  
Created: 2026-10-11

## User Intent

Brainstorm pour une version Windows COMPILÉE du TDP (option A de la discussion Windows), distincte de la V1 native Linux (spec tdp-v1, en attente de T-008).

## User intent
Produire un vrai `tdp.exe` de terminal Windows qui réutilise le cœur DSP/params déjà portable et remplace ncurses par la console Windows native (A1), avec un rendu amélioré : blocs Unicode (█▓░), couleurs, titre de fenêtre (B). Build double : MSYS2 (principal) + .bat MinGW (secours) (C), aboutissant à un `build_windows.bat` qui produit un dossier `dist/` (tdp.exe + portaudio.dll + README) (option B).

## Target users/stakeholders
L'auteur (dev Windows) et tout utilisateur Windows voulant lancer la pédale dans un terminal natif, sans WSL2.

## Problem and desired outcome
La V1 (spec tdp-v1) est native Linux (ncurses + PortAudio). Objectif : réutiliser `src/dsp.c` et `src/params.c` (C pur / atomics) SANS modification, ne écrire que la couche UI pour console Windows, et obtenir `tdp.exe` + `portaudio.dll` qui se lancent dans un terminal Windows et reproduisent le comportement de la V1 (DSP tanh, VU RMS/peak, contrôles clavier, bypass/mute, pied de page sr/buffer/statut).

## In scope
- Couche UI Windows SANS ncurses : `ui_windows.c` — rendu console Windows (SetConsoleCursorPosition / séquences ANSI / SetConsoleTextAttribute), blocs Unicode █▓░, SetConsoleTitle (ex. "TDP — Gain 50% Master 75% [ON]"), même layout que la TUI V1 (en-tête ON/BYPASS, sélection >, jauges 12 cases, VU 20 cases + dB, pied de page).
- SetConsoleOutputCP(CP_UTF8) pour l'Unicode ; repli sans couleurs sur Windows 7 / terminal non ANSI.
- Capture clavier Windows (ReadConsoleInput ou lecture non bloquante), même mapping que V1 : ESPACE(bypass), M(mute), ↑/↓(cycle param), ←/→(±0.05 clamp), R(reset 0.5/0.75), q/CTRL-C(quit).
- Réutilisation de src/dsp.c, src/params.c SANS modification ; src/audio.c PortAudio tel quel (fallback sr/buffer 48000/256 → 44100/256 → 48000/128).
- Contraintes RT préservées (pas de malloc/I-O/lock dans le callback audio).
- Build : Makefile MSYS2/MinGW64 (principal) + build_windows.bat (secours MinGW direct) → dossier dist/ (tdp.exe, portaudio.dll, README.md).
- README section Windows (build MSYS2 + .bat + run + WSL2 déjà présent en V1).

## Out of scope
- ncurses sous Windows (rejeté au profit de la console native).
- WASM / browser.
- CI, empaquetage/distribution au-delà de dist/, installeur.
- Nouveau DSP, presets, tone control, post-gain, stéréo (tout ce qui est hors V1).
- Portage du cœur dsp.c/params.c : déjà portable, inchangé.

## Constraints/risks
- PortAudio sous Windows exige la DLL portaudio.dll à côté de tdp.exe (à copier dans dist/ via build_windows.bat).
- Nom de la DLL sous MSYS2 : libportaudio-0.dll vs portaudio.dll → à confirmer au build.
- Unicode console : SetConsoleOutputCP(CP_UTF8) ; risque de blocs non affichés sur certains terminaux → repli ASCII propre.
- Couleurs ANSI = Windows 10+ ; Windows 7 → repli sans couleurs.
- Latence audio WASAPI/MME dépend du driver (cible ≤ 10 ms, à vérifier en session Windows).
- LE BUILD NE PEUT PAS ÊTRE VALIDÉ ICI (pas de Windows/MinGW sur cette machine) : la validation de compilation/exécution se fait en session sur Windows. Ici on ne peut que vérifier la cohérence du code et re-valider le cœur DSP (déjà testé via make test/check).

## Proposed approach
1. Brainstorm (ceci) → requirements.md → design.md → tasks.md (spec tdp-windows, gates séparés).
2. Implémentation : ui_windows.c (nouveau) + main.c avec #ifdef _WIN32 (factorisation UI derrière ui_* pour ne pas dupliquer la logique clavier/rendu) + Makefile MSYS2 + build_windows.bat + README.
3. Validation : compilation MSYS2/MinGW SUR WINDOWS (en session). Ici : re-valider le cœur DSP (make test/check) + revue de cohérence du code Windows.

## Assumptions
- src/dsp.c et src/params.c restent inchangés et portables (C11 pur, atomics, math.h).
- PortAudio Windows = même API que Linux (Pa_OpenStream, PaStreamCallback signature identique), seule la DLL change.
- Le build Windows est effectué par l'utilisateur (pas de CI), via MSYS2 (principal) ou le .bat (secours).
- Un seul main.c avec #ifdef _WIN32 plutôt qu'un main_windows.c duplicata (à confirmer au design).

## Remaining doubts
- Facteurisation : un seul main.c avec #ifdef _WIN32 (recommandé) vs main_windows.c séparé → design.
- Nom exact de la DLL PortAudio sous MSYS2/MinGW → design/build.
- Interface ui_* : quelle surface (init/shutdown/clear/cursor/addstr/setcolor/title/pollkey) → design.

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
