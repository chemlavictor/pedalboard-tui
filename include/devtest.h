/* Test des peripheriques audio : joue un bip sur chaque sortie. */
#ifndef TDP_DEVTEST_H
#define TDP_DEVTEST_H

/* Joue un bip (440 Hz, 0.5 s) sur chaque device de sortie, en les
 * espaçant de ~1 s. Affiche le nom + l'API du device avant chaque bip.
 * api_filter : si non-NULL, ne teste que les devices dont l'API contient
 * cette chaine (ex. "WASAPI", "MME"). NULL = tous les devices.
 * Retourne 0 si ok, -1 si PortAudio indisponible. */
int devtest_test_outputs(const char *api_filter);

/* Liste les noms des host API disponibles (pour la completion).
 * Renvoie un tableau de chaine termin par NULL, max n entrees. */
void devtest_list_apis(const char **out, int n);

#endif /* TDP_DEVTEST_H */
