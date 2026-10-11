/* Serveur TCP TDP : ecoute les commandes et envoie l'etat (VU).
 * Mode serveur : tdp --serve <port>
 * Le client (navigateur via Python) envoie les commandes en texte,
 * le serveur repond avec la sortie + envoie l'etat periodiquement. */
#ifndef TDP_SERVER_H
#define TDP_SERVER_H

/* Lance le serveur TCP sur le port donne.
 * Retourne 0 si ok, -1 si erreur. */
int tdp_server_run(int port);

#endif /* TDP_SERVER_H */
