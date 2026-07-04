#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from datetime import datetime
import argparse


class GatewayHandler(BaseHTTPRequestHandler):
    server_version = "WinXPGateway/0.1"

    def do_GET(self):
        if self.path in ("/", "/health"):
            body = (
                "winxp host gateway ok\r\n"
                f"time: {datetime.now().isoformat(timespec='seconds')}\r\n"
                "guest target: http://10.0.2.2:7777/health\r\n"
            ).encode("ascii")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=us-ascii")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        self.send_error(404, "Not found")

    def do_POST(self):
        if self.path != "/agent":
            self.send_error(404, "Not found")
            return

        length = int(self.headers.get("Content-Length", "0"))
        payload = self.rfile.read(length)
        body = b"received:\r\n" + payload + b"\r\n"

        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=us-ascii")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print("%s %s" % (self.address_string(), fmt % args), flush=True)


def main():
    parser = argparse.ArgumentParser(description="Tiny host gateway for the XP VM.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7777)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), GatewayHandler)
    print(f"gateway listening on http://{args.host}:{args.port}")
    print("from XP, try: http://10.0.2.2:7777/health")
    server.serve_forever()


if __name__ == "__main__":
    main()
