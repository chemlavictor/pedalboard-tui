#!/usr/bin/env python3
"""Serveur web TPD : relaye les commandes vers tdp --serve (TCP).
Lance tdp --serve en arriere-plan, puis relaye entre le navigateur
(WebSocket) et TPD (TCP). Le VU est poll par le navigateur (status).

Usage : python3 web/server.py [port_web]
Puis ouvre http://localhost:port_web dans le navigateur.
"""
import http.server
import socketserver
import subprocess
import json
import sys
import os
import socket
import threading
import time

WEB_PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
TPD_PORT = WEB_PORT + 1  # Port TCP de tdp --serve.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(SCRIPT_DIR)

# Chemin du binaire TPD.
TPD_BIN = None
for candidate in [
    os.path.join(REPO_DIR, "build", "tdp"),
    os.path.join(REPO_DIR, "build", "tdp.exe"),
    os.path.join(REPO_DIR, "tdp"),
    "tdp",
    "tdp.exe",
]:
    if os.path.exists(candidate):
        TPD_BIN = candidate
        break

if TPD_BIN is None:
    print("ERREUR : binaire TPD introuvable. Lance 'make' d'abord.")
    sys.exit(1)

# Session TCP avec TPD (un seul client a la fois, simplifie).
class TPDSession:
    def __init__(self):
        self.sock = None
        self.lock = threading.Lock()
        self.process = None

    def start(self):
        """Lance tdp --serve en arriere-plan et connecte le socket."""
        self.process = subprocess.Popen(
            [TPD_BIN, "--serve", str(TPD_PORT)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        # Attend que le serveur soit pret.
        time.sleep(0.5)
        for _ in range(10):
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.connect(("localhost", TPD_PORT))
                self.sock.settimeout(2.0)
                return True
            except ConnectionRefusedError:
                time.sleep(0.3)
        return False

    def send_cmd(self, cmd):
        """Envoie une commande et renvoie la reponse."""
        with self.lock:
            if not self.sock:
                return "(non connecte)"
            try:
                self.sock.sendall((cmd + "\n").encode())
                # Lit la reponse (jusqu'a timeout ou fin).
                chunks = []
                self.sock.settimeout(1.0)
                while True:
                    try:
                        data = self.sock.recv(4096)
                        if not data:
                            break
                        chunks.append(data.decode(errors="replace"))
                    except socket.timeout:
                        break
                return "".join(chunks)
            except Exception as e:
                return f"(erreur: {e})"

    def stop(self):
        """Arrete le processus TPD."""
        try:
            if self.sock:
                self.sock.sendall(b"quit\n")
                self.sock.close()
        except Exception:
            pass
        if self.process:
            self.process.terminate()

session = TPDSession()

class TPDHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/cmd":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        try:
            data = json.loads(body)
            cmd = data.get("cmd", "").strip()
        except json.JSONDecodeError:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"invalid JSON")
            return

        if not cmd:
            result = {"output": "(vide)", "exit": 0}
        else:
            output = session.send_cmd(cmd)
            result = {"output": output, "exit": 0}

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(result).encode())

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            index = os.path.join(SCRIPT_DIR, "index.html")
            if os.path.exists(index):
                with open(index, "rb") as f:
                    content = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.end_headers()
                self.wfile.write(content)
            else:
                self.send_response(404)
                self.end_headers()
        elif self.path == "/status":
            result = {
                "tdp_bin": TPD_BIN,
                "tdp_port": TPD_PORT,
                "connected": session.sock is not None,
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(result).encode())
        else:
            self.send_error(404)

    def log_message(self, format, *args):
        pass

class ReuseTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

def main():
    print(f"Demarrage de tdp --serve sur le port {TPD_PORT}...")
    if not session.start():
        print("ERREUR : impossible de connecter a tdp --serve.")
        sys.exit(1)
    print(f"TPD Web — ouvre http://localhost:{WEB_PORT} dans ton navigateur")
    print(f"Binaire TPD : {TPD_BIN}")

    with ReuseTCPServer(("", WEB_PORT), TPDHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nArret du serveur.")
        finally:
            session.stop()

if __name__ == "__main__":
    main()
