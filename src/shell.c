/* Shell de commandes TDP CLI.
 * Mode interactif : prompt "tdp> " sur stdin.
 * Mode script : lit un fichier ligne par ligne, pas de prompt, termine a la fin.
 * Portable Windows + Linux : uniquement C standard (fgets/printf/fopen/time/strftime/mkdir).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "pedals.h"
#include "audio.h"
#include "wav.h"

#define LINE_MAX 512

/* Chaine de pedales chargee (construite par `load`). */
static chain_t g_chain = {0};

/* Etat audio partagé + ring buffer d'enregistrement (statiques, RT-safe). */
static audio_shared g_sh;
static rec_ring g_ring;
static float g_rec_buf[TDP_REC_CAPACITY]; /* ~60 s mono 48 kHz */

/* Handlers des commandes (implémentés plus bas ou dans d'autres modules).
 * Retourne -1 pour demander la sortie du shell. */
typedef int (*cmd_fn)(int argc, char **argv);

static int cmd_help(int argc, char **argv);
static int cmd_quit(int argc, char **argv);
static int cmd_pedals(int argc, char **argv);
static int cmd_info(int argc, char **argv);
static int cmd_load(int argc, char **argv);
static int cmd_chain(int argc, char **argv);
static int cmd_reset(int argc, char **argv);
static int cmd_in(int argc, char **argv);
static int cmd_out(int argc, char **argv);
static int cmd_start(int argc, char **argv);
static int cmd_stop(int argc, char **argv);
static int cmd_record(int argc, char **argv);
static int cmd_noop(int argc, char **argv);

/* Table de dispatch. Les commandes audio (in/out/start/stop/record/load...)
 * seront branchées dans les phases suivantes (audio.c / wav.c). */
static struct { const char *name; cmd_fn fn; } g_cmds[] = {
  { "help",   cmd_help },
  { "quit",   cmd_quit },
  { "exit",   cmd_quit },
  { "pedals", cmd_pedals },
  { "info",   cmd_info },
  { "load",   cmd_load },
  { "chain",  cmd_chain },
  { "reset",  cmd_reset },
  { "in",     cmd_in },
  { "out",    cmd_out },
  { "start",  cmd_start },
  { "stop",   cmd_stop },
  { "record", cmd_record },
  /* placeholders (implémentés en phase 4) : */
  { "status", cmd_noop },
  { "watch",  cmd_noop },
};
static const int N_CMDS = (int)(sizeof g_cmds / sizeof g_cmds[0]);

/* ------------------------------------------------------------------ */
/* help : vue d'ensemble                                               */
/* ------------------------------------------------------------------ */
static int cmd_help(int argc, char **argv) {
  (void)argc; (void)argv;
  printf(
    "TDP — Terminal Distortion Pedal (CLI)\n"
    "Chaine de pedales audio temps reel, pilotee par commandes.\n"
    "\n"
    "Commandes :\n"
    "  in [N]          Listes les entrees / choisit l'entree N\n"
    "  out [N]         Listes les sorties / choisit la sortie N\n"
    "  load <fichier>  Charge une chaine de pedales (.tdp)\n"
    "  chain           Affiche la chaine chargee\n"
    "  start           Demarre le flux audio (pedales appliquees)\n"
    "  stop            Arrete le flux audio\n"
    "  record          Enregistre le signal (post-pedales)\n"
    "  <Entree>        Arrete l'enregistrement + sauvegarde WAV horodate\n"
    "  status          Snapshot du niveau (RMS/peak in/out, etat)\n"
    "  watch           Affichage continu du niveau (on/off)\n"
    "  pedals          Liste les pedales disponibles\n"
    "  info <nom>      Details d'une pedale (params, min/max/defaut)\n"
    "  reset           Reset les params de la chaine aux defauts\n"
    "  help            Cette aide\n"
    "  quit            Quitter\n"
    "\n"
    "Pedales (V1) :\n");
  const pedal_def *all = pedals_all();
  for (int i = 0; all[i].name; ++i) {
    printf("  %-12s %s\n", all[i].name, all[i].desc);
  }
  printf(
    "\n"
    "Exemple :\n"
    "  tdp> in\n"
    "  tdp> out\n"
    "  tdp> load guitar.tdp\n"
    "  tdp> start\n"
    "  tdp> record\n"
    "  ... (jouer) ...\n"
    "  tdp>            <Entree> -> sauvegarde recordings/2026-07-09_15-42-33.wav\n"
    "  tdp> stop\n"
    "  tdp> quit\n");
  return 0;
}

