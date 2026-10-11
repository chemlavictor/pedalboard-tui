#include "dsp.h"
#include "pedals.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { printf("FAIL: %s (ligne %d)\n", msg, __LINE__); failures++; } \
  else { printf("ok:   %s\n", msg); } \
} while (0)

static int is_finite(float x) { return isfinite(x); }

int main(void) {
  const float sr = 48000.0f;

  /* 1) tanh(0) = 0 : in=[0] -> out≈0 (FR-010) */
  {
    dsp_vu vu; dsp_vu_reset(&vu);
    float in[1] = {0.0f}, out[1] = {999.0f};
    dsp_process(in, out, 1, 0.5f, 0.75f, false, false, sr, &vu);
    CHECK(fabsf(out[0]) < 1e-6f, "tanh(0)=0 -> out≈0");
    CHECK(vu.in_rms == 0.0f && vu.out_rms == 0.0f, "RMS silence = 0");
  }

  /* 2) Bornes + pas de NaN : grandes entrées, G=1,V=1 (FR-011) */
  {
    dsp_vu vu; dsp_vu_reset(&vu);
    float in[4] = {10.0f, -10.0f, 1e6f, -1e6f}, out[4];
    dsp_process(in, out, 4, 1.0f, 1.0f, false, false, sr, &vu);
    int bounded = 1, finite = 1;
    for (int i = 0; i < 4; ++i) {
      if (out[i] > 1.0f + 1e-6f || out[i] < -1.0f - 1e-6f) bounded = 0;
      if (!is_finite(out[i])) finite = 0;
    }
    CHECK(bounded, "tanh borne out dans [-1,1]");
    CHECK(finite, "pas de NaN/Inf sur entrées extrêmes");
  }

  /* 3) Bypass : out == in (FR-015) */
  {
    dsp_vu vu; dsp_vu_reset(&vu);
    float in[3] = {0.25f, -0.7f, 1.0f}, out[3];
    dsp_process(in, out, 3, 0.5f, 0.75f, true, false, sr, &vu);
    CHECK(memcmp(in, out, sizeof in) == 0, "bypass -> out==in");
    CHECK(fabsf(vu.out_rms - vu.in_rms) < 1e-5f, "bypass -> VU_out==VU_in");
  }

  /* 4) Mute : out == 0, VU_out = 0, DSP exécuté (FR-016) */
  {
    dsp_vu vu; dsp_vu_reset(&vu);
    float in[4] = {0.5f, -0.5f, 0.9f, -0.9f}, out[4];
    dsp_process(in, out, 4, 0.5f, 0.75f, false, true, sr, &vu);
    int all_zero = 1;
    for (int i = 0; i < 4; ++i) if (out[i] != 0.0f) all_zero = 0;
    CHECK(all_zero, "mute -> out==0");
    CHECK(vu.out_rms == 0.0f && vu.out_peak == 0.0f, "mute -> VU_out=0");
    CHECK(vu.in_rms > 0.0f, "mute -> VU_in continue (>0)");
  }

  /* 5) RMS sinusoïde ≈ amp/sqrt(2) (FR-012/013) */
  {
    dsp_vu vu; dsp_vu_reset(&vu);
    const size_t n = 100000;
    float *in = malloc(n * sizeof(float));
    float *out = malloc(n * sizeof(float));
    float amp = 0.5f;
    for (size_t i = 0; i < n; ++i) {
      double ph = 6.28318530718 * 220.0 * (double)i / (double)sr; /* ~220 Hz, 4 périodes */
      in[i] = (float)(amp * sin(ph));
    }
    /* G=1, V=1 : out = tanh(in). tanh est ~linéaire sur [-0.5,0.5], donc
     * RMS(out) ≈ RMS(in) ≈ amp/sqrt(2). Vérifie la chaîne VU in/out. */
    dsp_process(in, out, n, 1.0f, 1.0f, false, false, sr, &vu);
    float expected = amp / sqrtf(2.0f); /* ≈ 0.3536 */
    CHECK(fabsf(vu.in_rms - expected) < 0.02f, "VU_in RMS sinusoïde ≈ amp/sqrt(2)");
    CHECK(fabsf(vu.out_rms - expected) < 0.03f, "VU_out RMS ≈ VU_in (tanh quasi-linéaire)");
    CHECK(vu.out_peak <= 1.0f + 1e-6f, "VU_out peak borné ≤ 1");
    free(in); free(out);
  }

  /* 6) Peak-hold : décroît linéairement sur ~500 ms (FR-014) */
  {
    dsp_vu vu; dsp_vu_reset(&vu);
    long block = 512;
    float big[512], small[512];
    for (long i = 0; i < block; ++i) { big[i] = 1.0f; small[i] = 0.0f; }
    /* Bloc de pic : peak monte à 1. */
    dsp_process(big, big, block, 1.0f, 1.0f, false, false, sr, &vu);
    float peak_after_hit = vu.in_peak;
    CHECK(fabsf(peak_after_hit - 1.0f) < 1e-6f, "peak monte au niveau du pic");
    /* Blocs silencieux : le hold retient puis décroît. */
    dsp_process(small, small, block, 1.0f, 1.0f, false, false, sr, &vu);
    float peak_held = vu.in_peak;
    CHECK(peak_held >= 0.99f, "peak-hold retient le pic juste après");
    /* Beaucoup de blocs silencieux : le hold retombe. */
    for (int k = 0; k < 100; ++k)
      dsp_process(small, small, block, 1.0f, 1.0f, false, false, sr, &vu);
    CHECK(vu.in_peak < 0.05f, "peak-hold retombe après la fenêtre");
  }

  /* 7) Catalogue : chaque pedale du catalogue est trouvee (FR-007) */
  {
    const pedal_def *all = pedals_all();
    int n = pedals_count();
    CHECK(n == 5, "catalogue contient 5 pedales");
    int found = 0;
    for (int i = 0; all[i].name; ++i) {
      const pedal_def *d = pedal_find(all[i].name);
      if (d && d == &all[i]) ++found;
    }
    CHECK(found == n, "chaque pedale du catalogue est trouvee par pedal_find");
    CHECK(pedal_find("distortion") != NULL, "pedal_find('distortion') != NULL");
    CHECK(pedal_find("booster") != NULL, "pedal_find('booster') != NULL");
    CHECK(pedal_find("noise_gate") != NULL, "pedal_find('noise_gate') != NULL");
    CHECK(pedal_find("volume") != NULL, "pedal_find('volume') != NULL");
    CHECK(pedal_find("mute") != NULL, "pedal_find('mute') != NULL");
    CHECK(pedal_find("inconnue") == NULL, "pedal_find('inconnue') == NULL");
    CHECK(pedal_find(NULL) == NULL, "pedal_find(NULL) == NULL");
  }

  /* 8) Catalogue : params coerents (min<max, def dans [min,max]) (FR-007) */
  {
    const pedal_def *all = pedals_all();
    int ok = 1;
    for (int i = 0; all[i].name; ++i) {
      for (const pedal_param *p = all[i].params; p && p->name; ++p) {
        if (!(p->minv <= p->def && p->def <= p->maxv)) ok = 0;
        if (!(p->minv <= p->maxv)) ok = 0;
      }
    }
    CHECK(ok, "tous les params : min<=def<=max");
  }

  /* 9) DSP booster : x *= level (FR-005) */
  {
    const pedal_def *d = pedal_find("booster");
    pedal_state st = {0}; st.v[0] = 2.0f; /* level=2 */
    float buf[3] = {0.25f, -0.5f, 1.0f};
    d->process(buf, 3, &st);
    CHECK(fabsf(buf[0] - 0.5f) < 1e-6f, "booster level=2 : 0.25 -> 0.5");
    CHECK(fabsf(buf[1] + 1.0f) < 1e-6f, "booster level=2 : -0.5 -> -1.0");
    CHECK(fabsf(buf[2] - 2.0f) < 1e-6f, "booster level=2 : 1.0 -> 2.0");
  }

  /* 10) DSP distortion : y = tanh(gain*x) (FR-005) */
  {
    const pedal_def *d = pedal_find("distortion");
    pedal_state st = {0}; st.v[0] = 1.0f; /* gain=1 */
    float buf[3] = {0.0f, 1.0f, -1.0f};
    d->process(buf, 3, &st);
    CHECK(fabsf(buf[0]) < 1e-6f, "distortion : tanh(0)=0");
    CHECK(fabsf(buf[1] - tanhf(1.0f)) < 1e-6f, "distortion : tanh(1)");
    CHECK(fabsf(buf[2] + tanhf(1.0f)) < 1e-6f, "distortion : tanh(-1)=-tanh(1)");
  }

  /* 11) DSP noise_gate : silence si RMS < seuil (FR-005) */
  {
    const pedal_def *d = pedal_find("noise_gate");
    /* Seuil -45 dB -> ligne ~ 0.0056. */
    pedal_state st = {0}; st.v[0] = -45.0f;
    /* Signal faible (RMS bien en dessous du seuil) -> mute. */
    float quiet[8] = {0.001f, -0.001f, 0.001f, -0.001f, 0.001f, -0.001f, 0.001f, -0.001f};
    d->process(quiet, 8, &st);
    int all_zero = 1; for (int i = 0; i < 8; ++i) if (quiet[i] != 0.0f) all_zero = 0;
    CHECK(all_zero, "noise_gate : signal faible -> silence");
    /* Signal fort (RMS au-dessus du seuil) -> passe. */
    float loud[8]; for (int i = 0; i < 8; ++i) loud[i] = (i % 2 ? -0.5f : 0.5f);
    float saved[8]; memcpy(saved, loud, sizeof loud);
    d->process(loud, 8, &st);
    CHECK(memcmp(loud, saved, sizeof loud) == 0, "noise_gate : signal fort -> passe");
  }

  /* 12) DSP volume : x *= level (FR-005) */
  {
    const pedal_def *d = pedal_find("volume");
    pedal_state st = {0}; st.v[0] = 0.5f;
    float buf[2] = {1.0f, -1.0f};
    d->process(buf, 2, &st);
    CHECK(fabsf(buf[0] - 0.5f) < 1e-6f, "volume level=0.5 : 1.0 -> 0.5");
    CHECK(fabsf(buf[1] + 0.5f) < 1e-6f, "volume level=0.5 : -1.0 -> -0.5");
  }

  /* 13) DSP mute : out = 0 (FR-005) */
  {
    const pedal_def *d = pedal_find("mute");
    pedal_state st = {0};
    float buf[3] = {0.9f, -0.9f, 0.3f};
    d->process(buf, 3, &st);
    int all_zero = 1; for (int i = 0; i < 3; ++i) if (buf[i] != 0.0f) all_zero = 0;
    CHECK(all_zero, "mute : out = 0");
  }

  /* 14) Chaine complete : booster -> distortion -> volume (FR-005) */
  {
    chain_t c = {0};
    c.defs[0] = pedal_find("booster");    c.state[0].v[0] = 2.0f;
    c.defs[1] = pedal_find("distortion"); c.state[1].v[0] = 1.0f;
    c.defs[2] = pedal_find("volume");     c.state[2].v[0] = 0.5f;
    c.len = 3;
    float buf[1] = {0.5f};
    chain_process(&c, buf, 1);
    /* Attente : tanh(1.0 * (2.0*0.5)) * 0.5 = tanh(1.0)*0.5 */
    float expected = tanhf(1.0f) * 0.5f;
    CHECK(fabsf(buf[0] - expected) < 1e-5f, "chaine booster->distortion->volume correcte");
  }

  printf("\n%s (%d échec%s)\n", failures ? "TESTS ÉCHEUÉS" : "TOUS LES TESTS PASSENT",
         failures, failures > 1 ? "s" : "");
  return failures ? 1 : 0;
}
