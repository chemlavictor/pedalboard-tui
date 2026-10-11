/* Test de l'entree audio : affiche le volume percu (0-100%) pendant 5 s. */
#ifndef TDP_INTEST_H
#define TDP_INTEST_H

/* Ouvre l'entree, lit le signal pendant 5 s, affiche le volume (peak) en %
 * a chaque ~200 ms. Retourne 0 si ok, -1 si PortAudio indisponible. */
int in_test_run(void);

#endif /* TDP_INTEST_H */
