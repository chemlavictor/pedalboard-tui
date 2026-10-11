/* Catalogue de pédales : définitions + lookup.
 * Source de vérité pour le runtime (pedals/info) et pour le prompt LLM.
 * Une nouvelle pédale = ajouter une entrée dans src/pedals.c + make.
 */
#ifndef TDP_PEDALS_H
#define TDP_PEDALS_H

#include <stdbool.h>

#define MAX_CHAIN 16
#define MAX_PARAMS 4

/* Paramètre d'une pédale : nom, description, plage, défaut, unité. */
typedef struct pedal_param {
  const char *name;   /* NULL = fin du tableau */
  const char *desc;
  float minv, maxv, def;
  const char *unit;   /* "", "dB", "%" */
} pedal_param;

/* État d'une pédale dans la chaîne : valeurs courantes de ses params. */
typedef struct pedal_state {
  float v[MAX_PARAMS];
} pedal_state;

/* Définition d'une pédale (constante, le catalogue). */
typedef struct pedal_def {
  const char *name;
  const char *desc;
  const pedal_param *params; /* array terminée par name==NULL */
  /* Applique la pédale au buffer. st = état courant. n = nb d'échantillons. */
  void (*process)(float *buf, int n, const pedal_state *st);
} pedal_def;

/* Chaîne de pédales chargée (mémoire statique, RT-safe). */
typedef struct chain {
  const pedal_def *defs[MAX_CHAIN];
  pedal_state state[MAX_CHAIN];
  int len;
} chain_t;

/* Renvoie la pédale nommée, ou NULL si inconnue. */
const pedal_def *pedal_find(const char *name);

/* Renvoie tout le catalogue (array terminée par entry->name==NULL). */
const pedal_def *pedals_all(void);

/* Nombre de pédales dans le catalogue. */
int pedals_count(void);

/* Applique toute la chaîne à un buffer, dans l'ordre. */
void chain_process(chain_t *c, float *buf, int n);

#endif /* TDP_PEDALS_H */
