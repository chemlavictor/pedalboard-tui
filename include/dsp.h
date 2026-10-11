#ifndef TDP_DSP_H
#define TDP_DSP_H

#include <stdbool.h>
#include <stddef.h>

/*
 * État des VU maintenu par le thread audio entre les appels de dsp_process.
 * Le peak-hold (500 ms décroissant) est tenu ici : la struct est passée en
 * sortie à chaque bloc, ce qui permet aux tests de l'inspecter/resetter sans
 * état global.
 */
typedef struct {
  float in_rms, in_peak;
  float out_rms, out_peak;
  long  in_hold_left;   /* échantillons restants avant reset du hold */
  long  out_hold_left;
} dsp_vu;

void dsp_vu_reset(dsp_vu *vu);

/*
 * Traite un bloc de n échantillons mono.
 *   bypass : out = in (signal sec), VU_out calculé sur in.
 *   sinon  : y = tanh(G * in[i]) ; out[i] = V * y.
 *   mute   : out = 0 (le DSP est exécuté quand même), VU_out = 0.
 * VU_in = RMS(in) ; VU_out = RMS(out) ; peak-hold 500 ms (fenêtre = 0.5*sr).
 * C pur : dépendance uniquement <math.h>. Pas d'allocation.
 */
void dsp_process(const float *in, float *out, size_t n,
                 float G, float V, bool bypass, bool mute,
                 float sr, dsp_vu *vu);

#endif /* TDP_DSP_H */
