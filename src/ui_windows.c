/*
 * Implémentation UI console Windows (sans ncurses). (FR-004..FR-010, NFR-003/004)
 * Les appels Windows sont confinés à ce fichier ; include/ui.h reste portable.
 *
 * Rendu : blocs Unicode █▓░ (UTF-16 via WriteConsoleW), couleurs ANSI
 * (Windows 10+, repli monochrome sinon), titre de fenêtre, double-buffer.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"

static HANDLE hOut = NULL;
static HANDLE hIn = NULL;
static int g_cols = 80, g_lines = 24;
static int g_ansicolor = 0;

#define MAXW 200
#define MAXH 120
static wchar_t g_buf[MAXH][MAXW + 1];

static void ensure_console(void) {
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (h == NULL || (GetFileType(h) & 3) != FILE_TYPE_CHAR) AllocConsole();
  hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  hIn = GetStdHandle(STD_INPUT_HANDLE);
}

int ui_init(void) {
  SetConsoleOutputCP(CP_UTF8);
  ensure_console();

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
    g_cols = csbi.dwSize.X;
    g_lines = csbi.dwSize.Y;
  }

  /* Mode raw sur l'entree : desactive echo + line buffer pour ReadConsole. */
  DWORD mode = 0;
  if (hIn && GetConsoleMode(hIn, &mode)) {
    SetConsoleMode(hIn, mode & ~(ENABLE_ECHO_OUTPUT | ENABLE_LINE_INPUT));
  }

  DWORD mode_out = 0;
  if (GetConsoleMode(hOut, &mode_out)) {
    if (SetConsoleMode(hOut, mode_out | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) g_ansicolor = 1;
  }
  return 0;
}

void ui_shutdown(void) {
  /* Restaure le mode console normal. */
  if (hIn) {
    DWORD mode = 0;
    if (GetConsoleMode(hIn, &mode)) {
      SetConsoleMode(hIn, mode | ENABLE_ECHO_OUTPUT | ENABLE_LINE_INPUT);
    }
  }
  if (g_ansicolor) {
    const char *r = "\x1b[0m";
    WriteConsoleA(hOut, r, (DWORD)strlen(r), NULL, NULL);
  }
}

void ui_clear(void) {
  for (int y = 0; y < g_lines && y < MAXH; ++y) {
    for (int x = 0; x < g_cols && x < MAXW; ++x) g_buf[y][x] = L' ';
    g_buf[y][g_cols < MAXW ? g_cols : MAXW - 1] = L'\0';
  }
}

/* Converting UTF-8 -> wide and placing into the double-buffer at (row, col). */
static void buf_put(int row, int col, const char *utf8) {
  if (row < 0 || row >= g_lines || row >= MAXH) return;
  int c = col;
  const unsigned char *p = (const unsigned char *)utf8;
  while (*p && c < g_cols && c < MAXW) {
    int n;
    if (p[0] < 0x80) n = 1;
    else if ((p[0] & 0xE0) == 0xC0) n = 2;
    else if ((p[0] & 0xF0) == 0xE0) n = 3;
    else { n = 1; }
    wchar_t w[2];
    int wn = MultiByteToWideChar(CP_UTF8, 0, (const char *)p, n, w, 2);
    if (wn > 0 && c < MAXW) g_buf[row][c++] = w[0];
    p += n;
  }
  if (c >= 0 && c < MAXW) g_buf[row][c] = L'\0';
}

void ui_cursor(int row, int col) { (void)row; (void)col; }

void ui_text(int row, int col, const char *utf8) { buf_put(row, col, utf8); }

void ui_color(ui_color_t c, int on) {
  if (!g_ansicolor) return;
  const char *seq = NULL;
  if (on) {
    if (c == UI_COLOR_BYPASS) seq = "\x1b[1;91m";
    else if (c == UI_COLOR_SELECTED) seq = "\x1b[1;96m";
  } else seq = "\x1b[0m";
  if (seq) WriteConsoleA(hOut, seq, (DWORD)strlen(seq), NULL, NULL);
}

void ui_title(const char *utf8) { SetConsoleTitleA(utf8); }

ui_key ui_poll_key(void) {
  if (hIn == NULL) return UI_KEY_NONE;

  /* Non bloquant : verifie s'il y a une touche en attente. */
  DWORD n = 0;
  if (!PeekConsoleInput(hIn, NULL, 1, &n) || n == 0) return UI_KEY_NONE;

  /* Lit la touche. */
  wchar_t ch = 0;
  DWORD read = 0;
  if (!ReadConsoleW(hIn, &ch, 1, &read) || read == 0) return UI_KEY_NONE;

  switch (ch) {
    case 0:              return UI_KEY_NONE; /* touche speciale (fleches) : pas geree ici */
    case 'M': case 'm': return UI_KEY_M;
    case 'R': case 'r': return UI_KEY_R;
    case 'I': case 'i': return UI_KEY_I;
    case 'O': case 'o': return UI_KEY_O;
    case 'Q': case 'q': return UI_KEY_Q;
    case ' ':            return UI_KEY_SPACE;
    default:             return UI_KEY_NONE;
  }
}

int ui_cols(void)  { return g_cols; }
int ui_lines(void) { return g_lines; }

void ui_frame(void) {
  SetConsoleCursorPosition(hOut, (COORD){0, 0});
  for (int y = 0; y < g_lines && y < MAXH; ++y) {
    DWORD len = (DWORD)wcslen(g_buf[y]);
    if (len == 0) continue;
    WriteConsoleW(hOut, g_buf[y], len, NULL, NULL);
    SetConsoleCursorPosition(hOut, (COORD){0, (SHORT)(y + 1)});
  }
}

void ui_sleep_ms(int ms) { Sleep(ms); }
