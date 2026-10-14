/* Test des peripheriques audio : joue un bip sur chaque sortie. */
#ifndef TPD_DEVTEST_H
#define TPD_DEVTEST_H

/* Joue un bip (440 Hz) sur les devices de sortie.
 * device >= 0 : joue le bip sur ce device specifique pendant duration_ms.
 * device < 0  : joue le bip sur chaque device (0.5 s chacun, espace 1 s).
 * api_filter : si non-NULL et device < 0, ne teste que les devices dont
 *   l'API contient cette chaine (ex. "WASAPI", "MME"). NULL = tous.
 * Retourne 0 si ok, -1 si PortAudio indisponible. */
int devtest_test_outputs(int device, int duration_ms, const char *api_filter);

/* Liste les noms des host API disponibles (pour la completion).
 * Renvoie un tableau de chaine termin par NULL, max n entrees. */
void devtest_list_apis(const char **out, int n);

#endif /* TPD_DEVTEST_H */
