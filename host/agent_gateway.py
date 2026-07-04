#!/usr/bin/env python3
import argparse
import json
import os
import shlex
import shutil
import socket
import subprocess
import tempfile
import threading


DEFAULT_CODEX_CANDIDATES = (
    "/Applications/Codex.app/Contents/Resources/codex",
    "codex",
)


def read_line(conn):
    data = bytearray()
    while True:
        ch = conn.recv(1)
        if not ch:
            raise ConnectionError("client disconnected")
        data += ch
        if ch == b"\n":
            return bytes(data)
        if len(data) > 4096:
            raise ValueError("line too long")


def read_exact(conn, length):
    data = bytearray()
    while len(data) < length:
        chunk = conn.recv(length - len(data))
        if not chunk:
            raise ConnectionError("client disconnected")
        data += chunk
    return bytes(data)


def send_frame(conn, frame_type, frame_id, body):
    if isinstance(body, str):
        body = body.encode("utf-8")
    header = f"AG1 {frame_type} {frame_id} {len(body)}\n".encode("ascii")
    conn.sendall(header + body)


def command_works(command):
    try:
        result = subprocess.run(
            command + ["--version"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5,
        )
        return result.returncode == 0
    except Exception:
        return False


def resolve_codex_command(configured):
    if configured:
        command = shlex.split(configured)
        if command and command_works(command):
            return command
        raise RuntimeError(f"configured codex command does not work: {configured}")

    env_command = os.environ.get("XPAGENT_CODEX_CMD")
    if env_command:
        command = shlex.split(env_command)
        if command and command_works(command):
            return command
        raise RuntimeError(f"XPAGENT_CODEX_CMD does not work: {env_command}")

    for candidate in DEFAULT_CODEX_CANDIDATES:
        path = candidate if os.path.isabs(candidate) else shutil.which(candidate)
        if not path:
            continue
        command = [path]
        if command_works(command):
            return command

    raise RuntimeError("no working Codex CLI found")


class GatewayBackend:
    def __init__(self, args):
        self.kind = args.backend
        self.codex_command = None
        self.codex_cd = args.codex_cd
        self.codex_model = args.codex_model
        self.codex_sandbox = args.codex_sandbox
        self.codex_session_file = args.codex_session_file
        self.codex_new_session = args.codex_new_session
        self.codex_timeout = args.codex_timeout
        self.system_prompt = args.system_prompt
        self.lock = threading.Lock()
        self.codex_thread_id = ""

        if self.kind == "codex":
            self.codex_command = resolve_codex_command(args.codex_command)
            print("codex command:", " ".join(self.codex_command), flush=True)
            self.codex_thread_id = self.load_codex_thread_id()
            if self.codex_thread_id:
                print(f"codex session: {self.codex_thread_id}", flush=True)

    def respond(self, user_text, session):
        if self.kind == "echo":
            return f"echo from host: {user_text}"
        if self.kind == "codex":
            return self.respond_with_codex(user_text, session)
        raise RuntimeError(f"unsupported backend: {self.kind}")

    def respond_with_codex(self, user_text, session):
        with self.lock:
            return self._respond_with_codex_locked(user_text, session)

    def _respond_with_codex_locked(self, user_text, session):
        fd, output_path = tempfile.mkstemp(prefix="xpagent-codex-", suffix=".txt")
        os.close(fd)

        thread_id = self.current_codex_thread_id(session)
        command = list(self.codex_command)
        if thread_id:
            prompt = user_text
            command.extend(
                [
                    "exec",
                    "resume",
                    "--skip-git-repo-check",
                    "--json",
                    "-o",
                    output_path,
                ]
            )
            if self.codex_model:
                command.extend(["--model", self.codex_model])
            command.extend(
                [
                    thread_id,
                    "-",
                ]
            )
        else:
            prompt = (
                f"{self.system_prompt}\n\n"
                "The user is typing from a Windows XP program named xpagent. "
                "Answer as plain text. Keep responses concise unless asked otherwise.\n\n"
                f"User message:\n{user_text}\n"
            )
            command.extend(
                [
                    "exec",
                    "--skip-git-repo-check",
                    "--sandbox",
                    self.codex_sandbox,
                    "--color",
                    "never",
                    "--json",
                    "-C",
                    self.codex_cd,
                    "-o",
                    output_path,
                ]
            )
            if self.codex_model:
                command.extend(["--model", self.codex_model])
            command.append("-")

        try:
            result = subprocess.run(
                command,
                input=prompt,
                text=True,
                capture_output=True,
                timeout=self.codex_timeout,
            )
            thread_id = parse_codex_thread_id(result.stdout)
            if thread_id:
                self.store_codex_thread_id(session, thread_id)

            try:
                with open(output_path, "r", encoding="utf-8", errors="replace") as f:
                    answer = f.read().strip()
            except OSError:
                answer = ""

            if result.returncode != 0:
                detail = result.stderr.strip() or result.stdout.strip()
                raise RuntimeError(f"codex exited {result.returncode}: {detail}")

            return answer or result.stdout.strip() or "(codex returned no text)"
        finally:
            try:
                os.unlink(output_path)
            except OSError:
                pass

    def current_codex_thread_id(self, session):
        if self.codex_session_file:
            return self.codex_thread_id
        return session.get("codex_thread_id", "")

    def load_codex_thread_id(self):
        if not self.codex_session_file:
            return ""
        if self.codex_new_session:
            try:
                os.unlink(self.codex_session_file)
            except OSError:
                pass
            return ""
        try:
            with open(self.codex_session_file, "r", encoding="utf-8") as f:
                return f.read().strip()
        except OSError:
            return ""

    def store_codex_thread_id(self, session, thread_id):
        if not thread_id:
            return

        session["codex_thread_id"] = thread_id
        if not self.codex_session_file:
            return
        if self.codex_thread_id == thread_id:
            return

        self.codex_thread_id = thread_id
        directory = os.path.dirname(self.codex_session_file)
        if directory:
            os.makedirs(directory, exist_ok=True)

        tmp_path = f"{self.codex_session_file}.tmp"
        with open(tmp_path, "w", encoding="utf-8") as f:
            f.write(thread_id + "\n")
        os.replace(tmp_path, self.codex_session_file)
        print(f"codex session: {thread_id}", flush=True)


def parse_codex_thread_id(stdout):
    for line in stdout.splitlines():
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            continue
        if event.get("type") == "thread.started" and event.get("thread_id"):
            return event["thread_id"]
    return ""


def handle_client(conn, peer, backend):
    print(f"agent client connected from {peer[0]}:{peer[1]}", flush=True)
    session = {}
    with conn:
        while True:
            try:
                line = read_line(conn).decode("ascii", "replace").strip()
            except ConnectionError:
                print(f"agent client disconnected from {peer[0]}:{peer[1]}", flush=True)
                return
            parts = line.split()
            if len(parts) != 4 or parts[0] != "AG1":
                send_frame(conn, "ERROR", 0, f"bad frame header: {line}")
                return

            _, frame_type, frame_id_text, length_text = parts
            frame_id = int(frame_id_text)
            length = int(length_text)
            body = read_exact(conn, length).decode("utf-8", "replace")

            print(f"{peer[0]} #{frame_id} {frame_type} {length} bytes", flush=True)

            if frame_type == "BYE":
                send_frame(conn, "BYE", frame_id, "bye")
                return
            if frame_type != "USER":
                send_frame(conn, "ERROR", frame_id, f"unsupported frame: {frame_type}")
                continue

            try:
                reply = backend.respond(body, session)
            except Exception as exc:
                send_frame(conn, "ERROR", frame_id, str(exc))
                continue

            send_frame(conn, "ASSISTANT", frame_id, reply)


def main():
    parser = argparse.ArgumentParser(description="Host-side xpagent gateway.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7790)
    parser.add_argument("--backend", choices=("echo", "codex"), default="echo")
    parser.add_argument("--codex-command", default="")
    parser.add_argument("--codex-model", default="")
    parser.add_argument("--codex-cd", default=os.getcwd())
    parser.add_argument("--codex-sandbox", default="read-only")
    parser.add_argument(
        "--codex-session-file",
        default=os.path.join(os.getcwd(), ".state", "xpagent-codex-thread.txt"),
        help=(
            "Path for the persisted Codex thread id. Use an empty string for "
            "per-connection sessions only."
        ),
    )
    parser.add_argument(
        "--codex-new-session",
        action="store_true",
        help="Start a fresh Codex thread and overwrite --codex-session-file.",
    )
    parser.add_argument("--codex-timeout", type=int, default=120)
    parser.add_argument(
        "--system-prompt",
        default="You are the host-side LLM adapter for Agentic WinXP.",
    )
    args = parser.parse_args()
    backend = GatewayBackend(args)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind((args.host, args.port))
    listener.listen(5)

    print(f"xpagent gateway listening on {args.host}:{args.port}", flush=True)
    print(f"from XP, connect to 10.0.2.2:{args.port}", flush=True)
    while True:
        conn, peer = listener.accept()
        thread = threading.Thread(
            target=handle_client, args=(conn, peer, backend), daemon=True
        )
        thread.start()


if __name__ == "__main__":
    main()
