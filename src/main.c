/* TDP CLI — point d'entrée.
 * ./tdp            -> shell interactif
 * ./tdp <script>   -> execute le script ligne par ligne
 * L'interface est un shell de commandes (src/shell.c), portable Windows/Linux.
 */
#include <stdio.h>
#include <stdlib.h>

#include "shell.h"

#ifndef TDP_GIT_HASH
#define TDP_GIT_HASH "nogit"
#endif

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  /* L'argument optionnel (script) est géré dans shell_run. */
  const char *script = (argc >= 2) ? argv[1] : NULL;
  shell_run(script);
  return 0;
}
