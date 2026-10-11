# Requirements: TDP CLI — shell de commandes

## Summary
Remplacer le TUI (ncurses / console) par un **shell de commandes CLI** : `./tdp.exe [script]`. L'utilisateur pilote l'audio par des commandes texte (`in`, `out`, `start`, `stop`, `record`, `load`, `status`, `watch`). La chaîne de pédales (booster, distortion, noise_gate, volume, mute) est chargée depuis des fichiers `.tdp` et traitée en série dans le thread audio temps réel. Un catalogue de pédales en C (`pedals.c/h`) est lu au runtime et sert de source de vérité pour générer de nouvelles pédales via LLM. L'enregistrement capture le signal après les pédales et le sauvegarde en WAV horodaté.

**Contexte** : le TUI a engendré trop de bugs (clavier Windows, Unicode, sélection de device). La CLI est plus simple, 0 bug d'interface, portable Windows/Linux.

## Goals
- G1 : Shell de commandes interactif (`tdp>`) + exécution d'un script (ligne par ligne).
- G2 : Chaîne de pédales en série, chargée depuis un `.tdp`, traitée en temps réel.
- G3 : Contrôle du flux audio : `start`/`stop`, sélection d'entrée/sortie.
- G4 : Enregistrement du signal post-pédales en WAV horodaté (`record` + Enter).
- G5 : Catalogue de pédales en C, lu au runtime (`pedals`, `info`), source de prompt LLM.
- G6 : Portable Windows + Linux (même code, même build `make` / `make dist`).

## Non-Goals
- Ampli High-Gain, Wah, Delay, Reverb (V2+).
- Plugin dynamique / interpréteur DSP (ajout de pédale = recompile).
- TUI ncurses (remplacé ; le code V1 reste en git history).
- WASM / navigateur, multi-pistes, plugins VST.

## User Stories

### US-001: Jouer avec ma chaîne de pédales
En tant que **guitariste**, je veux charger ma chaîne de pédales (`booster` → `distortion` → `noise_gate` → `volume`) depuis un fichier `.tdp` et la lancer (`start`), afin de m'entendre saturé proprement au casque sans bug d'interface.

### US-002: Enregistrer mon son
En tant que **guitariste**, je veux `record` puis presser Entrée pour sauvegarder mon passage en WAV horodaté, afin de réécouter mes riffs sans matériel d'enregistrement externe.

### US-003: Automatiser un test
En tant que **développeur**, je veux `./tdp.exe script.txt` pour exécuter une suite de commandes (load, start, record, stop) en non-interactif, afin de tester ma chaîne en batch (CI, reproductibilité).

### US-004: Générer une nouvelle pédale avec un LLM
En tant que **développeur**, je veux donner `src/pedals.c` (le catalogue) à un LLM pour qu'il me génère une nouvelle pédale (description + params + DSP), afin d'ajouter des effets sans réinventer le framework.

### US-005: Voir le niveau du signal
En tant que **guitariste**, je veux `status` (snapshot) ou `watch` (continu) pour voir le RMS/peak en dB, afin de vérifier que je n'ai pas de clipping et que le noise gate coupe bien le bruit.

### US-006: Choisir mes entrées/sorties
En tant que **guitariste**, je veux `in` / `out` pour lister et choisir mes périphériques (jack guitare, casque), afin de brancher la bonne source et la bonne destination.

## Actors
- **Utilisateur** : tape les commandes au prompt, ou fournit un script.
- **LLM (out-of-band)** : lit `src/pedals.c` pour générer de nouvelles pédales (hors runtime de TDP).

## Functional Requirements

### FR-001: Shell interactif et exécution de script
La CLI SHALL fournir un shell de commandes avec un prompt `tdp> ` en mode interactif (aucun argument), et SHALL exécuter ligne par ligne les commandes d'un fichier (script) quand un argument de chemin est fourni (`./tdp.exe script.txt`).

#### Scenario: Démarrage interactif
- **GIVEN** l'utilisateur lance `./tdp.exe` sans argument
- **WHEN** le programme démarre
- **THEN** il affiche un prompt `tdp> ` et attend une commande.

#### Scenario: Exécution d'un script
- **GIVEN** un fichier `run.tdp` contenant des commandes (une par ligne)
- **WHEN** l'utilisateur lance `./tdp.exe run.tdp`
- **THEN** chaque ligne est exécutée comme une commande, et le programme se termine à la fin du fichier.

