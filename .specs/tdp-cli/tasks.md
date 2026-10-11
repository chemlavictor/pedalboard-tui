# Tasks: TDP CLI — shell de commandes

Status: tasks-draft
Date: 2026-07-09
Spec: .specs/tdp-cli
Requirements: .specs/tdp-cli/requirements.md
Design: .specs/tdp-cli/design.md

## Execution Notes
- Phase 1 : catalogue de pédales + DSP + tests (cœur, testable sans carte son).
- Phase 2 : shell de commandes + parser .tdp + tests.
- Phase 3 : audio (start/stop, device, record) + enregistrement WAV.
- Phase 4 : status/watch + portabilité Windows.
- Chaque tâche : implémenter, valider (`make test`), checker la box.
- Contrainte RT : le callback audio ne fait ni malloc, ni I/O, ni locks.

## Tasks

- [ ] 1.1 Structure du catalogue + lookup
  - ID: T-001
  - Requirement(s): FR-007
  - Design: Component 2 (pedals.h)
  - Files: include/pedals.h, src/pedals.c
  - Steps:
    - Créer `include/pedals.h` : `pedal_param`, `pedal_def`, `pedal_state`, `chain_t`, `pedal_find()`, `pedals_all()`.
    - Créer `src/pedals.c` : tableau `const pedal_def` (5 pédales), `pedal_find(name)`.
  - Validation: compile ; `pedal_find("distortion")` → bonne def ; `pedal_find("nope")` → NULL.

- [ ] 1.2 Fonctions DSP des 5 pédales
  - ID: T-002
  - Requirement(s): FR-005
  - Design: Component 2 (fonctions DSP)
  - Files: src/pedals.c, include/pedals.h
  - Steps:
    - `booster` (x*=level), `distortion` (tanh(gain*x)), `noise_gate` (RMS<seuil→0), `volume` (x*=level), `mute` (x=0).
    - `noise_gate` : RMS du buffer, si < threshold (linéaire depuis dB) → zéro tout.
  - Validation: unit tests sur buffers synthétiques (chaque pédale).

- [ ] 1.3 Tests unitaires catalogue + DSP
  - ID: T-003
  - Requirement(s): FR-005, FR-007
  - Design: Testing Strategy
  - Files: tests/test_dsp.c, Makefile
  - Steps:
    - Catalogue : chaque pédale trouvée ; params min<max, def dans [min,max].
    - DSP : booster, distortion, noise_gate, volume, mute.
    - Chaîne complète : booster→distortion→volume → sortie attendue.
  - Validation: `make test` + `make check` (Valgrind) passent.

