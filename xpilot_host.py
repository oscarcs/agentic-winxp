#!/usr/bin/env python3
import argparse
import itertools
import socket
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse


DEFAULT_AGENT_HOST = "127.0.0.1"
DEFAULT_AGENT_PORT = 7778
DEFAULT_API_HOST = "127.0.0.1"
DEFAULT_API_PORT = 7780


def read_line(sock):
    data = bytearray()
    while True:
        ch = sock.recv(1)
        if not ch:
            raise ConnectionError("agent disconnected")
        data += ch
        if ch == b"\n":
            return bytes(data)
        if len(data) > 4096:
            raise ValueError("protocol line too long")


def read_exact(sock, length):
    chunks = []
    remaining = length
    while remaining:
        chunk = sock.recv(min(65536, remaining))
        if not chunk:
            raise ConnectionError("agent disconnected")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


class AgentBridge:
    def __init__(self):
        self.conn = None
        self.peer = None
        self.connected_at = None
        self.conn_lock = threading.Lock()
        self.request_lock = threading.Lock()
        self.request_ids = itertools.count(1)

    def set_connection(self, conn, peer):
        with self.conn_lock:
            if self.conn:
                try:
                    self.conn.close()
                except OSError:
                    pass
            self.conn = conn
            self.peer = peer

    def clear_connection(self, conn):
        with self.conn_lock:
            if self.conn is conn:
                self.conn = None
                self.peer = None

    def close_current(self):
        with self.conn_lock:
            conn = self.conn
            self.conn = None
            self.peer = None
        if conn:
            try:
                conn.close()
            except OSError:
                pass

    def connected(self):
        with self.conn_lock:
            return self.conn is not None, self.peer

    def transact(self, verb, *parts, numbers=()):
        with self.conn_lock:
            conn = self.conn
        if conn is None:
            raise RuntimeError("no XP agent connected")

        req_id = next(self.request_ids)
        fields = [verb, str(req_id)]
        fields.extend(str(n) for n in numbers)
        fields.extend(str(len(part)) for part in parts)
        header = " ".join(fields).encode("ascii") + b"\n"

        with self.request_lock:
            try:
                conn.sendall(header)
                for part in parts:
                    conn.sendall(part)

                line = read_line(conn).decode("ascii", "replace").strip()
                fields = line.split()
                if len(fields) != 4:
                    raise RuntimeError(f"bad response header: {line!r}")
                status, got_id, code, length = fields
                if int(got_id) != req_id:
                    raise RuntimeError(f"response id mismatch: {line!r}")
                body = read_exact(conn, int(length))
                return status, int(code), body
            except Exception:
                self.clear_connection(conn)
                try:
                    conn.close()
                except OSError:
                    pass
                raise

    def ping(self):
        return self.transact("PING")

    def quit(self):
        result = self.transact("QUIT")
        self.close_current()
        return result

    def run(self, command, cwd="", timeout_ms=30000, max_output=1024 * 1024):
        command_b = command.encode("utf-8")
        cwd_b = cwd.encode("utf-8")
        if cwd or timeout_ms != 30000 or max_output != 1024 * 1024:
            return self.transact(
                "RUNX", command_b, cwd_b, numbers=(timeout_ms, max_output)
            )
        return self.transact("RUN", command_b)

    def get_file(self, path):
        return self.transact("GET", path.encode("utf-8"))

    def put_file(self, path, data):
        return self.transact("PUT", path.encode("utf-8"), data)

    def append_file(self, path, data):
        return self.transact("APPEND", path.encode("utf-8"), data)

    def cwd(self, path=None):
        if path is None:
            return self.transact("CWD")
        return self.transact("CWD", path.encode("utf-8"))

    def stat(self, path):
        return self.transact("STAT", path.encode("utf-8"))

    def info(self):
        return self.transact("INFO")