### FR-002: Listage et sélection des périphériques audio
La CLI SHALL lister les entrées (`in`) et sorties (`out`) audio disponibles, et SHALL permettre de sélectionner un périphérique par son index (`in 1`, `out 0`).

#### Scenario: Lister les entrées
- **GIVEN** le programme est démarré
- **WHEN** l'utilisateur tape `in`
- **THEN** la liste des entrées audio est affichée avec leur index et nom.

#### Scenario: Sélectionner une sortie
- **GIVEN** la liste des sorties est connue
- **WHEN** l'utilisateur tape `out 0`
- **THEN** la sortie 0 est sélectionnée pour le prochain `start`.

### FR-003: Démarrage et arrêt du flux audio
La CLI SHALL démarrer le flux audio (`start`) qui applique la chaîne de pédales au signal d'entrée, et SHALL l'arrêter (`stop`). Les périphériques par défaut PortAudio sont utilisés si `in`/`out` n'ont pas été choisis.

#### Scenario: Démarrer le flux
- **GIVEN** une chaîne de pédales est chargée
- **WHEN** l'utilisateur tape `start`
- **THEN** le thread audio capture l'entrée, applique les pédales, et rend la sortie.

#### Scenario: Arrêter le flux
- **GIVEN** le flux est en cours
- **WHEN** l'utilisateur tape `stop`
- **THEN** le flux audio est arrêté proprement.

### FR-004: Chargement d'une chaîne de pédales (.tdp)
La CLI SHALL charger une chaîne de pédales depuis un fichier `.tdp` via `load <fichier>`. Le format est : une pédale par ligne (ordre = ordre en série), paramètres en `nom=valeur`, `#` = commentaire, ligne vide ignorée.

#### Scenario: Charger un fichier de pédales
- **GIVEN** un fichier `guitar.tdp` avec `booster level=2.0`, `distortion gain=0.6`, etc.
- **WHEN** l'utilisateur tape `load guitar.tdp`
- **THEN** la chaîne de pédales est construite dans l'ordre et prête pour `start`.

#### Scenario: Ligne invalide dans le .tdp
- **GIVEN** une ligne `unknown_pedal x=1` (pédale inconnue)
- **WHEN** `load` est exécuté
- **THEN** une erreur est affichée (nom de la pédale inconnue + numéro de ligne), et la chaîne n'est pas chargée.

### FR-005: Pédales de la V1 CLI
La chaîne SHALL implémenter les pédales suivantes, traitées en série dans le thread audio :

| Pédale | Paramètre(s) | Défaut | Rôle |
|--------|--------------|--------|------|
| `booster` | `level` (0..2) | 1.5 | Gain linéaire propre (type TS9 level) |
| `distortion` | `gain` (0..1) | 0.5 | Distorsion `tanh(G·x)` |
| `noise_gate` | `threshold` (dB) | -45 | Coupe le signal si RMS < seuil |
| `volume` | `level` (0..1) | 0.75 | Master volume de sortie |
| `mute` | — | off | Silenced total (toggle) |

#### Scenario: Booster pousse le signal
- **GIVEN** `booster level=2.0`
- **WHEN** le signal passe
- **THEN** il est multiplié par 2.0 (avant clippage éventuel).

#### Scenario: Noise gate coupe le bruit de fond
- **GIVEN** `noise_gate threshold=-40`
- **WHEN** le RMS du signal est < -40 dB
- **THEN** la sortie est forcée à 0 (silence).

### FR-006: Enregistrement en WAV horodaté
La CLI SHALL capturer le signal **après les pédales** quand `record` est actif, et SHALL le sauvegarder en WAV quand l'utilisateur appuie sur Entrée (ligne vide). Le fichier est horodaté : `recordings/YYYY-MM-DD_HH-MM-SS.wav`.

#### Scenario: Enregistrer et sauvegarder
- **GIVEN** le flux est en cours (`start` fait)
- **WHEN** l'utilisateur tape `record` puis (plus tard) appuie sur Entrée (ligne vide)
- **THEN** le signal post-pédales capturé est encodé en WAV et sauvegardé dans `recordings/` avec un nom horodaté, et l'enregistrement s'arrête.

#### Scenario: Enregistrement sans flux
- **GIVEN** le flux n'est pas démarré
- **WHEN** l'utilisateur tape `record`
- **THEN** un avertissement est affiché (rien à enregistrer sans flux).

