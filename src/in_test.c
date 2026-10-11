/* Test de l'entree : affiche le volume percu (0-100%) pendant 5 s.
 * Aide a verifier que du son entre par le micro. */
#include "in_test.h"
#include <portaudio.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Buffer de lecture (256 frames). */
#define FRAME_COUNT 256
static float g_peak = 0.0f; /* peak du dernier buffer (mis a jour par le callback) */

/* Callback : lit le signal d'entree, calcule le peak. */
static PaError in_test_callback(const void *input, void *output,
                                unsigned long frame_count,
                                const PaStreamCallbackTimeInfo *time_info,
                                PaStreamCallbackFlags status_flags,
                                void *user_data) {
  (void)output; (void)time_info; (void)status_flags; (void)user_data;
  const float *in = (const float *)input;
  float peak = 0.0f;
  for (unsigned long i = 0; i < frame_count; ++i) {
    float v = in[i] < 0.0f ? -in[i] : in[i];
    if (v > peak) peak = v;
  }
  g_peak = peak; /* le main lit ce champ */
  return paContinue;
}

int in_test_run(void) {
  if (Pa_Initialize() != paNoError) {
    printf("TDP: PortAudio indisponible\n");
    return -1;
  }

  /* Trouve le device d'entree par defaut. */
  PaDeviceIndex in_dev = Pa_GetDefaultInputDevice();
  if (in_dev == (PaDeviceIndex)-1) {
    printf("TDP: aucune entree par defaut\n");
    Pa_Terminate();
    return 0;
  }
  const PaDeviceInfo *info = Pa_GetDeviceInfo(in_dev);
  if (!info || info->maxInputChannels < 1) {
    printf("TDP: entree par defaut injouable\n");
    Pa_Terminate();
    return 0;
  }

  printf("TDP: test de l'entree — parle dans le micro (5 s)\n");
  printf("    entree : %s\n\n", info->name);

  /* Parameters d'entree : mono, 48 kHz, float32. */
  PaStreamParameters in_params;
  in_params.device = in_dev;
  in_params.channelCount = 1;
  in_params.sampleFormat = paFloat32;
  in_params.suggestedLatency = info->defaultLowInputLatency;
  in_params.hostApiSpecificStreamInfo = NULL;

  PaStream *stream = NULL;
  PaError err = Pa_OpenStream(
      &stream,
      &in_params,    /* entree */
      NULL,          /* pas de sortie */
      48000.0,       /* sample rate */
      FRAME_COUNT,   /* frames par buffer */
      paNoFlag,
      in_test_callback,
      NULL);

  if (err != paNoError) {
    printf("TDP: impossible d'ouvrir l'entree (err=%d)\n", err);
    Pa_Terminate();
    return 0;
  }

  Pa_StartStream(stream);

  /* Lit pendant 5 s, affiche le volume a chaque ~200 ms. */
  int elapsed = 0;
  while (elapsed < 5000) {
    Pa_Sleep(200); /* 200 ms */
    elapsed += 200;
    float vol = g_peak;
    if (vol > 1.0f) vol = 1.0f;
    int pct = (int)(vol * 100.0f);
    /* Barre de volume (50 cases max, 2% par case). */
    int filled = pct / 2; /* 0..50 */
    if (filled > 50) filled = 50;
    printf("\r  volume : [");
    for (int i = 0; i < 50; ++i) {
      if (i < filled) printf("#");
      else printf(" ");
    }
    printf("] %3d%%", pct);
    fflush(stdout);
    /* Reset le peak pour le prochain buffer. */
    g_peak = 0.0f;
  }

  printf("\n\nTDP: test termine.\n");
  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  Pa_Terminate();
  return 0;
}
