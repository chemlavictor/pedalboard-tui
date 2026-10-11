/*
 * TDP — boucle UI commune (Linux ncurses / Windows console).
 * Le rendu et le clavier passent par l'interface ui_* (include/ui.h) ;
 * seule l'implémentation diffère (src/ui_ncurses.c / src/ui_windows.c).
 */
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <wincon.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "params.h"
#include "audio.h"
#include "ui.h"

/* Paramètre sélectionné : état local au thread UI (pas partagé). */
enum { SEL_GAIN = 0, SEL_MASTER = 1 };
static int g_selected = SEL_GAIN;

static volatile sig_atomic_t g_signal_quit = 0;

#ifndef _WIN32
static void on_sigint(int sig) { (void)sig; g_signal_quit = 1; }
#else
static BOOL WINAPI ctrl_handler(DWORD type) { (void)type; g_signal_quit = 1; return TRUE; }
#endif

static float af_load(const _Atomic float *a) { return atomic_load_explicit(a, memory_order_relaxed); }
static void af_store(_Atomic float *a, float v) { atomic_store_explicit(a, v, memory_order_relaxed); }

static float rms_to_db(float rms) {
  if (rms <= 0.0f) return -60.0f;
  float db = 20.0f * log10f(rms);
  return db < -60.0f ? -60.0f : db;
}

/* Construit une jauge de `cells` cases. fill/empty : 1 octet (ASCII) ou 2 (UTF-8). */
static void gauge_str(char *buf, int cells, float frac, const char *fill, const char *empty) {
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  int filled = (int)(frac * cells + 0.5f);
  int p = 1; /* après '[' */
  buf[0] = '[';
  for (int i = 0; i < cells; ++i) {
    const char *s = (i < filled) ? fill : empty;
    int n = (s[1] != 0) ? 2 : 1;
    memcpy(buf + p, s, n);
    p += n;
  }
  buf[p] = ']'; buf[p + 1] = '\0';
}

static void render(void) {
  ui_clear();

  bool bypass = atomic_load_explicit(&P.bypass, memory_order_relaxed);
  bool pa_ok = atomic_load_explicit(&P.pa_ok, memory_order_relaxed);
  long sr = atomic_load_explicit(&P.sr, memory_order_relaxed);
  long fb = atomic_load_explicit(&P.frame_buffer, memory_order_relaxed);
  float gain = af_load(&P.gain);
  float master = af_load(&P.master);
  int sel = g_selected;

  /* En-tête (FR-024) */
  ui_text(0, 0, "TDP - Terminal Distortion Pedal");
  ui_color(UI_COLOR_BYPASS, bypass);
  ui_text(0, ui_cols() - 10, bypass ? "[BYPASS]" : "[ON]");
  ui_color(UI_COLOR_BYPASS, 0);

  /* Paramètres (FR-025, FR-026) */
  char gbuf[64];
  ui_color(UI_COLOR_SELECTED, sel == SEL_GAIN);
  ui_text(2, 0, sel == SEL_GAIN ? "> " : "  ");
  ui_text(2, 2, "Gain   ");
  gauge_str(gbuf, 12, gain, "=", "-");
  ui_text(2, 10, gbuf);
  char pct[32]; snprintf(pct, sizeof pct, " %3.0f%%", gain * 100.0f);
  ui_text(2, 37, pct);
  ui_color(UI_COLOR_SELECTED, 0);

  ui_color(UI_COLOR_SELECTED, sel == SEL_MASTER);
  ui_text(3, 0, sel == SEL_MASTER ? "> " : "  ");
  ui_text(3, 2, "Master ");
  gauge_str(gbuf, 12, master, "=", "-");
  ui_text(3, 10, gbuf);
  snprintf(pct, sizeof pct, " %3.0f%%", master * 100.0f);
  ui_text(3, 37, pct);
  ui_color(UI_COLOR_SELECTED, 0);

  /* VU (FR-027) */
  float in_rms = af_load(&P.vu_in_rms), in_peak = af_load(&P.vu_in_peak);
  float out_rms = af_load(&P.vu_out_rms), out_peak = af_load(&P.vu_out_peak);

  /* Jauge VU de 20 cases : '#' niveau, '*' peak-hold, '-' vide. */
  char vub[64];
  { const int c=20; int f=(int)((in_rms<=0?0:in_rms>1?1:in_rms)*c+0.5); int p=(int)((in_peak<=0?0:in_peak>1?1:in_peak)*c+0.5); if(p>c-1)p=c-1; vub[0]='['; int q=1; for(int i=0;i<c;++i){vub[q++]=(i==p)?'*':(i<f)?'#':'-';} vub[q]=']'; vub[q+1]=0; }
  ui_text(5, 0, "INPUT   "); ui_text(5, 8, vub);
  { char db[48]; snprintf(db, sizeof db, "%6.1f dB  peak", rms_to_db(in_rms)); ui_text(5, 30, db); }

  { const int c=20; int f=(int)((out_rms<=0?0:out_rms>1?1:out_rms)*c+0.5); int p=(int)((out_peak<=0?0:out_peak>1?1:out_peak)*c+0.5); if(p>c-1)p=c-1; vub[0]='['; int q=1; for(int i=0;i<c;++i){vub[q++]=(i==p)?'*':(i<f)?'#':'-';} vub[q]=']'; vub[q+1]=0; }
  ui_text(6, 0, "OUTPUT  "); ui_text(6, 8, vub);
  { char db[48]; snprintf(db, sizeof db, "%6.1f dB  peak", rms_to_db(out_rms)); ui_text(6, 30, db); }

  /* Pied de page (FR-028) */
  char foot[128];
  snprintf(foot, sizeof foot, "%ld Hz - buffer %ld - PortAudio %s",
           sr, fb, pa_ok ? "OK" : "ERREUR");
  ui_text(ui_lines() - 1, 0, foot);

  /* Titre fenêtre (FR-007) */
  char title[128];
  snprintf(title, sizeof title, "TDP — Gain %.0f%% Master %.0f%% [%s]",
           gain * 100.0f, master * 100.0f, bypass ? "BYPASS" : "ON");
  ui_title(title);

  ui_frame();
}

