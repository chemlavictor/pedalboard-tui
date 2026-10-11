/* Thread audio temps réel : applique la chaine de pedales au signal,
 * calcule le VU, et ecrit dans le ring buffer si `recording`.
 *
 * CONTRAINTES RT : pas de malloc/free, pas d'I/O, pas de locks dans le
 * callback. Lecture de l'etat via stdatomic uniquement.
 */
#include "audio.h"
#include <portaudio.h>
#include <string.h>
#include <math.h>

static PaStream *g_stream = NULL;

/* Etat partagé (pointe vers des structures allouees par le main). */
static audio_shared *g_sh = NULL;
static rec_ring *g_ring = NULL;

/* Chaine active : le RT lit l'index (atomic) 0/1 du tampon g_chain_active.
 * On utilise un index atomique pour eviter les pointeurs atomiques (piège
 * _Atomic T* en C11). */
static _Atomic int g_chain_idx = 0;
/* Tampon statique de la chaine active. */
static chain_t g_chain_active = {0};

/* VU local au thread audio (peak-hold). */
typedef struct { float in_rms, in_peak, out_rms, out_peak; } vu_t;
static vu_t g_vu = {0};

static void vu_reset(void) { g_vu = (vu_t){0}; }

/* Calcule RMS + peak (sur le bloc) pour un buffer. */
static void vu_update(vu_t *vu, const float *buf, int n, float sr) {
  (void)sr;
  float sum = 0.0f, peak = 0.0f;
  for (int i = 0; i < n; ++i) {
    sum += buf[i] * buf[i];
    float a = buf[i] < 0 ? -buf[i] : buf[i];
    if (a > peak) peak = a;
  }
  float rms = sqrtf(sum / (float)(n > 0 ? n : 1));
  if (rms > vu->in_rms) vu->in_rms = rms; /* simplifie : peak-hold sur le RMS du bloc */
  if (peak > vu->in_peak) vu->in_peak = peak;
}

/*
 * Callback PortAudio (thread audio, temps réel).
 */
static int audio_callback(const void *in, void *out, unsigned long n,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData) {
  (void)timeInfo; (void)statusFlags; (void)userData;
  if (!g_sh || !g_ring) return paContinue;

  const float *in_buf = (const float *)in;
  float *out_buf = (float *)out;

  /* Copie l'entree dans un buffer local (on modifie en place). */
  /* out_buf sert de buffer de travail (on y ecrit le signal traite). */
  memcpy(out_buf, in_buf, n * sizeof(float));

  /* VU in (avant pedales). */
  vu_t vu_in = {0};
  vu_update(&vu_in, out_buf, (int)n, 0.0f);

  /* Applique la chaine de pedales. */
  int idx = g_chain_idx;
  if (idx == 1 && g_chain_active.len > 0) chain_process(&g_chain_active, out_buf, (int)n);

  /* VU out (apres pedales). */
  vu_t vu_out = {0};
  vu_update(&vu_out, out_buf, (int)n, 0.0f);

  /* Publie les VU (le main les lit). On combine peak-hold simple. */
  atomic_store_explicit(&g_sh->vu_in_rms, vu_in.in_rms, memory_order_relaxed);
  atomic_store_explicit(&g_sh->vu_in_peak, vu_in.in_peak, memory_order_relaxed);
  atomic_store_explicit(&g_sh->vu_out_rms, vu_out.out_rms, memory_order_relaxed);
  atomic_store_explicit(&g_sh->vu_out_peak, vu_out.out_peak, memory_order_relaxed);

  /* Enregistrement : ecrit le signal post-pedales dans le ring si recording. */
  if (atomic_load_explicit(&g_sh->recording, memory_order_relaxed)) {
    unsigned head = g_ring->head;
    float *buf = g_ring->buf;
    unsigned cap = TDP_REC_CAPACITY;
    for (unsigned i = 0; i < n; ++i) {
      buf[head] = out_buf[i];
      head = (head + 1) % cap;
    }
    g_ring->head = head;
    unsigned long long c = atomic_load_explicit(&g_ring->count, memory_order_relaxed);
    atomic_store_explicit(&g_ring->count, c + n, memory_order_relaxed);
  }

  return paContinue;
}

