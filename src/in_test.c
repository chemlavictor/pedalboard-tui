/* Test des entrees audio : lit le volume d'un device (ou tous) d'entree.
 * Si device >= 0 : teste ce device pendant duration_ms.
 * Si device < 0 : teste tous les devices (2 s chacun). */
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
static float g_peak = 0.0f;

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
  g_peak = peak;
  return paContinue;
}

/* Copie un nom de device en remplacant les caracteres de controle par des
 * espaces, pour afficher sur une seule ligne. */
static void safe_name(const char *in, char *out, int outsz) {
  int n = 0;
  for (int i = 0; in[i] && n < outsz - 1; ++i) {
    char c = in[i];
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    out[n++] = c;
  }
  out[n] = '\0';
}

/* Teste un device d'entree specifique pendant duration_ms.
 * Affiche le volume en continu (toutes les 200 ms). */
static void test_one_input(int dev, int duration_ms) {
  const PaDeviceInfo *info = Pa_GetDeviceInfo(dev);
  if (!info || info->maxInputChannels < 1) {
    printf("(device %d n'est pas une entree)\n", dev);
    return;
  }

  /* Nom du host API. */
  const char *api_name = "?";
  if (info->hostApi >= 0) {
    const PaHostApiInfo *api_info = Pa_GetHostApiInfo(info->hostApi);
    if (api_info) api_name = api_info->name;
  }

  char name[128];
  safe_name(info->name, name, sizeof name);
  printf("  entree : [%2d]  [%-12s]  %s\n", dev, api_name, name);
  printf("  (parle dans le micro, %d s)\n\n", duration_ms / 1000);

  PaStreamParameters in_params;
  in_params.device = dev;
  in_params.channelCount = 1;
  in_params.sampleFormat = paFloat32;
  in_params.suggestedLatency = info->defaultLowInputLatency;
  in_params.hostApiSpecificStreamInfo = NULL;

  PaStream *stream = NULL;
  PaError err = Pa_OpenStream(
      &stream, &in_params, NULL, 48000.0, FRAME_COUNT,
      paNoFlag, in_test_callback, NULL);
  if (err != paNoError) {
    printf("(impossible d'ouvrir l'entree, err=%d)\n", err);
    return;
  }

  Pa_StartStream(stream);
  int elapsed = 0;
  while (elapsed < duration_ms) {
    Pa_Sleep(200);
    elapsed += 200;
    float vol = g_peak;
    if (vol > 1.0f) vol = 1.0f;
    int pct = (int)(vol * 100.0f);
    int filled = pct / 2;
    if (filled > 50) filled = 50;
    printf("\r  volume : [");
    for (int i = 0; i < 50; ++i) printf(i < filled ? "#" : " ");
    printf("] %3d%%", pct);
    fflush(stdout);
    g_peak = 0.0f;
  }
  printf("\n\n");
  Pa_StopStream(stream);
  Pa_CloseStream(stream);
}

int in_test_run(int device, int duration_ms) {
  if (Pa_Initialize() != paNoError) {
    printf("TPD: PortAudio indisponible\n");
    return -1;
  }

  if (device >= 0) {
    /* Teste un device specifique. */
    test_one_input(device, duration_ms);
    printf("TPD: test termine.\n");
    Pa_Terminate();
    return 0;
  }

  /* Teste tous les devices d'entree (2 s chacun). */
  int n_dev = Pa_GetDeviceCount();
  if (n_dev <= 0) {
    printf("TPD: aucun peripherique audio\n");
    Pa_Terminate();
    return 0;
  }

  printf("TPD: test des entrees — parle dans le micro sur chaque peripherique\n");
  printf("    (lecture 2 s par device, montre le volume max percu)\n\n");

  int tested = 0;
  for (int i = 0; i < n_dev; ++i) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
    if (!info || info->maxInputChannels < 1) continue;

    const char *api_name = "?";
    if (info->hostApi >= 0) {
      const PaHostApiInfo *api_info = Pa_GetHostApiInfo(info->hostApi);
      if (api_info) api_name = api_info->name;
    }

    char name[128];
    safe_name(info->name, name, sizeof name);
    printf("  [%2d]  [%-12s]  %s ... ", i, api_name, name);
    fflush(stdout);

    PaStreamParameters in_params;
    in_params.device = i;
    in_params.channelCount = 1;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = info->defaultLowInputLatency;
    in_params.hostApiSpecificStreamInfo = NULL;

    PaStream *stream = NULL;
    PaError err = Pa_OpenStream(
        &stream, &in_params, NULL, 48000.0, FRAME_COUNT,
        paNoFlag, in_test_callback, NULL);
    if (err != paNoError) {
      printf("(device injouable, err=%d)\n", err);
      continue;
    }

    g_peak = 0.0f;
    float max_vol = 0.0f;
    Pa_StartStream(stream);
    int elapsed = 0;
    while (elapsed < 2000) {
      Pa_Sleep(100);
      elapsed += 100;
      if (g_peak > max_vol) max_vol = g_peak;
    }
    Pa_StopStream(stream);
    Pa_CloseStream(stream);

    if (max_vol > 1.0f) max_vol = 1.0f;
    int pct = (int)(max_vol * 100.0f);
    printf("volume max %3d%%\n", pct);
    tested++;
  }

  if (tested == 0) printf("  (aucune entree)\n");
  printf("\nTPD: test termine. Note le numero du device qui a capt du son.\n");
  Pa_Terminate();
  return 0;
}