static int cmd_quit(int argc, char **argv) { (void)argc; (void)argv; return -1; }

static int cmd_pedals(int argc, char **argv) {
  (void)argc; (void)argv;
  const pedal_def *all = pedals_all();
  printf("Pedales disponibles :\n");
  for (int i = 0; all[i].name; ++i) {
    printf("  %-12s %s\n", all[i].name, all[i].desc);
  }
  return 0;
}

static int cmd_info(int argc, char **argv) {
  if (argc < 2) { printf("usage: info <nom>\n"); return 0; }
  const pedal_def *d = pedal_find(argv[1]);
  if (!d) { printf("TDP: pedale inconnue '%s'\n", argv[1]); return 0; }
  printf("%s — %s\n", d->name, d->desc);
  int has_params = 0;
  for (const pedal_param *p = d->params; p && p->name; ++p) {
    if (!has_params) { printf("Parametres :\n"); has_params = 1; }
    printf("  %-10s = %g  [%g .. %g]%s  (%s)\n",
           p->name, p->def, p->minv, p->maxv,
           (p->unit && p->unit[0]) ? p->unit : "",
           p->desc);
  }
  if (!has_params) printf("(pas de parametre)\n");
  return 0;
}

static int cmd_noop(int argc, char **argv) {
  (void)argc; (void)argv;
  printf("(commande pas encore implementee — phase suivante)\n");
  return 0;
}

/* ------------------------------------------------------------------ */
/* Parser .tdp + load / chain / reset                                  */
/* ------------------------------------------------------------------ */

/* Clone un pedal_def avec ses params aux valeurs donnees.
 * Retourne 0 si ok, -1 si pedale inconnue ou trop de params.
 * Publique (non static) pour etre testable par tests/test_tdp.c. */
int parse_tdp_line(const char *line, int lineno, chain_t *c) {
  char buf[LINE_MAX];
  strncpy(buf, line, sizeof buf - 1);
  buf[sizeof buf - 1] = '\0';
  /* Supprime le newline. */
  buf[strcspn(buf, "\r\n")] = '\0';

  char *s = buf;
  while (*s == ' ' || *s == '\t') ++s;
  if (*s == '\0' || *s == '#') return 0; /* vide ou commentaire : ignore */

  /* Nom de la pedale. */
  char *sp = s;
  while (*sp && *sp != ' ' && *sp != '\t') ++sp;
  if (sp == s) return 0; /* pas de nom */
  char name[64];
  int nl = (int)(sp - s);
  if (nl >= (int)sizeof name) nl = sizeof name - 1;
  memcpy(name, s, nl);
  name[nl] = '\0';

  const pedal_def *def = pedal_find(name);
  if (!def) {
    printf("TDP: pedale inconnue '%s' (ligne %d)\n", name, lineno);
    return -1;
  }
  if (c->len >= MAX_CHAIN) {
    printf("TDP: chaine pleine (max %d pedales) a la ligne %d\n", MAX_CHAIN, lineno);
    return -1;
  }

  /* Initialise l'etat aux defauts du catalogue. */
  pedal_state st = {0};
  int pi = 0;
  for (const pedal_param *p = def->params; p && p->name && pi < MAX_PARAMS; ++p, ++pi) {
    st.v[pi] = p->def;
  }

  /* Parse les params nom=valeur (apres le nom). */
  char *tok = sp;
  while (*tok) {
    while (*tok == ' ' || *tok == '\t') ++tok;
    if (*tok == '\0') break;
    /* debut du token param */
    char *eq = strchr(tok, '=');
    if (!eq) {
      printf("TDP: param invalide '%s' (ligne %d)\n", tok, lineno);
      return -1;
    }
    *eq = '\0';
    const char *pname = tok;
    const char *pval = eq + 1;
    float val = (float)strtod(pval, NULL);

    int found = 0;
    for (const pedal_param *p = def->params; p && p->name; ++p) {
      if (strcmp(p->name, pname) == 0) {
        found = 1;
        /* Clamp si hors plage + avertissement. */
        if (val < p->minv) { printf("TDP: %s=%g clampé a %g (ligne %d)\n", pname, val, p->minv, lineno); val = p->minv; }
        if (val > p->maxv) { printf("TDP: %s=%g clampé a %g (ligne %d)\n", pname, val, p->maxv, lineno); val = p->maxv; }
        st.v[p - def->params] = val;
        break;
      }
    }
    if (!found) {
      printf("TDP: parametre inconnu '%s' pour '%s' (ligne %d)\n", pname, name, lineno);
      return -1;
    }
    /* Passe au token suivant. */
    tok = eq + 1;
    while (*tok && *tok != ' ' && *tok != '\t') ++tok;
  }

  /* Enregistre dans la chaine. */
  c->defs[c->len] = def;
  c->state[c->len] = st;
  c->len++;
  return 0;
}

