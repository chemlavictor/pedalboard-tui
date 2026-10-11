/* Encodeur WAV PCM 16-bit mono (portable, sans dépendance). */
#ifndef TDP_WAV_H
#define TDP_WAV_H

/* Écrit un fichier WAV PCM 16-bit mono.
 * samples : buffer float (post-pédales), n : nb d'échantillons, sr : Hz.
 * Retourne 0 si ok, -1 si échec (fichier non ouvrable). */
int wav_write(const char *path, const float *samples, long n, double sr);

/* Génère un nom de fichier horodaté : recordings/YYYY-MM-DD_HH-MM-SS.wav.
 * Crée le dossier recordings/ s'il n'existe pas.
 * Retourne 0 si ok. */
int wav_timestamped_name(char *out, int out_len);

#endif /* TDP_WAV_H */
