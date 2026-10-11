/* Test des peripheriques audio : joue un bip sur chaque sortie. */
#ifndef TDP_DEVTEST_H
#define TDP_DEVTEST_H

/* Joue un bip (440 Hz, 0.5 s) sur chaque device de sortie, en les
 * espaçant de ~1 s. Affiche le nom du device avant chaque bip.
 * Retourne 0 si ok, -1 si PortAudio indisponible. */
int devtest_test_outputs(void);

#endif /* TDP_DEVTEST_H */