static int cmd_load(int argc, char **argv) {
  if (argc < 2) { printf("usage: load <fichier>\n"); return 0; }
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    printf("TDP: fichier introuvable: %s\n", argv[1]);
    return 0;
  }
  chain_t c = {0};
  char line[LINE_MAX];
  int lineno = 0, err = 0;
  while (fgets(line, sizeof line, f)) {
    ++lineno;
    if (parse_tdp_line(line, lineno, &c) != 0) { err = 1; break; }
  }
  fclose(f);
  if (err) {
    printf("TDP: chaine non chargee (erreur dans %s)\n", argv[1]);
    return 0;
  }
  g_chain = c;
  printf("TDP: chaine chargee de '%s' (%d pedale%s)\n", argv[1], c.len, c.len > 1 ? "s" : "");
  return 0;
}

static int cmd_chain(int argc, char **argv) {
  (void)argc; (void)argv;
  if (g_chain.len == 0) { printf("(aucune chaine chargee — tapez 'load <fichier>')\n"); return 0; }
  printf("Chaine chargee (%d pedale%s) :\n", g_chain.len, g_chain.len > 1 ? "s" : "");
  for (int i = 0; i < g_chain.len; ++i) {
    const pedal_def *d = g_chain.defs[i];
    printf("  %d. %s", i + 1, d->name);
    int first = 1;
    for (const pedal_param *p = d->params; p && p->name; ++p) {
      printf(" %s=%g", p->name, g_chain.state[i].v[p - d->params]);
      (void)first;
    }
    printf("\n");
  }
  return 0;
}

static int cmd_reset(int argc, char **argv) {
  (void)argc; (void)argv;
  if (g_chain.len == 0) { printf("(aucune chaine a reset)\n"); return 0; }
  for (int i = 0; i < g_chain.len; ++i) {
    const pedal_def *d = g_chain.defs[i];
    int pi = 0;
    for (const pedal_param *p = d->params; p && p->name && pi < MAX_PARAMS; ++p, ++pi) {
      g_chain.state[i].v[pi] = p->def;
    }
  }
  printf("TDP: params reset aux defauts du catalogue\n");
  return 0;
}

/* ------------------------------------------------------------------ */
/* in / out / start / stop (audio)                                     */
/* ------------------------------------------------------------------ */

static int g_in_dev = -1;  /* -1 = defaut */
static int g_out_dev = -1;

