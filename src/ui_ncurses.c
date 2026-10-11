/* Implémentation UI console Linux via ncurses. (Cible V1, comportement inchangé.) */
#include <ncurses.h>
#include <math.h>
#include <stdio.h>

#include "ui.h"

int ui_init(void) {
  if (initscr() == NULL) return 1;
  cbreak();
  noecho();
  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);
  curs_set(0);
  return 0;
}

void ui_shutdown(void) {
  endwin();
}

void ui_clear(void) {
  erase();
}

void ui_cursor(int row, int col) {
  move(row, col);
}

void ui_text(int row, int col, const char *utf8) {
  (void)utf8;
  mvprintw(row, col, "%s", utf8);
}

void ui_color(ui_color_t c, int on) {
  if (c == UI_COLOR_BYPASS) {
    if (on) attron(A_BOLD | A_REVERSE);
    else attroff(A_BOLD | A_REVERSE);
  } else if (c == UI_COLOR_SELECTED) {
    if (on) attron(A_BOLD);
    else attroff(A_BOLD);
  }
}

void ui_title(const char *utf8) {
  (void)utf8; /* ncurses : titre de la fenêtre non géré (pas d'équivalent direct) */
}

ui_key ui_poll_key(void) {
  int key = getch();
  switch (key) {
    case ' ':          return UI_KEY_SPACE;
    case 'M': case 'm': return UI_KEY_M;
    case 'R': case 'r': return UI_KEY_R;
    case 'q': case 'Q': return UI_KEY_Q;
    case KEY_UP:       return UI_KEY_UP;
    case KEY_DOWN:     return UI_KEY_DOWN;
    case KEY_LEFT:     return UI_KEY_LEFT;
    case KEY_RIGHT:    return UI_KEY_RIGHT;
    default:           return UI_KEY_NONE;
  }
}

int ui_cols(void)  { return COLS; }
int ui_lines(void) { return LINES; }

void ui_frame(void) {
  refresh();
}

void ui_sleep_ms(int ms) {
  napms(ms);
}
