#!/usr/bin/env python3
"""Mini-serveur web pour TDP : exécute les commandes TDP via un pipe.
Usage : python3 server.py [port]
Puis ouvre http://localhost:8080 dans le navigateur.
"""
import http.server
import socketserver
import subprocess
import json
import sys
import os

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
TDP_BIN = os.path.join(os.path.dirname(__file__), "..", "build", "tdp")

# Si le binaire n'existe pas, essaie tdp.exe (Windows) ou ./tdp
if not os.path.exists(TDP_BIN):
    for candidate in [
        os.path.join(os.path.dirname(__file__), "..", "build", "tdp.exe"),
        os.path.join(os.path.dirname(__file__), "..", "tdp"),
        "tdp",
        "tdp.exe",
    ]:
        if os.path.exists(candidate):
            TDP_BIN = candidate
            break

class TDPHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/cmd":
            self.send_error(404)
            return
        # Lit le body (JSON : {"cmd": "load son_metal.tdp"}).
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
            # Exécute tdp en mode script : tdp <cmd> (une commande + quit).
            script = cmd + "\nquit\n"
            try:
                proc = subprocess.run(
                    [TDP_BIN],
                    input=script.encode(),
                    capture_output=True,
                    timeout=10,
                )
                output = proc.stdout.decode(errors="replace")
                # Supprime les messages ALSA (bruit sur Linux sans carte son).
                lines = [l for l in output.splitlines() if "ALSA" not in l]
                output = "\n".join(lines)
                result = {"output": output, "exit": proc.returncode}
            except subprocess.TimeoutExpired:
                result = {"output": "(timeout)", "exit": -1}
            except Exception as e:
                result = {"output": f"(erreur: {e})", "exit": -1}

        # Renvoie en JSON.
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(result).encode())

    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            # Sert la page HTML.
            index = os.path.join(os.path.dirname(__file__), "index.html")
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
                self.wfile.write(b"index.html not found")
        elif self.path == "/status":
            # Renvoie l'état de TDP (version, binaire).
            result = {
                "tdp_bin": TDP_BIN,
                "exists": os.path.exists(TDP_BIN),
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(result).encode())
        else:
            self.send_error(404)

    def log_message(self, format, *args):
        # Supprime le logging par défaut (plus propre).
        pass

class ReuseTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

if __name__ == "__main__":
    with ReuseTCPServer(("", PORT), TDPHandler) as httpd:
        print(f"TDP Web — ouvre http://localhost:{PORT} dans ton navigateur")
        print(f"Binaire TDP : {TDP_BIN} (existe : {os.path.exists(TDP_BIN)})")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nArrêt du serveur.")