static int cmd_in(int argc, char **argv) {
  if (argc >= 2) {
    g_in_dev = atoi(argv[1]);
    printf("TDP: entree %d selectionnee\n", g_in_dev);
    return 0;
  }
  /* Listage. */
  tdp_device devs[TDP_MAX_DEVICES];
  int n = audio_list_devices(devs);
  if (n == 0) { printf("(aucune entree detectee)\n"); return 0; }
  printf("Entrees audio :\n");
  for (int i = 0; i < n; ++i) {
    if (!devs[i].is_input) continue;
    const char *mark = (devs[i].id == g_in_dev) ? "> " : "  ";
    printf("%s[%d] %s\n", mark, devs[i].id, devs[i].name);
  }
  return 0;
}

static int cmd_out(int argc, char **argv) {
  if (argc >= 2) {
    g_out_dev = atoi(argv[1]);
    printf("TDP: sortie %d selectionnee\n", g_out_dev);
    return 0;
  }
  tdp_device devs[TDP_MAX_DEVICES];
  int n = audio_list_devices(devs);
  if (n == 0) { printf("(aucune sortie detectee)\n"); return 0; }
  printf("Sorties audio :\n");
  for (int i = 0; i < n; ++i) {
    if (!devs[i].is_output) continue;
    const char *mark = (devs[i].id == g_out_dev) ? "> " : "  ";
    printf("%s[%d] %s\n", mark, devs[i].id, devs[i].name);
  }
  return 0;
}

static int cmd_start(int argc, char **argv) {
  (void)argc; (void)argv;
  if (atomic_load_explicit(&g_sh.running, memory_order_relaxed)) {
    printf("TDP: deja en cours (stop d'abord)\n");
    return 0;
  }
  /* Active la chaine chargee dans l'audio. */
  audio_set_chain(&g_chain);
  if (!audio_start(g_in_dev, g_out_dev)) {
    printf("TDP: audio indisponible (portaudio.dll introuvable ? ou aucune carte?)\n");
    return 0;
  }
  printf("TDP: flux demarre (entree %d, sortie %d)\n", g_in_dev, g_out_dev);
  return 0;
}

static int cmd_stop(int argc, char **argv) {
  (void)argc; (void)argv;
  audio_stop();
  printf("TDP: flux arrete\n");
  return 0;
}

/* Record : demarre l'enregistrement (post-pedales). */
static int cmd_record(int argc, char **argv) {
  (void)argc; (void)argv;
  if (!atomic_load_explicit(&g_sh.running, memory_order_relaxed)) {
    printf("TDP: rien a enregistrer (flux non demarre)\n");
    return 0;
  }
  if (atomic_load_explicit(&g_sh.recording, memory_order_relaxed)) {
    printf("TDP: deja en train d'enregistrer\n");
    return 0;
  }
  audio_rec_reset();
  atomic_store_explicit(&g_sh.recording, true, memory_order_relaxed);
  printf("TDP: enregistrement demarre (tapez <Entree> pour stopper + sauvegarder)\n");
  return 0;
}

/* Stoppe l'enregistrement (si actif) + sauvegarde le WAV horodate.
 * Appelle quand l'utilisateur tape <Entree> (ligne vide). */