void audio_init(audio_shared *sh, rec_ring *ring, float *rec_buf) {
  g_sh = sh;
  g_ring = ring;
  ring->buf = rec_buf;
  ring->head = 0;
  atomic_store_explicit(&ring->count, 0, memory_order_relaxed);
  atomic_store_explicit(&sh->running, false, memory_order_relaxed);
  atomic_store_explicit(&sh->recording, false, memory_order_relaxed);
  atomic_store_explicit(&sh->pa_ok, false, memory_order_relaxed);
  g_chain_idx = 0;
}

void audio_set_chain(const chain_t *c) {
  if (c && c->len > 0) {
    g_chain_active = *c; /* copie dans le tampon statique */
    g_chain_idx = 1;
  } else {
    g_chain_idx = 0;
  }
}

static bool try_open(double sr, long frames, int in_dev, int out_dev) {
  PaStreamParameters inp, outp;
  memset(&inp, 0, sizeof inp);
  memset(&outp, 0, sizeof outp);
  inp.device = (in_dev >= 0) ? (PaDeviceIndex)in_dev : Pa_GetDefaultInputDevice();
  outp.device = (out_dev >= 0) ? (PaDeviceIndex)out_dev : Pa_GetDefaultOutputDevice();
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
  if (s != paNoError) { Pa_CloseStream(g_stream); g_stream = NULL; return false; }
  return true;
}

bool audio_start(int in_id, int out_id) {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    if (g_sh) atomic_store_explicit(&g_sh->pa_ok, false, memory_order_relaxed);
    return false;
  }
  vu_reset();

  bool opened = try_open(48000.0, 256, in_id, out_id);
  if (!opened) opened = try_open(44100.0, 256, in_id, out_id);
  if (!opened) opened = try_open(48000.0, 128, in_id, out_id);
  if (!opened) opened = try_open(48000.0, 256, -1, -1);

  if (!opened) {
    if (g_sh) atomic_store_explicit(&g_sh->pa_ok, false, memory_order_relaxed);
    return false;
  }
  if (g_sh) {
    atomic_store_explicit(&g_sh->pa_ok, true, memory_order_relaxed);
    atomic_store_explicit(&g_sh->running, true, memory_order_relaxed);
  }
  return true;
}

void audio_stop(void) {
  if (g_stream) {
    Pa_StopStream(g_stream);
    Pa_CloseStream(g_stream);
    g_stream = NULL;
  }
  if (g_sh) {
    atomic_store_explicit(&g_sh->pa_ok, false, memory_order_relaxed);
    atomic_store_explicit(&g_sh->running, false, memory_order_relaxed);
    atomic_store_explicit(&g_sh->recording, false, memory_order_relaxed);
  }
}

int audio_list_devices(tdp_device *out) {
  int n = 0;
  int max = Pa_GetDeviceCount();
  if (max < 0) return 0;
  if (max > TDP_MAX_DEVICES) max = TDP_MAX_DEVICES;
  for (int i = 0; i < max; ++i) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
    if (!info) continue;
    out[n].id = i;
    strncpy(out[n].name, info->name, TDP_DEVICE_NAME_LEN - 1);
    out[n].name[TDP_DEVICE_NAME_LEN - 1] = '\0';
    out[n].is_input = (info->maxInputChannels > 0);
    out[n].is_output = (info->maxOutputChannels > 0);
    ++n;
  }
  return n;
}

unsigned audio_rec_head(void) { return g_ring ? g_ring->head : 0; }

void audio_rec_reset(void) {
  if (!g_ring) return;
  g_ring->head = 0;
  atomic_store_explicit(&g_ring->count, 0, memory_order_relaxed);
}
