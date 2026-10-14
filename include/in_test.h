/* Test des entrees audio : lit le volume d'un device (ou tous) d'entree. */
#ifndef TPD_INTEST_H
#define TPD_INTEST_H

/* Teste les entrees.
 * device >= 0 : teste ce device specifique pendant duration_ms (volume en continu).
 * device < 0  : teste tous les devices d'entree (2 s chacun, volume max).
 * Retourne 0 si ok, -1 si PortAudio indisponible. */
int in_test_run(int device, int duration_ms);

#endif /* TPD_INTEST_H */
