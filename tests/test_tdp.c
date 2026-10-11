/* Tests du parser .tdp + load/chain (FR-004).
 * On teste parse_tdp_line en le rendant accessible via un header de test,
 * ou en recréant le comportement. Ici on teste via la fonction publique
 * du shell si exposée, sinon on duplique la logique. Pour rester simple,
 * on expose parse_tdp_line via un prototype dans ce fichier (linkage).
 */
#include "pedals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prototype de la fonction interne du shell (définie dans shell.c).
 * Pour être testable, on la déclare ici ; le Makefile compile shell.c
 * avec le test. */
extern int parse_tdp_line(const char *line, int lineno, chain_t *c);

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { printf("FAIL: %s (ligne %d)\n", msg, __LINE__); failures++; } \
  else { printf("ok:   %s\n", msg); } \
} while (0)

int main(void) {
  /* 1) .tdp valide -> chaine correcte (ordre + params) */
  {
    chain_t c = {0};
    CHECK(parse_tdp_line("booster level=2.0", 1, &c) == 0, "ligne booster ok");
    CHECK(parse_tdp_line("distortion gain=0.6", 2, &c) == 0, "ligne distortion ok");
    CHECK(parse_tdp_line("volume level=0.8", 3, &c) == 0, "ligne volume ok");
    CHECK(c.len == 3, "chaine contient 3 pedales");
    CHECK(c.defs[0] == pedal_find("booster"), "pedale 1 = booster");
    CHECK(c.defs[1] == pedal_find("distortion"), "pedale 2 = distortion");
    CHECK(c.defs[2] == pedal_find("volume"), "pedale 3 = volume");
    CHECK(c.state[0].v[0] == 2.0f, "booster level=2.0");
    CHECK(c.state[1].v[0] == 0.6f, "distortion gain=0.6");
    CHECK(c.state[2].v[0] == 0.8f, "volume level=0.8");
  }

  /* 2) Pedale inconnue -> erreur */
  {
    chain_t c = {0};
    CHECK(parse_tdp_line("inconnue x=1", 1, &c) == -1, "pedale inconnue -> erreur");
    CHECK(c.len == 0, "chaine vide apres erreur");
  }

  /* 3) Commentaires et lignes vides -> ignores */
  {
    chain_t c = {0};
    CHECK(parse_tdp_line("# commentaire", 1, &c) == 0, "commentaire ignore");
    CHECK(parse_tdp_line("", 2, &c) == 0, "ligne vide ignoree");
    CHECK(parse_tdp_line("   ", 3, &c) == 0, "lignes blanches ignorees");
    CHECK(c.len == 0, "rien ajoute par commentaires/vides");
  }

  /* 4) Param hors plage -> clampé */
  {
    chain_t c = {0};
    CHECK(parse_tdp_line("booster level=5.0", 1, &c) == 0, "booster level=5 parse ok");
    CHECK(c.state[0].v[0] == 2.0f, "booster level clampé a 2.0 (max)");
  }

  /* 5) Param inconnu -> erreur */
  {
    chain_t c = {0};
    CHECK(parse_tdp_line("booster bogus=1", 1, &c) == -1, "param inconnu -> erreur");
  }

  /* 6) Valeur par defaut si pas de param */
  {
    chain_t c = {0};
    CHECK(parse_tdp_line("distortion", 1, &c) == 0, "distortion sans param ok");
    CHECK(c.state[0].v[0] == 0.5f, "distortion gain = defaut 0.5");
  }

  /* 7) Chaine pleine (max pedales) */
  {
    chain_t c = {0};
    for (int i = 0; i < MAX_CHAIN; ++i) {
      CHECK(parse_tdp_line("volume", i + 1, &c) == 0, "ajout volume ok");
    }
    CHECK(c.len == MAX_CHAIN, "chaine pleine");
    CHECK(parse_tdp_line("volume", MAX_CHAIN + 1, &c) == -1, "depassement -> erreur");
  }

  printf("\n%s (%d échec%s)\n", failures ? "TESTS ÉCHEUÉS" : "TOUS LES TESTS PASSENT",
         failures, failures > 1 ? "s" : "");
  return failures ? 1 : 0;
}
