#include "dsp.h"
#include <math.h>

void dsp_vu_reset(dsp_vu *vu) {
  vu->in_rms = vu->in_peak = 0.0f;
  vu->out_rms = vu->out_peak = 0.0f;
  vu->in_hold_left = vu->out_hold_left = 0L;
}

static float rms_of(const float *x, size_t n) {
  if (n == 0) return 0.0f;
  double sum = 0.0;
  for (size_t i = 0; i < n; ++i) sum += (double)x[i] * (double)x[i];
  return (float)sqrt(sum / (double)n);
}

/* Met à jour un peak avec hold de 500 ms. */
static void peak_hold(float *peak, long *hold_left, float new_peak, long n, float sr) {
  long window = (long)(0.5 * sr); /* ~500 ms en échantillons */
  if (window < 1) window = 1;
  if (new_peak > *peak) {
    *peak = new_peak;
    *hold_left = window;
    return;
  }
  if (*hold_left > 0) {
    *hold_left -= (long)n;
    if (*hold_left < 0) *hold_left = 0;
  } else {
    *peak = new_peak;
  }
}

void dsp_process(const float *in, float *out, size_t n,
                 float G, float V, bool bypass, bool mute,
                 float sr, dsp_vu *vu) {
  float in_rms = rms_of(in, n);
  float in_peak = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    float a = in[i] < 0.0f ? -in[i] : in[i];
    if (a > in_peak) in_peak = a;
  }

  if (bypass) {
    for (size_t i = 0; i < n; ++i) out[i] = in[i];
  } else {
    for (size_t i = 0; i < n; ++i) {
      float y = tanhf(G * in[i]);
      out[i] = V * y;
    }
  }

  float out_rms = rms_of(out, n);
  float out_peak = 0.0f;
  for (size_t i = 0; i < n; ++i) {
    float a = out[i] < 0.0f ? -out[i] : out[i];
    if (a > out_peak) out_peak = a;
  }

  if (mute) {
    for (size_t i = 0; i < n; ++i) out[i] = 0.0f;
    out_rms = 0.0f;
    out_peak = 0.0f;
  }

  vu->in_rms = in_rms;
  vu->out_rms = out_rms;
  peak_hold(&vu->in_peak, &vu->in_hold_left, in_peak, (long)n, sr);
  peak_hold(&vu->out_peak, &vu->out_hold_left, out_peak, (long)n, sr);
}
