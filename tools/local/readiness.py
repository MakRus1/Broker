"""Локальные readiness-пробы для Grafana; фиксированный набор низкокардинальных labels."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import urllib.request


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/metrics":
            self.send_error(404)
            return
        lines = ["# TYPE broker_service_ready gauge"]
        for service, port in [("broker", 8082), ("configs", 8092), ("diagnostics", 8085)]:
            ready = 0
            try:
                with urllib.request.urlopen(
                    f"http://{service}:{port}/ping/ready", timeout=1
                ) as response:
                    ready = int(response.status == 200)
            except OSError:
                pass
            lines.append(f'broker_service_ready{{service="{service}"}} {ready}')
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; version=0.0.4")
        self.end_headers()
        self.wfile.write(("\n".join(lines) + "\n").encode())

    def log_message(self, *_):
        pass


ThreadingHTTPServer(("0.0.0.0", 9100), Handler).serve_forever()