int main(void) {
  params_init();

#ifndef _WIN32
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_sigint;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
#endif

#ifdef _WIN32
  SetConsoleCtrlHandler(ctrl_handler, TRUE);
#endif

  if (ui_init() != 0) {
    fprintf(stderr, "TDP: impossible d'initialiser l'interface (pas de console ?).\n");
    return 1;
  }

  if (!audio_start()) {
#ifdef _WIN32
    /* Pas de stream : probable portaudio.dll absente (FR-017). */
    fprintf(stderr,
      "TDP: audio indisponible. portaudio.dll introuvable ?\n"
      "      Verifiez qu'elle est a cote de tdp.exe (dans le meme dossier).\n");
#endif
    /* La TUI reste utilisable, pied de page = ERREUR. */
  }

  bool quit = false;
  while (!quit) {
    ui_key key = ui_poll_key();
    switch (key) {
      case UI_KEY_SPACE: /* BYPASS */
        atomic_store_explicit(&P.bypass, !atomic_load_explicit(&P.bypass, memory_order_relaxed), memory_order_relaxed);
        break;
      case UI_KEY_M: /* MUTE */
        atomic_store_explicit(&P.mute, !atomic_load_explicit(&P.mute, memory_order_relaxed), memory_order_relaxed);
        break;
      case UI_KEY_UP:
      case UI_KEY_DOWN: /* cycle param */
        g_selected = 1 - g_selected;
        break;
      case UI_KEY_LEFT:
      case UI_KEY_RIGHT: { /* inc/déc 0.05, clamp */
        bool right = (key == UI_KEY_RIGHT);
        _Atomic float *p = (g_selected == SEL_GAIN) ? &P.gain : &P.master;
        float cur = af_load(p);
        cur += right ? 0.05f : -0.05f;
        if (cur < 0.0f) cur = 0.0f;
        if (cur > 1.0f) cur = 1.0f;
        af_store(p, cur);
        break;
      }
      case UI_KEY_R: /* reset */
        af_store(&P.gain, 0.5f);
        af_store(&P.master, 0.75f);
        break;
      case UI_KEY_Q: /* quit */
        quit = true;
        break;
      default:
        break;
    }
    if (g_signal_quit) quit = true;

    render();
    ui_sleep_ms(20); /* ~50 fps */
  }

  audio_stop();
  ui_shutdown();
  return 0;
}
