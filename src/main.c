/* TPD CLI — point d'entrée.
 * ./tdp            -> shell interactif
 * ./tdp <script>   -> execute le script ligne par ligne
 * ./tdp --serve <port> -> mode serveur TCP (ecoute les commandes)
 * L'interface est un shell de commandes (src/shell.c), portable Windows/Linux.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "server.h"

#ifndef TPD_GIT_HASH
#define TPD_GIT_HASH "nogit"
#endif

int main(int argc, char **argv) {
  /* Mode serveur : tdp --serve <port> */
  if (argc >= 2 && strcmp(argv[1], "--serve") == 0) {
    int port = 8080;
    if (argc >= 3) port = atoi(argv[2]);
    return tpd_server_run(port);
  }

  /* Mode classique : shell interactif ou script. */
  const char *script = (argc >= 2) ? argv[1] : NULL;
  shell_run(script);
  return 0;
}
