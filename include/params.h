#ifndef TPD_PARAMS_H
#define TPD_PARAMS_H

#include <stdatomic.h>
#include <stdbool.h>

/*
 * État partagé entre le thread UI (main) et le thread audio (callback
 * PortAudio). Lock-free : tout passe par des stdatomic.
 *
 *  - Thread UI : producteur de gain/master/bypass/mute, consommateur des vu_*
 *    et de l'état stream (sr, frame_buffer, pa_ok).
 *  - Thread audio : consommateur de gain/master/bypass/mute, producteur des
 *    vu_*.
 */
typedef struct {
  _Atomic float gain;      /* 0..1, défaut 0.5   (FR-005) */
  _Atomic float master;    /* 0..1, défaut 0.75  (FR-006) */
  _Atomic bool  bypass;    /* FR-007 */
  _Atomic bool  mute;      /* FR-007 */
  _Atomic float vu_in_rms;   /* 0..1 (FR-008) */
  _Atomic float vu_in_peak;  /* 0..1, peak-hold (FR-008) */
  _Atomic float vu_out_rms;  /* 0..1 (FR-008) */
  _Atomic float vu_out_peak; /* 0..1, peak-hold (FR-008) */
  /* État stream réel : écrit une fois au démarrage, lu par l'UI. (FR-019/020) */
  _Atomic long sr;            /* Hz réel */
  _Atomic long frame_buffer;  /* échantillons / bloc réel */
  _Atomic bool pa_ok;         /* statut PortAudio */
} tpd_params;

/* Instance unique, définie dans params.c. */
extern tpd_params P;

/* Pose les valeurs par défaut (gain 0.5, master 0.75, autres 0/false). */
void params_init(void);

#endif /* TPD_PARAMS_H */
