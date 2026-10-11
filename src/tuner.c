/* Accordeur : detection de frequence fondamentale par YIN.
 * YIN (de Chafe) est robuste aux sinusoïdes et aux signaux de guitare.
 * Portable, sans malloc (buffers statiques). */
#include "tuner.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* Notes de reference : freq = 440 * 2^((n-69)/12) (A4 = 440 Hz, MIDI 69). */
static const char *g_note_names[12] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

/* Tables statiques pour YIN (pas de malloc). */
#define MAX_N 4096
static float g_signal[MAX_N];   /* signal (centré) */
static float g_d[MAX_N];        /* difference cumulative */
static float g_dnorm[MAX_N];    /* normalisation cumulative */

/* Algorithme YIN. Retourne la frequence en Hz (0 si non detectee).
 * confidence : 0..1. */
static float yin_pitch(const float *buf, int n, double sr, float *confidence) {
  *confidence = 0.0f;
  if (n > MAX_N) n = MAX_N;
  if (n < 32) return 0.0f;

  /* On utilise la moitie du buffer pour les delais. */
  int half = n / 2;
  if (half < 16) return 0.0f;

  /* Centrage (retire la moyenne). */
  float mean = 0.0f;
  for (int i = 0; i < n; ++i) mean += buf[i];
  mean /= (float)n;
  for (int i = 0; i < n; ++i) g_signal[i] = buf[i] - mean;

  /* Energie totale. */
  float energy = 0.0f;
  for (int i = 0; i < n; ++i) energy += g_signal[i] * g_signal[i];
  if (energy < 1e-4f) return 0.0f;

  /* 1) Difference cumulative d(tau) = sum (x[i] - x[i+tau])^2,
   *    pour tau dans [tau_min, tau_max] (freq 60..1200 Hz). */
  int tau_min = (int)(sr / 1200.0);
  int tau_max = (int)(sr / 60.0);
  if (tau_min < 1) tau_min = 1;
  if (tau_max > half) tau_max = half;
  if (tau_max <= tau_min) return 0.0f;

  /* d[0] = 0. */
  g_d[0] = 0.0f;
  for (int tau = 1; tau < half; ++tau) {
    float sum = 0.0f;
    for (int i = 0; i < half; ++i) {
      float diff = g_signal[i] - g_signal[i + tau];
      sum += diff * diff;
    }
    g_d[tau] = sum;
  }

  /* 2) Normalisation cumulative : d'(tau) = d(tau) * (sum_{i=0}^{tau-1} x[i]^2) /
   *    (tau * energy). Evite les sous-harmoniques. */
  float running = 0.0f;
  g_dnorm[0] = 1.0f; /* inutilisé */
  for (int tau = 1; tau < half; ++tau) {
    running += g_signal[tau - 1] * g_signal[tau - 1];
    if (running > 1e-9f)
      g_dnorm[tau] = (g_d[tau] * energy) / ((float)tau * running);
    else
      g_dnorm[tau] = 1.0f;
  }

  /* 3) Cherche le premier tau (a partir de tau_min) ou d'(tau) < seuil
   *    (0.1), en prenant un minimum local. */
  float threshold = 0.1f;
  int best_tau = -1;
  for (int tau = tau_min; tau < tau_max; ++tau) {
    if (g_dnorm[tau] < threshold) {
      /* descend en suivant les minimums locaux jusqu'au minimum. */
      int t = tau;
      while (t + 1 < tau_max && g_dnorm[t + 1] < g_dnorm[t]) ++t;
      best_tau = t;
      break;
    }
  }

  /* Fallback : si aucun tau sous le seuil, prend le minimum global. */
  if (best_tau < 0) {
    float minv = 1e30f;
    for (int tau = tau_min; tau < tau_max; ++tau) {
      if (g_dnorm[tau] < minv) { minv = g_dnorm[tau]; best_tau = tau; }
    }
    /* Si le minimum est encore trop haut, pas de note fiable. */
    if (minv > 0.5f) return 0.0f;
  }

  /* 4) Interpolation parabolique autour de best_tau. */
  float better_tau = (float)best_tau;
  if (best_tau > tau_min && best_tau < tau_max - 1) {
    float s0 = g_dnorm[best_tau - 1];
    float s1 = g_dnorm[best_tau];
    float s2 = g_dnorm[best_tau + 1];
    float denom = (s0 - 2.0f * s1 + s2);
    if (fabsf(denom) > 1e-9f) {
      float adj = 0.5f * (s0 - s2) / denom;
      if (adj > -0.5f && adj < 0.5f) better_tau += adj;
    }
  }
  if (better_tau < 1.0f) better_tau = 1.0f;

  float freq = (float)(sr / better_tau);

  /* Confidence : 1 - d'(best_tau) (plus c'est petit, plus c'est confiant). */
  float conf = 1.0f - g_dnorm[best_tau];
  if (conf < 0.0f) conf = 0.0f;
  if (conf > 1.0f) conf = 1.0f;
  *confidence = conf;

  if (conf < 0.3f) return 0.0f;
  return freq;
}

