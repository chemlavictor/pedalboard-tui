#include "audio.h"
#include "params.h"
#include "dsp.h"

#include <portaudio.h>
#include <stdatomic.h>
#include <string.h>

static PaStream *g_stream = NULL;
static dsp_vu g_vu; /* état VU local au thread audio (peak-hold) */

/*
 * Callback PortAudio : thread audio, temps réel critique.
 * CONTRAINTES RT (FR-021..023) :
 *  - pas de malloc/free/new
 *  - pas d'I/O, pas de printf, pas de lock/mutex
 *  - lecture des paramètres uniquement via atomic_load
 */
static int audio_callback(const void *in, void *out, unsigned long n,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData) {
  (void)timeInfo;
  (void)statusFlags;
  (void)userData;

  const float *in_buf = (const float *)in;
  float *out_buf = (float *)out;

  float G = atomic_load_explicit(&P.gain, memory_order_relaxed);
  float V = atomic_load_explicit(&P.master, memory_order_relaxed);
  bool bypass = atomic_load_explicit(&P.bypass, memory_order_relaxed);
  bool mute = atomic_load_explicit(&P.mute, memory_order_relaxed);
  float sr = (float)atomic_load_explicit(&P.sr, memory_order_relaxed);

  dsp_process(in_buf, out_buf, n, G, V, bypass, mute, sr, &g_vu);

  /* Publice les VU pour le thread UI. */
  atomic_store_explicit(&P.vu_in_rms, g_vu.in_rms, memory_order_relaxed);
  atomic_store_explicit(&P.vu_in_peak, g_vu.in_peak, memory_order_relaxed);
  atomic_store_explicit(&P.vu_out_rms, g_vu.out_rms, memory_order_relaxed);
  atomic_store_explicit(&P.vu_out_peak, g_vu.out_peak, memory_order_relaxed);

  return paContinue;
}

static bool try_open(double sr, long frames) {
  PaStreamParameters inp, outp;
  memset(&inp, 0, sizeof inp);
  memset(&outp, 0, sizeof outp);
  inp.device = Pa_GetDefaultInputDevice();
  outp.device = Pa_GetDefaultOutputDevice();
  inp.channelCount = 1;
  outp.channelCount = 1;
  inp.sampleFormat = paFloat32;
  outp.sampleFormat = paFloat32;
  inp.suggestedLatency = 0.0;
  outp.suggestedLatency = 0.0;

  PaError err = Pa_OpenStream(&g_stream, &inp, &outp, sr, (long)frames,
                              paNoFlag, audio_callback, NULL);
  if (err != paNoError) return false;

  PaError s = Pa_StartStream(g_stream);
  if (s != paNoError) {
    Pa_CloseStream(g_stream);
    g_stream = NULL;
    return false;
  }
  return true;
}

bool audio_start(void) {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    atomic_store_explicit(&P.pa_ok, false, memory_order_relaxed);
    return false;
  }

  dsp_vu_reset(&g_vu);

  double req_sr = 0.0;
  long req_fb = 0;
  bool opened = false;
  req_sr = 48000.0; req_fb = 256;
  if (try_open(req_sr, req_fb)) opened = true;
  if (!opened) { req_sr = 44100.0; req_fb = 256; if (try_open(req_sr, req_fb)) opened = true; }
  if (!opened) { req_sr = 48000.0; req_fb = 128; if (try_open(req_sr, req_fb)) opened = true; }

  if (!opened) {
    atomic_store_explicit(&P.pa_ok, false, memory_order_relaxed);
    Pa_Terminate();
    return false;
  }

  /* PortAudio garantit un framesPerBuffer non nul dans le callback ; on enregistre
   * la valeur demandée (réelle ici). sr demandé = sr du stream full-duplex. */
  atomic_store_explicit(&P.sr, (long)req_sr, memory_order_relaxed);
  atomic_store_explicit(&P.frame_buffer, req_fb, memory_order_relaxed);
  atomic_store_explicit(&P.pa_ok, true, memory_order_relaxed);
  return true;
}

void audio_stop(void) {
  if (g_stream) {
    Pa_StopStream(g_stream);
    Pa_CloseStream(g_stream);
    g_stream = NULL;
  }
  atomic_store_explicit(&P.pa_ok, false, memory_order_relaxed);
  Pa_Terminate();
}
