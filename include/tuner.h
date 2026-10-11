/* Accordeur : detecte la frequence fondamentale d'un signal mono et la
 * compare a la note la plus proche. Utilise l'autocorrelation (robuste
 * pour les notes de guitare). */
#ifndef TDP_TUNER_H
#define TDP_TUNER_H

/* Resultat de l'analyse d'un buffer d'entree. */
typedef struct {
  float freq;        /* frequence fondamentale en Hz (0 si non detectee) */
  int   has_note;    /* 1 si une frequence fiable est detectee */
  char  note[4];     /* note la plus proche (ex "E2", "A4") */
  int   cents;       /* ecart en centiemes (+ = trop aigu, - = trop grave) */
  float confidence;  /* 0..1 (qualite de la detection) */
} tuner_result;

/* Analyse un buffer mono (float, amplitude [-1,1]) a l'echantillonnage sr.
 * n = nb d'echantillons. Remplit out.
 * Portable : pas de malloc (buffers statiques internes). */
void tuner_analyze(const float *buf, int n, double sr, tuner_result *out);

/* Affiche le resultat d'un facon lisible :
 *   E2  -12 cents (trop grave)  [confiance 87%] */
void tuner_print(const tuner_result *r);

#endif /* TDP_TUNER_H */
