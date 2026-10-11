#ifndef TDP_AUDIO_H
#define TDP_AUDIO_H

#include <stdbool.h>

/*
 * Ouvre le stream audio (mono in+out) et démarre le thread audio.
 * Demande 48000 Hz / 256 échantillons, fallback en cascade 48k -> 44k -> default.
 * Retourne true si le stream est ouvert et en cours ; false en cas d'échec.
 * L'état réel (sr, frame_buffer, pa_ok) est écrit dans P (params.h).
 */
bool audio_start(void);

/* Arrête et ferme le stream audio. Idempotent. */
void audio_stop(void);

#endif /* TDP_AUDIO_H */
