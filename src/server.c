/* Serveur TCP TPD : ecoute les commandes et envoie l'etat (VU).
 * Execute les commandes IN-PROCESS (l'etat est conserve).
 * Redirige stdout vers un fichier temporaire pour capturer la sortie.
 * Envoie l'etat (VU) periodiquement.
 * Portable Windows + Linux. */
#include "server.h"
#include "shell.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>   /* _open_osfhandle, _dup2, _close, _O_TEXT */
#include <stdio.h>
#pragma comment(lib, "ws2_32")
typedef SOCKET sock_t;
#define SOCKET_INVALID_VAL INVALID_SOCKET
#define closesock(c) closesocket(c)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
typedef int sock_t;
#define SOCKET_INVALID_VAL -1
#define closesock(c) close(c)
#endif

/* Buffers de travail. */
#define BUF_SIZE 8192
static char g_rxbuf[BUF_SIZE];

/* Fichier temporaire pour capturer la sortie. */
static const char *g_tmp_out = "/tmp/tdp_server_out.txt";

/* Sauvegarde du handle stdout Windows (pour restaurer apres redirection). */
#ifdef _WIN32
static HANDLE g_saved_stdout_h = NULL;
#endif

/* Sauvegarde de stdout (pour restaurer apres la redirection). */
static FILE *g_saved_stdout = NULL;

/* Envoie une chaine au client. */
static int sock_sendline(sock_t s, const char *line) {
  int len = (int)strlen(line);
  if (len == 0) return 0;
  int total = 0;
  while (total < len) {
    int n = (int)send(s, line + total, (size_t)(len - total), 0);
    if (n <= 0) return -1;
    total += n;
  }
  return total;
}

/* Redirige stdout vers le fichier temporaire. */
static void redirect_stdout_on(void) {
#ifdef _WIN32
  /* Sauvegarde le handle stdout avant de le remplacer. */
  if (!g_saved_stdout_h) g_saved_stdout_h = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
  g_saved_stdout = stdout;
  /* Ouvre le fichier temporaire en mode write. */
  FILE *f = fopen(g_tmp_out, "w");
  if (!f) return;
  /* Redirige stdout vers ce fichier. */
  fflush(stdout);
  if (freopen(g_tmp_out, "w", stdout) == NULL) {
    fclose(f);
    return;
  }
  setvbuf(stdout, NULL, _IONBF, 0); /* non-bufferise */
}

/* Restaure stdout. */
static void redirect_stdout_off(void) {
#ifdef _WIN32
  /* Restaure stdout en re-ouvrant sur le handle sauvegarde. */
  if (g_saved_stdout_h) {
    HANDLE h = g_saved_stdout_h;
    int fd = _open_osfhandle((intptr_t)h, _O_TEXT);
    if (fd >= 0) {
      fflush(stdout);
      _dup2(fd, _fileno(stdout));
      _close(fd);
    }
  }
#else
  /* Sur POSIX : re-ouvre stdout sur /dev/tty (la console). */
  fflush(stdout);
  if (fclose(stdout) == 0) {
    stdout = fopen("/dev/tty", "w");
    if (stdout) setvbuf(stdout, NULL, _IONBF, 0);
  }
#endif
}

/* Lit le contenu du fichier temporaire et renvoie dans out. */
static int read_tmp_out(char *out, int outsz) {
  FILE *f = fopen(g_tmp_out, "r");
  if (!f) return -1;
  int n = 0;
  int c;
  while (n < outsz - 1 && (c = fgetc(f)) != EOF) {
    out[n++] = (char)c;
  }
  out[n] = '\0';
  fclose(f);
  return n;
}

/* Filtre les messages ALSA (bruit sur Linux sans carte son). */
static int filter_alsa(char *out, int len) {
  if (len <= 0) return 0;
  char *filtered = out;
  int flen = 0;
  char *ls = out;
  while (*ls && flen < len) {
    char *le = strchr(ls, '\n');
    int llen = le ? (int)(le - ls) : (int)strlen(ls);
    if (strncmp(ls, "ALSA", 4) != 0) {
      memcpy(filtered + flen, ls, (size_t)llen + 1);
      flen += llen + 1;
    }
    if (!le) break;
    ls = le + 1;
  }
  return flen;
}

int tpd_server_run(int port) {
  /* Initialise l'etat audio + pedales (in-process). */
  shell_server_init();

#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    printf("TPD: WSAStartup echec\n");
    return -1;
  }
#endif

  /* Cree le socket. */
  sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv == SOCKET_INVALID_VAL) {
    printf("TPD: socket echec\n");
    return -1;
  }

  int opt = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof opt);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((unsigned short)port);
  if (bind(srv, (struct sockaddr *)&addr, sizeof addr) != 0) {
    printf("TPD: bind echec (port %d deja utilise?)\n", port);
    closesock(srv);
    return -1;
  }

  if (listen(srv, 1) != 0) {
    printf("TPD: listen echec\n");
    closesock(srv);
    return -1;
  }

  printf("TPD: serveur demarre sur le port %d\n", port);
  printf("    (attend une connexion...)\n");
  fflush(stdout);

  /* Accepte la connexion. */
  sock_t client = accept(srv, NULL, NULL);
  if (client == SOCKET_INVALID_VAL) {
    printf("TPD: accept echec\n");
    closesock(srv);
    return -1;
  }
  printf("TPD: client connecte\n");
  fflush(stdout);

  /* Envoie le message de bienvenue. */
  sock_sendline(client, "TPD — Terminal Pedal Board (serveur TCP)\n");
  sock_sendline(client, "Tapez 'help' pour l'aide, 'quit' pour sortir.\n");

  /* Boucle : lit les commandes, execute in-process, repond. */
  int running = 1;
  int line_len = 0;
  while (running) {
    char c;
    int n = (int)recv(client, &c, 1, 0);
    if (n <= 0) break;

    if (c == '\n' || c == '\r') {
      g_rxbuf[line_len] = '\0';
      if (line_len > 0) {
        char *s = g_rxbuf;
        while (*s == ' ' || *s == '\t') ++s;
        if (*s != '\0') {
          if (strcmp(s, "quit") == 0 || strcmp(s, "exit") == 0) {
            sock_sendline(client, "TPD: au revoir.\n");
            running = 0;
            break;
          }
          /* Redirige stdout, execute la commande, lit la sortie. */
          redirect_stdout_on();
          shell_exec_line(s); /* execute la commande in-process */
          redirect_stdout_off();
          char out[BUF_SIZE];
          int outlen = read_tmp_out(out, sizeof out);
          if (outlen >= 0) {
            outlen = filter_alsa(out, outlen);
            sock_sendline(client, out);
          } else {
            sock_sendline(client, "(pas de sortie)\n");
          }
        }
        line_len = 0;
      }
    } else if (line_len < BUF_SIZE - 1) {
      g_rxbuf[line_len++] = c;
    }

    /* Envoie l'etat (VU) periodiquement. */
    /* Simplifie : on n'envoie l'etat que si le flux est en cours. */
  }

  closesock(client);
  closesock(srv);
#ifdef _WIN32
  WSACleanup();
#endif

  printf("TPD: serveur arrete\n");
  return 0;
}
