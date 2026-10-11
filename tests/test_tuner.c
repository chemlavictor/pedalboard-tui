/* Tests de l'accordeur (detection de frequence + note). */
#include "tuner.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { printf("FAIL: %s (ligne %d)\n", msg, __LINE__); failures++; } \
  else { printf("ok:   %s\n", msg); } \
} while (0)

/* Genere une sinusode de frequence f, amplitude amp, n echantillons a sr. */
static void gen_sine(float *buf, int n, double f, double sr, double amp) {
  for (int i = 0; i < n; ++i) {
    buf[i] = (float)(amp * sin(6.28318530718 * f * (double)i / sr));
  }
}

int main(void) {
  const double sr = 48000.0;
  const int n = 4096;
  static float buf[4096];

  /* 1) E2 (82.4 Hz) -> note E2. */
  {
    gen_sine(buf, n, 82.41, sr, 0.8);
    tuner_result r;
    tuner_analyze(buf, n, sr, &r);
    CHECK(r.has_note, "E2 : note detectee");
    CHECK(strcmp(r.note, "E2") == 0, "E2 : note = E2");
    CHECK(r.cents > -10 && r.cents < 10, "E2 : ecart < 10 cents");
    printf("  (detecte: %s %.1f Hz, %d cents)\n", r.note, r.freq, r.cents);
  }

  /* 2) A2 (110 Hz) -> note A2 (corde 5). */
  {
    gen_sine(buf, n, 110.0, sr, 0.8);
    tuner_result r;
    tuner_analyze(buf, n, sr, &r);
    CHECK(r.has_note, "A2 : note detectee");
    CHECK(strcmp(r.note, "A2") == 0, "A2 : note = A2");
    CHECK(r.cents > -10 && r.cents < 10, "A2 : ecart < 10 cents");
    printf("  (detecte: %s %.1f Hz, %d cents)\n", r.note, r.freq, r.cents);
  }

  /* 3) A4 (440 Hz) -> note A4. */
  {
    gen_sine(buf, n, 440.0, sr, 0.8);
    tuner_result r;
    tuner_analyze(buf, n, sr, &r);
    CHECK(r.has_note, "A4 : note detectee");
    CHECK(strcmp(r.note, "A4") == 0, "A4 : note = A4");
    CHECK(r.cents > -10 && r.cents < 10, "A4 : ecart < 10 cents");
    printf("  (detecte: %s %.1f Hz, %d cents)\n", r.note, r.freq, r.cents);
  }

  /* 4) Note detunee : A2 a 113 Hz (au lieu de 110) -> A2 avec cents positif (trop aigu). */
  {
    gen_sine(buf, n, 113.0, sr, 0.8);
    tuner_result r;
    tuner_analyze(buf, n, sr, &r);
    CHECK(r.has_note, "A2 detune : note detectee");
    CHECK(strcmp(r.note, "A2") == 0, "A2 detune : note reste A2");
    CHECK(r.cents > 10, "A2 detune : cents positif (trop aigu)");
    printf("  (detecte: %s %.1f Hz, %d cents — attendu A2, > 10)\n", r.note, r.freq, r.cents);
  }

  /* 5) Silence -> pas de note. */
  {
    for (int i = 0; i < n; ++i) buf[i] = 0.0f;
    tuner_result r;
    tuner_analyze(buf, n, sr, &r);
    CHECK(!r.has_note, "silence : pas de note");
  }

  /* 6) E4 (329.6 Hz) -> note E4 (corde 1). */
  {
    gen_sine(buf, n, 329.63, sr, 0.8);
    tuner_result r;
    tuner_analyze(buf, n, sr, &r);
    CHECK(r.has_note, "E4 : note detectee");
    CHECK(strcmp(r.note, "E4") == 0, "E4 : note = E4");
    printf("  (detecte: %s %.1f Hz, %d cents)\n", r.note, r.freq, r.cents);
  }

  printf("\n%s (%d échec%s)\n", failures ? "TESTS ÉCHEUÉS" : "TOUS LES TESTS PASSENT",
         failures, failures > 1 ? "s" : "");
  return failures ? 1 : 0;
}
