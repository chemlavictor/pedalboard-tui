# AGENT.md — règles pour les agents (AI / humains)

Règles à suivre **à chaque commit et pour l'organisation du travail** dans ce dépôt.

## 1. Commits atomiques

- **Un commit = un changement logique unique** (une fonctionnalité, un bugfix, un refactor, une doc).
- Ne jamais mélanger dans un même commit : du code + de la doc, un fix + un refactor, ou plusieurs features indépendantes.
- Chaque commit doit être **compilable et testable seul** (`make && make test`) : on peut `git revert` ou `git bisect` sans casse.
- Si une modification touche plusieurs domaines (ex. `src/pedals.c` + un preset `.tdp` + la doc), la séparer en plusieurs commits.
- Les petits pas rendent le `git log` lisible et la revue facile.

## 2. Push après de grosses modifications

- **Après chaque série de commits non-négligeables (feature complète, refactor important, doc substantielle), faire un `git push`.**
- Objectif : ne jamais perdre du travail important sur la machine locale.
- En cas d'interruption (session d'agent coupée, crash), le travail pushé est en sécurité.
- Les petits ajustements incrémentaux n'obligent pas à push à chaque ligne, mais un bloc de travail terminé = push.

## 3. Format du message de commit

Le message de commit **doit** respecter ces règles :

- La première ligne est le **sujet**, commençant par un verbe à l'impératif (ex. "Fix", "Add", "Refactor").
- Le sujet fait **50 caractères maximum**.
- Le commit commence par **"<emoji> <action>"**, exemple : "🐛 Fix:".
- Si nécessaire, ajouter une **ligne blanche** suivie d'un **corps** plus long et détaillé.
- Le corps peut contenir **paragraphes et listes**.
- Les listes du corps utilisent des **puces à tirets (-)**.
- Le corps est **replié à 72 caractères** par ligne.
- **AUCUN formatage Markdown** (pas de `**gras**`, pas de `` `code` ``, pas de titres `#`, pas de liens `[texte](url)`).

### Guide d'emoji

- 🐛 Fix: pour les corrections de bug.
- ✿ Feat: pour les nouvelles fonctionnalités.
- 📘 Docs: pour les changements de documentation.
- ♻️ Refactor: pour les refactorisations sans changement de comportement.
- ⚡ Perf: pour les améliorations de performance.
- 🔒 Security: pour les correctifs liés à la sécurité.
- 🔧 Chore: pour les tâches de maintenance.

### Exemples

Exemple simple (sujet seul) :

```
🐛 Fix: resolve buffer overflow in ring buffer
```

Exemple avec corps (sujet + ligne blanche + corps replié à 72) :

```
✿ Feat: add wah pedal to the catalog

- Add wah_process() with a swept bandpass filter (200-1000 Hz).
- Register the pedal in src/pedals.c with a sweep parameter.
- Add a son_wah.tdp preset.
- Update the pedal table in docs/documentation.md.

The sweep parameter is 0..1 and maps to the filter center
frequency. Tests in tests/test_dsp.c cover the DSP bounds.
```

Exemple de maintenance :

```
🔧 Chore: update MSYS2 dependencies in install script

- Replace deprecated pacman package names.
- Add unzip to the base toolchain.
```

Exemple de documentation :

```
📘 Docs: document the audio thread model
```

### Ce qui est interdit dans un message de commit

- Pas de formatage Markdown : `**`, `__`, `*`, `` ` ``, `##`, `[texte](url)`.
- Sujet > 50 caractères.
- Sujet ne commençant pas par l'emoji + l'action à l'impératif.
- Corps avec des lignes > 72 caractères (hors mots indivisibles).
- Puces avec `*` ou `+` : toujours `-`.

## 4. Rappels projet

- Build & tests : `make && make test` (sans carte son). `make check` sous Valgrind.
- Le catalogue de pédales (`src/pedals.c`) est la source de vérité : une nouvelle pédale = une entrée dans ce fichier + recompile.
- Contraintes du thread audio (temps réel) : pas de `malloc`/`free`, pas d'I/O, pas de lock ; mémoire statique, `stdatomic` lock-free uniquement. Tout changement dans `src/audio.c` doit respecter ces contraintes.
- **Règle obligatoire** : `README.md` (racine) et `docs/documentation.md`
  doivent être **maintenus à jour à chaque commit** qui change un comportement
  utilisateur visible. Si le commit touche l'un des points ci-dessous, la doc
  est mise à jour **dans le même commit** (pas en différé, pas dans un
  commit séparé) :
  - une nouvelle commande du shell, ou un changement d'arg/alias ;
  - une nouvelle pédale, un param, une plage, un défaut ;
  - un changement d'installation, de build, de dépendance ;
  - un changement de comportement audio (sample rate, buffer, thread, VU) ;
  - un déplacement / ajout / suppression de fichier ou de dossier (l'arbre
    du projet dans le README est mis à jour dans le même commit) ;
  - un changement de format de fichier (`.tdp`, WAV, historique).
- Après un commit, vérifier que les exemples de commandes de la doc
  (`load`, `in`, `out`, …) reflètent les chemins et noms actuels. Un exemple
  qui pointe vers un fichier qui n'existe plus est un bug de doc.
- Voir `specs.md` pour les spécifications techniques et
  `docs/documentation.md` pour la doc utilisateur.