static void record_stop_and_save(void) {
  if (!atomic_load_explicit(&g_sh.recording, memory_order_relaxed)) return;
  atomic_store_explicit(&g_sh.recording, false, memory_order_relaxed);

  /* Lit le ring buffer (head = position actuelle, count = nb ecrits). */
  unsigned head = audio_rec_head();
  unsigned long long count = atomic_load_explicit(&g_ring.count, memory_order_relaxed);
  unsigned cap = TDP_REC_CAPACITY;
  unsigned long n = (count > cap) ? cap : (unsigned long)count;

  /* Reconstitue le signal dans l'ordre (du plus ancien au plus recent).
   * Le ring ecrit en head ; les derniers n echantillons sont les plus recents.
   * On part de (head - n) % cap et on lit n echantillons. */
  static float tmp[TDP_REC_CAPACITY]; /* tampon statique pour la sauvegarde */
  unsigned start = (head > n) ? (head - n) : (head + cap - n);
  for (unsigned long i = 0; i < n; ++i) {
    tmp[i] = g_ring.buf[(start + i) % cap];
  }

  char path[512];
  if (wav_timestamped_name(path, sizeof path) != 0) {
    printf("TDP: impossible de generer le nom du fichier\n");
    return;
  }
  double sr = 48000.0; /* TODO: utiliser le sr reel (a ajouter dans audio.h) */
  if (wav_write(path, tmp, (long)n, sr) == 0) {
    double dur = (double)n / sr;
    printf("TDP: enregistrement sauvegarde : %s (%.1f s)\n", path, dur);
  } else {
    printf("TDP: echec de l'ecriture de %s\n", path);
  }
  audio_rec_reset();
}

/* Flag de sortie (set par cmd_quit via exec_line). */
int shell_quit_flag = 0;

/* Forward : stoppe l'enregistrement + sauvegarde (definie plus bas). */
static void record_stop_and_save(void);

/* ------------------------------------------------------------------ */
/* Boucle principale                                                   */
/* ------------------------------------------------------------------ */
static void exec_line(char *line, int interactive) {
  /* Supprime le newline. */
  line[strcspn(line, "\r\n")] = '\0';
  /* Ignore les espaces de depart. */
  char *s = line;
  while (*s == ' ' || *s == '\t') ++s;
  /* Ligne vide : si record actif -> stopper + sauvegarder le WAV. */
  if (*s == '\0') {
    record_stop_and_save();
    return;
  }
  /* Ignore les commentaires (#). */
  if (*s == '#') return;

  /* Scinde en arguments (sur espaces). */
  char *args[8];
  int argc = 0;
  char *tok = s;
  while (argc < 8) {
    while (*tok == ' ' || *tok == '\t') ++tok;
    if (*tok == '\0') break;
    args[argc++] = tok;
    char *sp = strchr(tok, ' ');
    if (sp) { *sp = '\0'; tok = sp + 1; }
    else break;
  }
  if (argc == 0) return;

  const char *name = args[0];
  for (int i = 0; i < N_CMDS; ++i) {
    if (strcmp(g_cmds[i].name, name) == 0) {
      if (g_cmds[i].fn(argc, args) == -1) {
        /* sortie demandee : on le gère par un flag global plus bas */
        shell_quit_flag = 1;
      }
      return;
    }
  }
  printf("TDP: commande inconnue '%s' (help pour la liste)\n", name);
  (void)interactive;
}

/* Point d'entrée du shell.
 * script_path : NULL = interactif, sinon lecture du fichier. */
void shell_run(const char *script_path) {
  /* Initialise l'etat audio partage (avant tout start). */
  audio_init(&g_sh, &g_ring, g_rec_buf);

  FILE *f = NULL;
  int interactive = (script_path == NULL);
  if (!interactive) {
    f = fopen(script_path, "r");
    if (!f) {
      fprintf(stderr, "TDP: impossible d'ouvrir le script '%s'\n", script_path);
      return;
    }
  }

  if (interactive) {
    printf("TDP — Terminal Distortion Pedal (CLI). Tapez 'help' pour l'aide, 'quit' pour sortir.\n");
  }

  char line[LINE_MAX];
  while (!shell_quit_flag) {
    if (interactive) {
      printf("tdp> ");
      fflush(stdout);
      if (!fgets(line, sizeof line, stdin)) break; /* EOF (Ctrl-D) */
    } else {
      if (!fgets(line, sizeof line, f)) break; /* fin du script */
    }
    exec_line(line, interactive);
  }

  /* Arrete le flux au quit (si en cours). */
  audio_stop();

  if (f) fclose(f);
  if (interactive) printf("\nTDP: au revoir.\n");
}
