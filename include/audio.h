#ifndef TDP_AUDIO_H
#define TDP_AUDIO_H

#include <stdbool.h>
#include <stdatomic.h>

#include "pedals.h"

/* Nombre max de périphériques listés. */
#define TDP_MAX_DEVICES 32
#define TDP_DEVICE_NAME_LEN 64

typedef struct {
  int id;
  char name[TDP_DEVICE_NAME_LEN];
  bool is_input;
  bool is_output;
} tdp_device;

/* Taille du ring buffer d'enregistrement (nb d'échantillons mono).
 * ~60 s à 48 kHz. Mémoire statique (RT-safe, pas de malloc). */
#define TDP_REC_CAPACITY 2880000

/* État partagé avec le thread audio (atomiques, lock-free). */
typedef struct {
  _Atomic bool running;    /* start/stop */
  _Atomic bool recording;  /* record actif */
  _Atomic float vu_in_rms;
  _Atomic float vu_in_peak;
  _Atomic float vu_out_rms;
  _Atomic float vu_out_peak;
  _Atomic bool pa_ok;
} audio_shared;

/* Ring buffer d'enregistrement (écrit par le RT, lu par le main). */
typedef struct {
  float *buf;        /* pointeur vers un tampon statique de TDP_REC_CAPACITY */
  unsigned head;     /* position d'écriture (RT) */
  _Atomic unsigned long long count; /* nb d'échantillons écrits */
} rec_ring;

/* Point d'entrée : initialise l'état partagé (appeler avant audio_start). */
void audio_init(audio_shared *sh, rec_ring *ring, float *rec_buf);

/* active la chaine a appliquer (le RT lit ce pointeur). */
void audio_set_chain(const chain_t *c);

/* Ouvre + démarre le stream (mono in+out). -1 = device par defaut.
 * Retourne true si le stream est ouvert. */
bool audio_start(int in_id, int out_id);

/* Arrete + ferme le stream. Idempotent. */
void audio_stop(void);

/* List les peripheriques. Retourne le nb (<= TDP_MAX_DEVICES). */
int audio_list_devices(tdp_device *out);

/* Lit la position d'écriture du ring (pour stopper l'enregistrement). */
unsigned audio_rec_head(void);
/* Reset le ring (apres sauvegarde). */
void audio_rec_reset(void);

#endif /* TDP_AUDIO_H */