/* Note MIDI la plus proche + ecart en centiemes. */
static void freq_to_note(float freq, char note[4], int *cents) {
  double midi = 69.0 + 12.0 * log2((double)freq / 440.0);
  int midi_round = (int)(midi + 0.5);
  if (midi_round < 0) midi_round = 0;
  if (midi_round > 127) midi_round = 127;
  double freq_round = 440.0 * pow(2.0, (midi_round - 69) / 12.0);
  /* Cents : 1200 * log2(freq / freq_round) (1 octave = 1200 cents). */
  double c = 1200.0 * log2((double)freq / freq_round);
  *cents = (int)(c + (c >= 0 ? 0.5 : -0.5));
  int name_idx = midi_round % 12;
  int octave = midi_round / 12 - 1;
  snprintf(note, 4, "%s%d", g_note_names[name_idx], octave);
}

void tuner_analyze(const float *buf, int n, double sr, tuner_result *out) {
  out->freq = 0.0f;
  out->has_note = 0;
  out->note[0] = '?'; out->note[1] = '?'; out->note[2] = 0;
  out->cents = 0;
  out->confidence = 0.0f;

  float conf = 0.0f;
  float freq = yin_pitch(buf, n, sr, &conf);
  if (freq > 0.0f) {
    out->freq = freq;
    out->has_note = 1;
    out->confidence = conf;
    freq_to_note(freq, out->note, &out->cents);
  }
}

void tuner_print(const tuner_result *r) {
  if (!r->has_note) {
    printf("  (pas de note detectee — joue une note)");
    return;
  }
  const char *state;
  if (r->cents == 0) state = "JUSTE";
  else if (r->cents > 0) state = "trop aigu";
  else state = "trop grave";
  /* Detecte si c'est une corde standard de guitare (E2 A2 D3 G3 B3 E4). */
  const char *string_name = NULL;
  if (strcmp(r->note, "E2") == 0) string_name = "corde 6 (Mi grave)";
  else if (strcmp(r->note, "A2") == 0) string_name = "corde 5 (La)";
  else if (strcmp(r->note, "D3") == 0) string_name = "corde 4 (Re)";
  else if (strcmp(r->note, "G3") == 0) string_name = "corde 3 (Sol)";
  else if (strcmp(r->note, "B3") == 0) string_name = "corde 2 (Si)";
  else if (strcmp(r->note, "E4") == 0) string_name = "corde 1 (Mi aigu)";

  printf("  %s  (%.1f Hz)  %d cents (%s)  [%.0f%%]",
         r->note, r->freq, r->cents, state, r->confidence * 100.0f);
  if (string_name) printf("  — %s", string_name);
  printf("\n");
}
