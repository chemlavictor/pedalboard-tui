/* Tests de l'encodeur WAV + nom horodaté (FR-006). */
#include "wav.h"
#include "pedals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { printf("FAIL: %s (ligne %d)\n", msg, __LINE__); failures++; } \
  else { printf("ok:   %s\n", msg); } \
} while (0)

int main(void) {
  /* 1) wav_write : en-tête correct + samples. */
  {
    const char *path = "test_out.wav";
    /* Signal : 100 échantillons, sinusoïde amplitude 0.5. */
    const long n = 100;
    static float buf[100];
    for (long i = 0; i < n; ++i) buf[i] = 0.5f * (float)sin(6.28318 * 440.0 * i / 48000.0);
    CHECK(wav_write(path, buf, n, 48000.0) == 0, "wav_write retourne 0");

    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "fichier WAV créé");
    if (f) {
      char riff[4];
      uint32_t chunk_size;
      char wave[4];
      fread(riff, 1, 4, f);
      fread(&chunk_size, 4, 1, f);
      fread(wave, 1, 4, f);
      CHECK(memcmp(riff, "RIFF", 4) == 0, "en-tête RIFF");
      CHECK(memcmp(wave, "WAVE", 4) == 0, "en-tête WAVE");
      CHECK(chunk_size == 36 + n * 2, "chunk_size = 36 + data");

      /* Lit le fmt chunk. */
      char fmt[4];
      uint32_t fmt_size;
      uint16_t audio_format, num_channels, block_align, bits;
      uint32_t sample_rate, byte_rate;
      fread(fmt, 1, 4, f);
      fread(&fmt_size, 4, 1, f);
      fread(&audio_format, 2, 1, f);
      fread(&num_channels, 2, 1, f);
      fread(&sample_rate, 4, 1, f);
      fread(&byte_rate, 4, 1, f);
      fread(&block_align, 2, 1, f);
      fread(&bits, 2, 1, f);
      CHECK(memcmp(fmt, "fmt ", 4) == 0, "chunk fmt");
      CHECK(audio_format == 1, "format PCM (1)");
      CHECK(num_channels == 1, "mono (1 canal)");
      CHECK(sample_rate == 48000, "sample rate 48000");
      CHECK(bits == 16, "16 bits");
      CHECK(block_align == 2, "block align 2 (16-bit mono)");

      /* data chunk. */
      char data[4];
      uint32_t data_size;
      fread(data, 1, 4, f);
      fread(&data_size, 4, 1, f);
      CHECK(memcmp(data, "data", 4) == 0, "chunk data");
      CHECK(data_size == n * 2, "data_size = n * 2");

      /* Premier sample : 0.5*sin(0) = 0 -> int16 0. */
      short s0;
      fread(&s0, 2, 1, f);
      CHECK(s0 == 0, "premier sample ≈ 0");
      fclose(f);
    }
    remove(path);
  }

  /* 2) Clamp : samples > 1.0 sont clampés à 32767. */
  {
    const char *path = "test_clamp.wav";
    float buf[2] = {2.0f, -3.0f}; /* hors plage */
    wav_write(path, buf, 2, 48000.0);
    FILE *f = fopen(path, "rb");
    if (f) {
      fseek(f, 44, SEEK_SET); /* saute l'en-tête (44 bytes) */
      short s0, s1;
      fread(&s0, 2, 1, f);
      fread(&s1, 2, 1, f);
      CHECK(s0 == 32767, "sample > 1 clampé à 32767");
      CHECK(s1 == -32767, "sample < -1 clampé à -32767");
      fclose(f);
    }
    remove(path);
  }

  /* 3) Nom horodaté : format + dossier recordings/. */
  {
    char path[512];
    CHECK(wav_timestamped_name(path, sizeof path) == 0, "timestamped_name retourne 0");
    CHECK(strstr(path, "recordings/") != NULL, "chemin contient recordings/");
    CHECK(strstr(path, ".wav") != NULL, "chemin se termine par .wav");
    /* Format : recordings/YYYY-MM-DD_HH-MM-SS.wav (23+ chars de base). */
    CHECK(strlen(path) >= strlen("recordings/2020-01-01_00-00-00.wav"), "nom horodaté complet");
    printf("  (nom généré: %s)\n", path);
  }

  printf("\n%s (%d échec%s)\n", failures ? "TESTS ÉCHEUÉS" : "TOUS LES TESTS PASSENT",
         failures, failures > 1 ? "s" : "");
  return failures ? 1 : 0;
}
