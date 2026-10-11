/* Encodeur WAV PCM 16-bit mono + nom horodaté. */
#include "wav.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>

/* Écrit l'en-tête RIFF/WAVE (44 bytes) + les données PCM 16-bit. */
int wav_write(const char *path, const float *samples, long n, double sr) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;

  uint32_t data_size = (uint32_t)n * 2; /* 16-bit mono */
  uint32_t chunk_size = 36 + data_size;

  /* RIFF header */
  fwrite("RIFF", 1, 4, f);
  fwrite(&chunk_size, 4, 1, f);
  fwrite("WAVE", 1, 4, f);
  /* fmt chunk */
  fwrite("fmt ", 1, 4, f);
  uint32_t fmt_size = 16;
  fwrite(&fmt_size, 4, 1, f);
  uint16_t audio_format = 1; /* PCM */
  uint16_t num_channels = 1;
  uint32_t sample_rate = (unsigned int)sr;
  uint16_t bits_per_sample = 16;
  uint16_t block_align = 2;
  uint32_t byte_rate = (unsigned int)sr * 2;
  fwrite(&audio_format, 2, 1, f);
  fwrite(&num_channels, 2, 1, f);
  fwrite(&sample_rate, 4, 1, f);
  fwrite(&byte_rate, 4, 1, f);
  fwrite(&block_align, 2, 1, f);
  fwrite(&bits_per_sample, 2, 1, f);
  /* data chunk */
  fwrite("data", 1, 4, f);
  fwrite(&data_size, 4, 1, f);

  /* Samples float -> int16 (clamp + scale). */
  for (long i = 0; i < n; ++i) {
    float v = samples[i];
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    short s = (short)(v * 32767.0f);
    fwrite(&s, 2, 1, f);
  }

  fclose(f);
  return 0;
}

/* Nom horodaté : recordings/YYYY-MM-DD_HH-MM-SS.wav (crée recordings/). */
int wav_timestamped_name(char *out, int out_len) {
  /* Crée le dossier recordings/ s'il n'existe pas (mkdir ignore si existe). */
  mkdir("recordings", 0755);

  time_t t = time(NULL);
  struct tm *lt = localtime(&t);
  char base[64];
  strftime(base, sizeof base, "%Y-%m-%d_%H-%M-%S", lt);

  int w = snprintf(out, out_len, "recordings/%s.wav", base);
  return (w >= 0 && w < out_len) ? 0 : -1;
}