class XPilotHandler(BaseHTTPRequestHandler):
    bridge = None
    server_version = "XPilotHost/0.2"

    def do_GET(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)

        if parsed.path == "/status":
            connected, peer = self.bridge.connected()
            body = (
                f"connected: {'yes' if connected else 'no'}\n"
                f"peer: {peer if peer else '-'}\n"
            ).encode("utf-8")
            self.send_bytes(200, body, "text/plain; charset=utf-8")
            return

        if parsed.path == "/info":
            self.forward_text(lambda: self.bridge.info())
            return

        if parsed.path == "/cwd":
            self.forward_text(lambda: self.bridge.cwd())
            return

        if parsed.path == "/file":
            path = params.get("path", [""])[0]
            if not path:
                self.send_bytes(400, b"missing path\n", "text/plain")
                return
            self.forward_binary(lambda: self.bridge.get_file(path))
            return

        if parsed.path == "/stat":
            path = params.get("path", [""])[0]
            if not path:
                self.send_bytes(400, b"missing path\n", "text/plain")
                return
            self.forward_text(lambda: self.bridge.stat(path))
            return

        self.send_bytes(404, b"not found\n", "text/plain")

    def do_POST(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        length = int(self.headers.get("Content-Length", "0"))
        payload = self.rfile.read(length)

        if parsed.path == "/run":
            cwd = params.get("cwd", [""])[0]
            timeout_ms = int(params.get("timeout_ms", ["30000"])[0])
            max_output = int(params.get("max_output", [str(1024 * 1024)])[0])
            command = payload.decode("utf-8", "replace")
            try:
                status, code, body = self.bridge.run(
                    command, cwd=cwd, timeout_ms=timeout_ms, max_output=max_output
                )
            except Exception as exc:
                self.send_bytes(503, f"{exc}\n".encode("utf-8"), "text/plain")
                return
            self.send_response(200 if status == "OK" else 500)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("X-XPilot-Status", status)
            self.send_header("X-XPilot-Exit-Code", str(code))
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if parsed.path == "/file":
            path = params.get("path", [""])[0]
            append = params.get("append", ["0"])[0] in ("1", "true", "yes")
            if not path:
                self.send_bytes(400, b"missing path\n", "text/plain")
                return
            if append:
                self.forward_text(lambda: self.bridge.append_file(path, payload))
            else:
                self.forward_text(lambda: self.bridge.put_file(path, payload))
            return

        if parsed.path == "/cwd":
            path = payload.decode("utf-8", "replace")
            self.forward_text(lambda: self.bridge.cwd(path))
            return

        if parsed.path == "/ping":
            self.forward_text(lambda: self.bridge.ping())
            return

        if parsed.path == "/quit":
            self.forward_text(lambda: self.bridge.quit())
            return

        self.send_bytes(404, b"not found\n", "text/plain")

    def forward_text(self, callback):
        try:
            status, code, body = callback()
        except Exception as exc:
            self.send_bytes(503, f"{exc}\n".encode("utf-8"), "text/plain")
            return
        self.send_response(200 if status == "OK" else 500)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("X-XPilot-Status", status)
        self.send_header("X-XPilot-Code", str(code))
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def forward_binary(self, callback):
        try:
            status, code, body = callback()
        except Exception as exc:
            self.send_bytes(503, f"{exc}\n".encode("utf-8"), "text/plain")
            return
        self.send_response(200 if status == "OK" else 500)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("X-XPilot-Status", status)
        self.send_header("X-XPilot-Code", str(code))
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_bytes(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print("api %s %s" % (self.client_address[0], fmt % args), flush=True)


def accept_agents(bridge, host, port):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((host, port))
    listener.listen(1)
    print(f"agent listener on {host}:{port}")
    print(f"from XP, run: xpilot.exe 10.0.2.2 {port}")

    while True:
        conn, peer = listener.accept()
        conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        bridge.set_connection(conn, peer)
        print(f"agent connected from {peer[0]}:{peer[1]}", flush=True)


def main():
    parser = argparse.ArgumentParser(description="Host bridge for the XP xpilot agent.")
    parser.add_argument("--agent-host", default=DEFAULT_AGENT_HOST)
    parser.add_argument("--agent-port", type=int, default=DEFAULT_AGENT_PORT)
    parser.add_argument("--api-host", default=DEFAULT_API_HOST)
    parser.add_argument("--api-port", type=int, default=DEFAULT_API_PORT)
    args = parser.parse_args()

    bridge = AgentBridge()
    XPilotHandler.bridge = bridge

    thread = threading.Thread(
        target=accept_agents,
        args=(bridge, args.agent_host, args.agent_port),
        daemon=True,
    )
    thread.start()

    api = ThreadingHTTPServer((args.api_host, args.api_port), XPilotHandler)
    print(f"api listening on http://{args.api_host}:{args.api_port}")
    api.serve_forever()


if __name__ == "__main__":
    main()
