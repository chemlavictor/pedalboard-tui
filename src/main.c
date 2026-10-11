#include <ncurses.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "params.h"
#include "audio.h"

/* Paramètre sélectionné : état local au thread UI (pas partagé, FR-032). */
enum { SEL_GAIN = 0, SEL_MASTER = 1 };
static int g_selected = SEL_GAIN;

static volatile sig_atomic_t g_signal_quit = 0;

static void on_sigint(int sig) {
  (void)sig;
  g_signal_quit = 1;
}

static float af_load(const _Atomic float *a) { return atomic_load_explicit(a, memory_order_relaxed); }
static void af_store(_Atomic float *a, float v) { atomic_store_explicit(a, v, memory_order_relaxed); }

static float rms_to_db(float rms) {
  if (rms <= 0.0f) return -60.0f;
  float db = 20.0f * log10f(rms);
  return db < -60.0f ? -60.0f : db;
}

/* Jauge de 12 cases, remplie selon frac (0..1). (FR-026) */
static void draw_gauge(WINDOW *w, int x, float frac) {
  const int cells = 12;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  int filled = (int)(frac * cells + 0.5f);
  for (int i = 0; i < cells; ++i) {
    char c = (i < filled) ? '=' : '-';
    mvwaddch(w, 0, x + i, (unsigned char)(i == 0 ? '[' : (i == cells - 1 ? ']' : c)));
  }
}

/* VU : 20 cases + dB + marquage de crête. (FR-027) */
static void draw_vu(WINDOW *w, int row, const char *label, float rms, float peak) {
  const int cells = 20;
  mvwprintw(w, row, 0, "%-7s", label);
  int x0 = 8;
  float p = rms <= 0.0f ? 0.0f : rms;
  if (p > 1.0f) p = 1.0f;
  int filled = (int)(p * cells + 0.5f);
  int peak_i = (int)(peak * cells + 0.5f);
  if (peak_i > cells - 1) peak_i = cells - 1;
  for (int i = 0; i < cells; ++i) {
    char c;
    if (i == peak_i) c = '*';
    else if (i < filled) c = '#';
    else c = '-';
    mvwaddch(w, row, x0 + i, (unsigned char)(i == 0 ? '[' : (i == cells - 1 ? ']' : c)));
  }
  float db = rms_to_db(rms);
  mvwprintw(w, row, x0 + cells + 2, "%6.1f dB  peak", db);
}

static void render(void) {
  erase();

  bool bypass = atomic_load_explicit(&P.bypass, memory_order_relaxed);
  bool pa_ok = atomic_load_explicit(&P.pa_ok, memory_order_relaxed);
  long sr = atomic_load_explicit(&P.sr, memory_order_relaxed);
  long fb = atomic_load_explicit(&P.frame_buffer, memory_order_relaxed);

  float gain = af_load(&P.gain);
  float master = af_load(&P.master);
  int sel = g_selected;

  /* En-tête (FR-024). ASCII only : ncurses positionne le curseur par colonne,
   * un caractère multi-octet (em-dash) le décalerait et casserait l'affichage. */
  mvprintw(0, 0, "TDP - Terminal Distortion Pedal");
  attron(bypass ? A_BOLD | A_REVERSE : A_BOLD);
  mvprintw(0, COLS - 10, "%-8s", bypass ? "[BYPASS]" : "[ON]");
  attroff(A_BOLD);
  attroff(A_REVERSE);

  /* Paramètres (FR-025, FR-026) */
  mvprintw(2, 0, "%s Gain   ", sel == SEL_GAIN ? "> " : "  ");
  draw_gauge(stdscr, 10, gain);
  mvprintw(2, 24, " %3.0f%%", gain * 100.0f);

  mvprintw(3, 0, "%s Master ", sel == SEL_MASTER ? "> " : "  ");
  draw_gauge(stdscr, 10, master);
  mvprintw(3, 24, " %3.0f%%", master * 100.0f);

  /* VU (FR-027) */
  draw_vu(stdscr, 5, "INPUT", af_load(&P.vu_in_rms), af_load(&P.vu_in_peak));
  draw_vu(stdscr, 6, "OUTPUT", af_load(&P.vu_out_rms), af_load(&P.vu_out_peak));

  /* Pied de page (FR-028) */
  mvprintw(LINES - 1, 0, "%ld Hz \302\267 buffer %ld \302\267 PortAudio %s",
           sr, fb, pa_ok ? "OK" : "ERREUR");

  refresh();
}

int main(void) {
  params_init();

  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_sigint;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);

  if (initscr() == NULL) {
    fprintf(stderr, "TDP: impossible d'initialiser ncurses (pas de TTY ?).\n");
    return 1;
  }
  cbreak();
  noecho();
  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);
  curs_set(0);

  audio_start(); /* en cas d'échec, la TUI reste utilisable, pied de page = ERREUR */

  bool quit = false;
  while (!quit) {
    int key = getch();
    switch (key) {
      case ' ': /* BYPASS (FR-030) */
        atomic_store_explicit(&P.bypass, !atomic_load_explicit(&P.bypass, memory_order_relaxed), memory_order_relaxed);
        break;
      case 'M':
      case 'm': /* MUTE (FR-031) */
        atomic_store_explicit(&P.mute, !atomic_load_explicit(&P.mute, memory_order_relaxed), memory_order_relaxed);
        break;
      case KEY_UP:
      case KEY_DOWN: /* cycle param (FR-032) */
        g_selected = 1 - g_selected;
        break;
      case KEY_LEFT:
      case KEY_RIGHT: /* inc/déc 0.05, clamp (FR-033) */ {
        bool right = (key == KEY_RIGHT);
        _Atomic float *p = (g_selected == SEL_GAIN) ? &P.gain : &P.master;
        float cur = af_load(p);
        cur += right ? 0.05f : -0.05f;
        if (cur < 0.0f) cur = 0.0f;
        if (cur > 1.0f) cur = 1.0f;
        af_store(p, cur);
        break;
      }
      case 'R':
      case 'r': /* reset (FR-034) */
        af_store(&P.gain, 0.5f);
        af_store(&P.master, 0.75f);
        break;
      case 'q':
      case 'Q': /* quit (FR-035) */
        quit = true;
        break;
      default:
        break;
    }
    if (g_signal_quit) quit = true;

    render();
    napms(20); /* ~50 fps */
  }

  audio_stop();
  endwin();
  return 0;
}