- [ ] 2.1 Shell de base (interactif + script)
  - ID: T-004
  - Requirement(s): FR-001, FR-009
  - Design: Component 1 (shell.c)
  - Files: src/shell.c, src/main.c
  - Steps:
    - `src/shell.c` : boucle `fgets` (stdin ou `FILE*` script), dispatch.
    - `help` (vue d'ensemble : intro, commandes, exemple, pédales depuis `pedals_all()`), `quit`/`exit`, boucle principale.
    - `main.c` : appeler le shell (remplacer la boucle TUI).
  - Validation: `./tdp` → `tdp> ` ; `help` affiche la vue d'ensemble ; `quit` sort.

- [ ] 2.2 Parser .tdp + commande load
  - ID: T-005
  - Requirement(s): FR-004
  - Design: Component 1 + Data Flow
  - Files: src/shell.c, src/pedals.c, include/pedals.h
  - Steps:
    - Parser ligne par ligne : ignorer `#`/vides ; `nom [param=valeur ...]`.
    - `pedal_find(name)` ; NULL → erreur (ligne N). Params clampés si hors plage.
    - Remplir `chain_t` (statique) ; `chain_idx` atomique.
    - `load <fichier>` + `chain` (afficher).
  - Validation: `load guitar.tdp` ; `chain` affiche ; fichier absent → erreur ; pédale inconnue → erreur (ligne N).

- [ ] 2.3 Tests parser .tdp
  - ID: T-006
  - Requirement(s): FR-004
  - Design: Testing Strategy
  - Files: tests/test_dsp.c, Makefile
  - Steps:
    - .tdp valide → chaîne correcte ; ligne invalide → erreur ; commentaires/vides ignorés ; param hors plage clampé.
    - Fichiers de test temporaires (créés/nettoyés).
  - Validation: `make test` passe.

- [ ] 3.1 Adapter audio.c pour la chaîne + sélection device
  - ID: T-007
  - Requirement(s): FR-002, FR-003
  - Design: Component 3 (audio.c)
  - Files: src/audio.c, include/audio.h
  - Steps:
    - Callback PortAudio applique la chaîne (lit `chain_idx` atomique).
    - `audio_set_input`/`audio_set_output`/`audio_start`/`audio_stop`/`audio_list_devices`.
    - VU (RMS/peak) in/out → atomiques. **RT-safe** : pas de malloc/I/O/locks.
  - Validation: compile ; `in`/`out` listent ; `start`/`stop`.

- [ ] 3.2 Commandes in/out/start/stop dans le shell
  - ID: T-008
  - Requirement(s): FR-002, FR-003
  - Design: Commands
  - Files: src/shell.c, src/audio.c
  - Steps:
    - `in`/`out` (liste + sélection), `start` (double → avertissement), `stop`.
    - `start` sans in/out → devices par défaut. Aucune carte → message, pas de crash.
  - Validation: manuel (carte son) : in/out/start/stop ; EC-001 (pas de carte) géré.

- [ ] 3.3 Enregistrement (ring buffer + record + WAV)
  - ID: T-009
  - Requirement(s): FR-006
  - Design: Component 3 + Component 4
  - Files: src/wav.c, include/wav.h, src/audio.c, src/shell.c
  - Steps:
    - Ring buffer statique (taille fixe, ~60 s mono 48 kHz) ; callback y écrit si `recording`.
    - `record` → `recording=true` (sans flux → avertissement).
    - Entrée (vide) → `recording=false` + `wav_write(recordings/<ts>.wav)`.
    - `src/wav.c` : encodeur WAV PCM 16-bit mono. Nom horodaté ; créer `recordings/`.
  - Validation: `record`+Enter → WAV valide (en-tête + durée) ; EC-004 géré.

- [ ] 3.4 Tests WAV + chaîne complète
  - ID: T-010
  - Requirement(s): FR-005, FR-006
  - Design: Testing Strategy
  - Files: tests/test_dsp.c, Makefile
  - Steps:
    - `wav_write` : buffer → WAV ; vérifier en-tête (PCM, sample rate, taille) + samples.
    - Chaîne + VU : buffer → chaîne → VU cohérent.
  - Validation: `make test` + `make check` passent (0 fuite).

- [ ] 4.1 status + watch
  - ID: T-011
  - Requirement(s): FR-008
  - Design: Component 1 + Data Flow
  - Files: src/shell.c
  - Steps:
    - `status` : RMS/peak in/out (dB) + pédales + état (flux/record).
    - `watch` (on/off) : affichage périodique (~100 ms, `\r`).
    - Résoudre OQ-001 (watch ne bloque pas la lecture).
  - Validation: `status` affiche ; `watch on/off` sans bloquer.

- [ ] 4.2 Portabilité Windows (make dist) + retrait TUI
  - ID: T-012
  - Requirement(s): FR-010
  - Design: FR-010
  - Files: Makefile, src/main.c
  - Steps:
    - `make dist` compile la CLI (plus de `ui_windows.c`/`ui_ncurses.c` requis).
    - `portaudio.dll` copiée dans `dist/` (Makefile déjà corrigé).
    - `main.c` n'utilise plus `ui_*`.
    - Tester `make` (Linux) + `make dist` (Windows).
  - Validation: `make` (Linux) OK ; `make dist` (Windows) → `tdp.exe` + `portaudio.dll` ; `./tdp.exe` lance le shell.

- [ ] 4.3 Tests script + Valgrind final + README
  - ID: T-013
  - Requirement(s): FR-001
  - Design: Testing Strategy
  - Files: tests/, README.md
  - Steps:
    - `script.txt` (load, start, record, vide, stop, quit) → non-interactif → propre, produit le WAV.
    - `make check` (Valgrind) → 0 fuite, 0 erreur.
    - Mettre à jour le README (mode CLI, commandes, pédales, enregistrement).
  - Validation: `make test` + `make check` ; script propre ; README à jour.

## Traceability Checklist
- [ ] Requirements IDs (FR-001..FR-010) sont référencés par les tâches.
- [ ] La stratégie de test du design est représentée par des tâches.
- [ ] Chaque tâche a une validation (commande ou check manuel).