### FR-007: Catalogue de pédales (runtime + prompt LLM)
La CLI SHALL exposer un catalogue de pédales en C (`include/pedals.h` + `src/pedals.c`) avec, pour chaque pédale : nom, description, paramètres (nom, type, min, max, défaut, unité), et fonction DSP. Les commandes `pedals` (liste) et `info <nom>` (détails) SHALL afficher ce catalogue.

#### Scenario: Lister les pédales
- **GIVEN** le programme est démarré
- **WHEN** l'utilisateur tape `pedals`
- **THEN** la liste des pédales disponibles est affichée (nom + description courte).

#### Scenario: Détails d'une pédale
- **GIVEN** la pédale `distortion` existe
- **WHEN** l'utilisateur tape `info distortion`
- **THEN** sa description, ses paramètres (min/max/défaut) sont affichés.

### FR-008: Status et watch (niveau du signal)
La CLI SHALL afficher un snapshot du niveau du signal (`status` : RMS/peak en dB, pédales actives, état record/flux) à la demande, et SHALL fournir un mode `watch` (on/off) qui affiche périodiquement le niveau.

#### Scenario: Snapshot du niveau
- **GIVEN** le flux est en cours
- **WHEN** l'utilisateur tape `status`
- **THEN** le RMS/peak en dB (entrée et sortie), la liste des pédales, et l'état (flux/record) sont affichés.

#### Scenario: Mode watch
- **GIVEN** le flux est en cours
- **WHEN** l'utilisateur tape `watch` (on)
- **THEN** une ligne de niveau se met à jour périodiquement jusqu'à `watch` (off).

### FR-009: Commandes utilitaires
La CLI SHALL fournir : `help` (liste des commandes), `quit`/`exit` (quitter proprement), `reset` (reset params aux défauts), `chain` (afficher la chaîne chargée).

### FR-010: Portabilité Windows + Linux
La CLI SHALL être compilable et fonctionnelle sur Windows (MinGW/MSYS2, `make dist` → `tdp.exe` + `portaudio.dll`) et Linux (`make`), avec le même code source.

## Acceptance Criteria
- **AC-001** : `./tdp.exe` affiche un prompt `tdp> ` et accepte les commandes (interactif).
- **AC-002** : `./tdp.exe script.txt` exécute le script ligne par ligne puis se termine.
- **AC-003** : `in` / `out` listent les périphériques ; `in N` / `out N` sélectionnent.
- **AC-004** : `start` / `stop` démarrent/arrêtent le flux (pédales appliquées).
- **AC-005** : `load x.tdp` charge la chaîne ; une ligne invalide est signalée.
- **AC-006** : `record` + Entrée produit `recordings/<horodatage>.wav` (signal post-pédales).
- **AC-007** : `pedals` / `info X` affichent le catalogue.
- **AC-008** : `status` / `watch` affichent le niveau du signal.
- **AC-009** : `make test` passe (tests DSP + catalogue, sans carte son).
- **AC-010** : `make` (Linux) et `make dist` (Windows) compilent sans erreur.

## Edge Cases
- **EC-001** : Aucune carte audio → message lisible, pas de crash ; `start` échoue proprement.
- **EC-002** : Fichier `.tdp` absent → erreur lisible.
- **EC-003** : Param hors plage (ex. `gain=5`) → clampé au max, avertissement.
- **EC-004** : `record` pendant que le flux n'est pas en cours → avertissement.
- **EC-005** : Script avec une commande inconnue → erreur affichée, le script continue (ou s'arrête — à trancher au design).
- **EC-006** : Double `start` (déjà en cours) → avertissement, pas de double stream.
- **EC-007** : Entrée (Enter) sans `record` actif dans le script → ignorée (pas de faux enregistrement).

## Constraints
- **C-001** : C11, threads (audio RT + main), `stdatomic` (pas de locks dans le callback audio).
- **C-002** : PortAudio pour l'audio (capture/lecture), encodeur WAV maison (PCM 16-bit mono/stéréo).
- **C-003** : Le callback audio ne fait ni `malloc`/`free`, ni I/O, ni locks (mémoire pré-allouée : ring buffer d'enregistrement de taille fixe).
- **C-004** : L'UI est du texte stdout/stdin simple (pas de ncurses, pas de console API custom) — portable.
- **C-005** : Le catalogue de pédales (`pedals.c/h`) est la source de vérité ; une nouvelle pédale s'ajoute par edit + `make` (recompile), pas par plugin.
- **C-006** : Windows : `portaudio.dll` doit être copiée dans `dist/` (le Makefile le gère).
