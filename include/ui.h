#ifndef TDP_UI_H
#define TDP_UI_H

/*
 * Interface de rendu/entrée commune entre la cible Linux (ncurses) et la
 * cible Windows (console native). Le main.c n'appelle que ces primitives ;
 * l'implémentation diffère via src/ui_ncurses.c et src/ui_windows.c.
 *
 * Aucune dépendance ncurses ni <windows.h> dans ce header (NFR-003).
 */

typedef enum {
  UI_KEY_NONE = 0,
  UI_KEY_UP,
  UI_KEY_DOWN,
  UI_KEY_LEFT,
  UI_KEY_RIGHT,
  UI_KEY_SPACE,
  UI_KEY_M,
  UI_KEY_R,
  UI_KEY_Q
} ui_key;

/* Couleurs sémantiques (le repli monochrome est géré par l'implémentation). */
typedef enum {
  UI_COLOR_RESET = 0,
  UI_COLOR_BYPASS,   /* surbrillance état BYPASS */
  UI_COLOR_SELECTED  /* ligne du paramètre sélectionné */
} ui_color_t;

/* Initialise l'UI. Retourne 0 si OK, non nul sinon. */
int  ui_init(void);
/* Libère/restaure l'UI (couleur, curseur, console). */
void ui_shutdown(void);

/* Efface l'écran (à appeler avant un rendu complet). */
void ui_clear(void);
/* Positionne le curseur (ligne, colonne, 0-indexées). */
void ui_cursor(int row, int col);
/* Écrit une chaîne UTF-8 à la position courante. */
void ui_text(int row, int col, const char *utf8);

/* Active/désactive une couleur sémantique. */
void ui_color(ui_color_t c, int on);
/* Définit le titre de la fenêtre console. */
void ui_title(const char *utf8);

/* Renvoie la prochaine touche (non bloquant) ou UI_KEY_NONE. */
ui_key ui_poll_key(void);

/* Dimensions de la console. */
int ui_cols(void);
int ui_lines(void);

/* Valide le rendu courant (double-buffer Windows / refresh ncurses). */
void ui_frame(void);

/* Pause d'environ `ms` millisecondes (boucle UI ~30-60 fps). */
void ui_sleep_ms(int ms);

#endif /* TDP_UI_H */
