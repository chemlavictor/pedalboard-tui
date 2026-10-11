/* Test des peripheriques : joue un bip sur chaque sortie audio.
 * Aide a identifier le bon peripherique de sortie. */
#include "devtest.h"
#include <portaudio.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* Buffer de travail pour le bip (0.5 s a 48 kHz). */
#define BIP_SAMPLES 24000
static float g_bip[BIP_SAMPLES];
static unsigned g_bip_pos = 0;

/* Copie un nom de device en remplaçant les caracteres de controle
 * (\n, \r, \t) par des espaces, pour afficher sur une seule ligne. */
static void safe_name(const char *in, char *out, int outsz) {
  int n = 0;
  for (int i = 0; in[i] && n < outsz - 1; ++i) {
    char c = in[i];
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    out[n++] = c;
  }
  out[n] = '\0';
}

/* Genere un bip 440 Hz, amplitude 0.5. */
static void gen_bip(float *buf, int n, double sr) {
  for (int i = 0; i < n; ++i) {
    buf[i] = 0.5f * sinf(6.28318530718f * 440.0f * (float)i / (float)sr);
  }
}

/* Callback PortAudio : lit le buffer de bip (pas de malloc, RT-safe). */
static PaError bip_callback(const void *input, void *output,
                            unsigned long frame_count,
                            const PaStreamCallbackTimeInfo *time_info,
                            PaStreamCallbackFlags status_flags,
                            void *user_data) {
  (void)input; (void)time_info; (void)status_flags; (void)user_data;
  float *out = (float *)output;
  for (unsigned i = 0; i < frame_count; ++i) {
    out[i] = g_bip[g_bip_pos];
    g_bip_pos = (g_bip_pos + 1) % BIP_SAMPLES;
  }
  return paContinue;
}

int devtest_test_outputs(void) {
  if (Pa_Initialize() != paNoError) {
    printf("TDP: PortAudio indisponible\n");
    return -1;
  }

  int n_dev = Pa_GetDeviceCount();
  if (n_dev <= 0) {
    printf("TDP: aucun peripherique audio\n");
    Pa_Terminate();
    return 0;
  }

  gen_bip(g_bip, BIP_SAMPLES, 48000.0);

  printf("TDP: test des sorties — ecoute le bip sur chaque peripherique\n");
  printf("    (bip 440 Hz, 0.5 s, espace de 1 s)\n\n");

  for (int i = 0; i < n_dev; ++i) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
    if (!info || info->maxOutputChannels < 1) continue;

    /* Nom du host API. */
    const char *api_name = "?";
    if (info->hostApi >= 0) {
      const PaHostApiInfo *api_info = Pa_GetHostApiInfo(info->hostApi);
      if (api_info) api_name = api_info->name;
    }

    /* Nom de device sur une seule ligne. */
    char name[128];
    safe_name(info->name, name, sizeof name);
    printf("  [%2d]  [%-12s]  %s ... ", i, api_name, name);
    fflush(stdout);

    /* Parameters de sortie : mono, 48 kHz, float32. */
    PaStreamParameters out_params;
    out_params.device = i;
    out_params.channelCount = 1;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = info->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = NULL;

    PaStream *stream = NULL;
    PaError err = Pa_OpenStream(
        &stream,
        NULL,            /* pas d'entree */
        &out_params,     /* sortie */
        48000.0,         /* sample rate */
        256,             /* frames par buffer */
        paNoFlag,
        bip_callback,
        NULL);

    if (err == paNoError) {
      g_bip_pos = 0;
      Pa_StartStream(stream);
      Pa_Sleep(1000); /* joue le bip 0.5 s + pause 0.5 s */
      Pa_StopStream(stream);
      Pa_CloseStream(stream);
      printf("bip joue\n");
    } else {
      printf("(device injouable, err=%d)\n", err);
    }
  }

  printf("\nTDP: test termine. Note le numero du device qui a fait le son.\n");
  Pa_Terminate();
  return 0;
}
