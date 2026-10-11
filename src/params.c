#include "params.h"

tdp_params P;

void params_init(void) {
  atomic_init(&P.gain, 0.5f);
  atomic_init(&P.master, 0.75f);
  atomic_init(&P.bypass, false);
  atomic_init(&P.mute, false);
  atomic_init(&P.vu_in_rms, 0.0f);
  atomic_init(&P.vu_in_peak, 0.0f);
  atomic_init(&P.vu_out_rms, 0.0f);
  atomic_init(&P.vu_out_peak, 0.0f);
  atomic_init(&P.sr, 0L);
  atomic_init(&P.frame_buffer, 0L);
  atomic_init(&P.pa_ok, false);
}
