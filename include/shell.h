/* Shell de commandes TDP CLI. */
#ifndef TDP_SHELL_H
#define TDP_SHELL_H

#include "pedals.h"

/* Lance le shell. script_path NULL = mode interactif (stdin),
 * sinon lecture d'un fichier (mode script). */
void shell_run(const char *script_path);

/* Execute une ligne de commande (in-process, pour le mode serveur).
 * La sortie est ecrite dans stdout (a rediriger). */
void shell_exec_line(const char *line);

/* Initialise l'etat audio + pedales (pour le mode serveur).
 * Appeler avant shell_exec_line. */
void shell_server_init(void);

/* Parser d'une ligne .tdp : ajoute la pedale a la chaine.
 * Retourne 0 si ok (ou ligne vide/commentaire), -1 si erreur. */
int parse_tdp_line(const char *line, int lineno, chain_t *c);

#endif /* TDP_SHELL_H */
