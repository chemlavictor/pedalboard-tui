/* Catalogue de pédales : les 5 pédales de la V1 CLI + leurs fonctions DSP.
 *
 * C'est la source de vérité :
 *  - au runtime, `pedals` / `info` affichent ce catalogue ;
 *  - pour générer une nouvelle pédale avec un LLM, on lui donne ce fichier :
 *    il produit un bloc (param + def + process) qu'on colle ici, puis `make`.
 */
#include "pedals.h"
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Fonctions DSP (une par pédale)                                      */
/* ------------------------------------------------------------------ */

/* Booster : gain linéaire propre (type TS9 level à fond). */
static void booster_process(float *buf, int n, const pedal_state *st) {
  float level = st->v[0];
  for (int i = 0; i < n; ++i) buf[i] *= level;
}

/* Distorsion : y = tanh(gain * x). Le tanh borne la sortie à [-1,1]. */
static void distortion_process(float *buf, int n, const pedal_state *st) {
  float gain = st->v[0];
  for (int i = 0; i < n; ++i) buf[i] = tanhf(gain * buf[i]);
}

/* Noise gate : si le RMS du buffer est en dessous du seuil (dB), silence.
 * Coupe le bruit de fond entre les notes pour des chugs nets. */
static void noise_gate_process(float *buf, int n, const pedal_state *st) {
  if (n <= 0) return;
  float thr_db = st->v[0];
  /* Convertit le seuil dB -> linéaire (amplitude RMS de référence). */
  float thr_lin = powf(10.0f, thr_db / 20.0f);
  /* RMS du buffer courant. */
  float sum = 0.0f;
  for (int i = 0; i < n; ++i) sum += buf[i] * buf[i];
  float rms = sqrtf(sum / (float)n);
  if (rms < thr_lin) {
    for (int i = 0; i < n; ++i) buf[i] = 0.0f;
  }
}

/* Volume : master volume de sortie. */
static void volume_process(float *buf, int n, const pedal_state *st) {
  float level = st->v[0];
  for (int i = 0; i < n; ++i) buf[i] *= level;
}

/* Mute : silenced total (pas de param). */
static void mute_process(float *buf, int n, const pedal_state *st) {
  (void)st;
  for (int i = 0; i < n; ++i) buf[i] = 0.0f;
}

/* ------------------------------------------------------------------ */
/* Tableaux de paramètres                                              */
/* ------------------------------------------------------------------ */

static const pedal_param p_booster[] = {
  { "level", "Gain linéaire propre (1.0 = unity)", 0.0f, 2.0f, 1.5f, "" },
  { NULL, NULL, 0, 0, 0, NULL },
};
static const pedal_param p_distortion[] = {
  { "gain", "Montée de distorsion tanh", 0.0f, 1.0f, 0.5f, "" },
  { NULL, NULL, 0, 0, 0, NULL },
};
static const pedal_param p_noise_gate[] = {
  { "threshold", "Seuil de coupure (RMS)", -80.0f, 0.0f, -45.0f, "dB" },
  { NULL, NULL, 0, 0, 0, NULL },
};
static const pedal_param p_volume[] = {
  { "level", "Volume de sortie", 0.0f, 1.0f, 0.75f, "" },
  { NULL, NULL, 0, 0, 0, NULL },
};
static const pedal_param p_mute[] = {
  { NULL, NULL, 0, 0, 0, NULL },
};

/* ------------------------------------------------------------------ */
/* Le catalogue                                                        */
/* ------------------------------------------------------------------ */

static const pedal_def g_pedals[] = {
  { "booster", "Gain linéaire propre (type TS9 level)", p_booster, booster_process },
  { "distortion", "Distorsion tanh (type TS9 drive)", p_distortion, distortion_process },
  { "noise_gate", "Coupe le bruit de fond si RMS < seuil", p_noise_gate, noise_gate_process },
  { "volume", "Master volume de sortie", p_volume, volume_process },
  { "mute", "Silenced total (toggle)", p_mute, mute_process },
  { NULL, NULL, NULL, NULL }, /* sentinel */
};

const pedal_def *pedal_find(const char *name) {
  if (!name) return NULL;
  for (int i = 0; g_pedals[i].name; ++i) {
    if (strcmp(name, g_pedals[i].name) == 0) return &g_pedals[i];
  }
  return NULL;
}

const pedal_def *pedals_all(void) { return g_pedals; }

int pedals_count(void) {
  int c = 0;
  for (int i = 0; g_pedals[i].name; ++i) ++c;
  return c;
}

void chain_process(chain_t *c, float *buf, int n) {
  if (!c) return;
  for (int i = 0; i < c->len; ++i) {
    if (c->defs[i] && c->defs[i]->process) {
      c->defs[i]->process(buf, n, &c->state[i]);
    }
  }
}
