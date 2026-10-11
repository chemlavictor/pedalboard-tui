#ifndef TDP_AUDIO_H
#define TDP_AUDIO_H

#include <stdbool.h>

/* Nombre max de périphériques listés (entrée + sortie). */
#define TDP_MAX_DEVICES 32
#define TDP_DEVICE_NAME_LEN 64

/* Info d'un périphérique audio. */
typedef struct {
  int id;                 /* index PortAudio */
  char name[TDP_DEVICE_NAME_LEN];
  bool is_input;          /* true si peut servir d'entrée */
  bool is_output;         /* true si peut servir de sortie */
} tdp_device;

/*
 * Ouvre le stream audio (mono in+out) et démarre le thread audio.
 * Demande 48000 Hz / 256 échantillons, fallback en cascade.
 * Retourne true si le stream est ouvert ; false en cas d'échec.
 */
bool audio_start(void);

/* Arrête et ferme le stream audio. Idempotent. */
void audio_stop(void);

/*
 * Remplie `out` avec la liste des périphériques PortAudio.
 * Retourne le nombre de périphériques trouvés (<= TDP_MAX_DEVICES).
 * L'app doit avoir appelé Pa_Initialize (fait par audio_start).
 */
int audio_list_devices(tdp_device *out);

/*
 * Ré-ouvre le stream avec les périphériques choisis.
 * `in_id` / `out_id` : index PortAudio (ou -1 pour défaut).
 * Retourne true si le stream est ré-ouvert ; false sinon.
 */
bool audio_set_devices(int in_id, int out_id);

#endif /* TDP_AUDIO_H */
