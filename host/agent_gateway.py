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
import urllib.error
import urllib.parse
import urllib.request


DEFAULT_CODEX_CANDIDATES = (
    "/Applications/Codex.app/Contents/Resources/codex",
    "codex",
)

TOOL_RESPONSE_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["kind", "message", "tool", "args_json"],
    "properties": {
        "kind": {"type": "string", "enum": ["final", "tool"]},
        "message": {"type": "string"},
        "tool": {
            "type": "string",
            "enum": [
                "",
                "xp.status",
                "xp.info",
                "xp.pwd",
                "xp.cd",
                "xp.run",
                "xp.list_dir",
                "xp.stat",
                "xp.read_file",
                "xp.write_file",
                "xp.append_file",
                "xp.mkdir",
                "xp.delete_file",
                "xp.remove_tree",
            ],
        },
        "args_json": {"type": "string"},
    },
}

TOOL_INSTRUCTIONS = r"""\
You may use these XP tools by returning a JSON object with kind="tool".
Return kind="final" with a message when you are done.

Response format:
- Final answer: {"kind":"final","message":"...","tool":"","args_json":"{}"}
- Tool request: {"kind":"tool","message":"","tool":"xp.pwd","args_json":"{}"}

Available tools:
- xp.status: check whether xpilot is connected.
  args: {}
- xp.info: get XP host/process metadata.
  args: {}
- xp.pwd: get xpilot's current XP working directory.
  args: {}
- xp.cd: set xpilot's current XP working directory.
  args: {"path": "C:\\agent"}
- xp.run: run a Windows XP cmd.exe command.
  args: {"command": "dir C:\\agent", "cwd": "C:\\agent", "timeout_ms": 30000, "max_output": 65536}
- xp.list_dir: list an XP directory using dir /a.
  args: {"path": "C:\\agent"}
- xp.stat: stat an XP path through xpilot.
  args: {"path": "C:\\agent\\xpagent.exe"}
- xp.read_file: read a text file from XP.
  args: {"path": "C:\\agent\\README-XP.txt", "encoding": "utf-8", "max_chars": 12000}
- xp.write_file: write a text file on XP.
  args: {"path": "C:\\agent\\note.txt", "text": "...", "encoding": "utf-8"}
- xp.append_file: append text to a file on XP.
  args: {"path": "C:\\agent\\note.txt", "text": "...", "encoding": "utf-8"}
- xp.mkdir: create an XP directory.
  args: {"path": "C:\\agent\\scratch"}
- xp.delete_file: delete one XP file.
  args: {"path": "C:\\agent\\scratch\\note.txt"}
- xp.remove_tree: remove an XP directory tree.
  args: {"path": "C:\\agent\\scratch"}

Use XP tools only for work that needs XP state. Keep final answers concise.
Use Windows XP paths and commands. Do not include Markdown code fences around
the JSON response. args_json must itself be valid JSON.
"""


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


def split_gui_metadata(body):
    metadata = {}
    if not body.startswith("XPAGENT-META "):
        return body, metadata

    header, sep, message = body.partition("\n\n")
    if not sep:
        return body, metadata

    for line in header.splitlines():
        if not line.startswith("XPAGENT-META "):
            continue
        key_value = line[len("XPAGENT-META ") :]
        key, _, value = key_value.partition(":")
        key = key.strip().lower()
        if key:
            metadata[key] = value.strip()
    return message, metadata


def clean_project_field(value):
    value = str(value or "").replace("\t", " ").replace("\r", " ").replace("\n", " ")
    value = " ".join(value.split())
    return value or "Untitled"


def build_project_metadata(codex_cd, codex_session_file):
    workspace = os.path.abspath(codex_cd or os.getcwd())
    project_name = clean_project_field(os.path.basename(workspace) or "workspace")
    rows = [f"PROJECT\t{project_name}\t1"]

    thread_title = "XPAgent session"
    try:
        if codex_session_file and os.path.exists(codex_session_file):
            with open(codex_session_file, "r", encoding="utf-8") as f:
                thread_id = f.read().strip()
            if thread_id:
                thread_title = f"Codex thread {thread_id[:8]}"
    except OSError:
        pass
    rows.append(f"THREAD\t{project_name}\t{clean_project_field(thread_title)}\tnow")

    parent = os.path.dirname(workspace)
    seen = {project_name.lower()}
    try:
        siblings = []
        for name in os.listdir(parent):
            path = os.path.join(parent, name)
            if name.startswith(".") or not os.path.isdir(path):
                continue
            clean_name = clean_project_field(name)
            if clean_name.lower() in seen:
                continue
            siblings.append(clean_name)
        for name in sorted(siblings)[:9]:
            seen.add(name.lower())
            rows.append(f"PROJECT\t{name}\t0")
    except OSError:
        pass

    return "\n".join(rows) + "\n"


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


def q(value):
    return urllib.parse.quote(value, safe="")


def decode_bytes(data, encoding="utf-8"):
    try:
        return data.decode(encoding, "replace")
    except LookupError:
        return data.decode("utf-8", "replace")


def truncate_text(text, limit):
    if limit <= 0 or len(text) <= limit:
        return text, False
    return text[:limit] + "\n[truncated]", True


def quote_xp_arg(value):
    return '"' + str(value).replace('"', '""') + '"'


def parse_structured_response(text):
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        start = text.find("{")
        end = text.rfind("}")
        if start >= 0 and end > start:
            return json.loads(text[start : end + 1])
        raise


class XPilotTools:
    def __init__(self, api, result_limit):
        self.api = api.rstrip("/")
        self.result_limit = result_limit

    def request(self, method, path, data=None):
        req = urllib.request.Request(self.api + path, data=data, method=method)
        try:
            with urllib.request.urlopen(req, timeout=180) as response:
                return response.status, response.headers, response.read()
        except urllib.error.HTTPError as exc:
            return exc.code, exc.headers, exc.read()
        except urllib.error.URLError as exc:
            body = f"{exc}\n".encode("utf-8")
            return 599, {}, body

    def text_result(self, name, status, headers, body, encoding="utf-8"):
        text = decode_bytes(body, encoding)
        text, truncated = truncate_text(text, self.result_limit)
        return {
            "ok": 200 <= status < 300,
            "tool": name,
            "http_status": status,
            "xpilot_status": headers.get("X-XPilot-Status", ""),
            "xpilot_code": headers.get("X-XPilot-Code", ""),
            "text": text,
            "truncated": truncated,
        }

    def run_command(
        self, command, cwd="", timeout_ms=30000, max_output=65536,
        tool_name="xp.run"
    ):
        query = urllib.parse.urlencode(
            {
                "cwd": cwd,
                "timeout_ms": str(timeout_ms),
                "max_output": str(max_output),
            }
        )
        status, headers, body = self.request(
            "POST", f"/run?{query}", command.encode("utf-8")
        )
        result = self.text_result(tool_name, status, headers, body, encoding="cp437")
        result["exit_code"] = int(headers.get("X-XPilot-Exit-Code", "1"))
        result["command"] = command
        if cwd:
            result["cwd"] = cwd
        return result

    def run(self, name, args):
        if not isinstance(args, dict):
            args = {}

        if name == "xp.status":
            status, headers, body = self.request("GET", "/status")
            return self.text_result(name, status, headers, body)

        if name == "xp.info":
            status, headers, body = self.request("GET", "/info")
            return self.text_result(name, status, headers, body)

        if name == "xp.pwd":
            status, headers, body = self.request("GET", "/cwd")
            return self.text_result(name, status, headers, body)

        if name == "xp.cd":
            path = str(args.get("path", ""))
            status, headers, body = self.request(
                "POST", "/cwd", path.encode("utf-8")
            )
            return self.text_result(name, status, headers, body)

        if name == "xp.run":
            command = str(args.get("command", ""))
            if not command:
                return {"ok": False, "tool": name, "error": "missing command"}
            return self.run_command(
                command,
                cwd=str(args.get("cwd", "")),
                timeout_ms=int(args.get("timeout_ms", 30000)),
                max_output=int(args.get("max_output", 65536)),
            )

        if name == "xp.list_dir":
            path = str(args.get("path", "."))
            return self.run_command(f"dir /a {quote_xp_arg(path)}", tool_name=name)

        if name == "xp.stat":
            path = str(args.get("path", ""))
            if not path:
                return {"ok": False, "tool": name, "error": "missing path"}
            status, headers, body = self.request("GET", f"/stat?path={q(path)}")
            return self.text_result(name, status, headers, body)

        if name == "xp.read_file":
            path = str(args.get("path", ""))
            if not path:
                return {"ok": False, "tool": name, "error": "missing path"}
            encoding = str(args.get("encoding", "utf-8"))
            max_chars = int(args.get("max_chars", self.result_limit))
            status, headers, body = self.request("GET", f"/file?path={q(path)}")
            text = decode_bytes(body, encoding)
            text, truncated = truncate_text(text, min(max_chars, self.result_limit))
            return {
                "ok": 200 <= status < 300,
                "tool": name,
                "http_status": status,
                "xpilot_status": headers.get("X-XPilot-Status", ""),
                "xpilot_code": headers.get("X-XPilot-Code", ""),
                "path": path,
                "text": text,
                "truncated": truncated,
            }

        if name in ("xp.write_file", "xp.append_file"):
            path = str(args.get("path", ""))
            text = str(args.get("text", ""))
            encoding = str(args.get("encoding", "utf-8"))
            if not path:
                return {"ok": False, "tool": name, "error": "missing path"}
            query = f"/file?path={q(path)}"
            if name == "xp.append_file":
                query += "&append=1"
            try:
                data = text.encode(encoding)
            except LookupError:
                data = text.encode("utf-8")
            status, headers, body = self.request("POST", query, data)
            result = self.text_result(name, status, headers, body)
            result["path"] = path
            result["bytes"] = len(data)
            return result

        if name == "xp.mkdir":
            path = str(args.get("path", ""))
            if not path:
                return {"ok": False, "tool": name, "error": "missing path"}
            return self.run_command(f"mkdir {quote_xp_arg(path)}", tool_name=name)

        if name == "xp.delete_file":
            path = str(args.get("path", ""))
            if not path:
                return {"ok": False, "tool": name, "error": "missing path"}
            return self.run_command(f"del /f /q {quote_xp_arg(path)}", tool_name=name)

        if name == "xp.remove_tree":
            path = str(args.get("path", ""))
            if not path:
                return {"ok": False, "tool": name, "error": "missing path"}
            return self.run_command(f"rmdir /s /q {quote_xp_arg(path)}", tool_name=name)

        return {"ok": False, "tool": name, "error": "unknown tool"}


class GatewayBackend:
    def __init__(self, args):
        self.kind = args.backend
        self.codex_command = None
        self.codex_cd = args.codex_cd
        self.codex_model = args.codex_model
        self.codex_sandbox = args.codex_sandbox
        self.codex_session_file = args.codex_session_file
        self.codex_new_session = args.codex_new_session
        self.codex_ignore_user_config = not args.codex_use_user_config
        self.codex_timeout = args.codex_timeout
        self.system_prompt = args.system_prompt
        self.xp_tools_enabled = not args.disable_xp_tools
        self.xp_tool_max_steps = args.xp_tool_max_steps
        self.xp_tools = XPilotTools(args.xpilot_api, args.xp_tool_result_limit)
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

    def project_metadata(self, session):
        return build_project_metadata(self.codex_cd, self.codex_session_file)

    def respond_with_codex(self, user_text, session):
        with self.lock:
            return self._respond_with_codex_locked(user_text, session)

    def _respond_with_codex_locked(self, user_text, session):
        if not self.xp_tools_enabled:
            prompt = (
                f"{self.system_prompt}\n\n"
                "The user is typing from a Windows XP program named xpagent. "
                "Answer as plain text. Keep responses concise unless asked otherwise.\n\n"
                f"User message:\n{user_text}\n"
            )
            return self.run_codex_once(prompt, session)

        prompt = self.build_tool_initial_prompt(user_text)
        for step in range(self.xp_tool_max_steps + 1):
            answer = self.run_codex_once(prompt, session, structured=True)
            try:
                response = parse_structured_response(answer)
            except (TypeError, json.JSONDecodeError) as exc:
                return f"Could not parse Codex tool response: {exc}\n\n{answer}"

            kind = response.get("kind")
            if kind == "final":
                return response.get("message", "").strip() or "(empty response)"

            if kind != "tool":
                return f"Unsupported Codex response kind: {kind!r}"
            if step >= self.xp_tool_max_steps:
                return "Stopped: tool step limit reached before a final answer."

            tool_name = response.get("tool", "")
            tool_args = self.parse_tool_args(response)
            print(
                f"codex tool step {step + 1}: {tool_name} {json.dumps(tool_args)}",
                flush=True,
            )
            tool_result = self.xp_tools.run(tool_name, tool_args)
            prompt = self.build_tool_result_prompt(tool_name, tool_args, tool_result)

        return "Stopped: tool loop ended before a final answer."

    def build_tool_initial_prompt(self, user_text):
        return (
            f"{self.system_prompt}\n\n"
            "The user is typing from a Windows XP program named xpagent. "
            "You are allowed to operate the XP VM through host-mediated tools. "
            "Do not ask the user to run commands for you when a provided XP tool "
            "can do the work.\n\n"
            f"{TOOL_INSTRUCTIONS}\n"
            "Respond with exactly one JSON object matching the schema.\n\n"
            f"User message:\n{user_text}\n"
        )

    def build_tool_result_prompt(self, tool_name, tool_args, tool_result):
        return (
            "Tool result for your previous XP tool request:\n"
            f"{json.dumps({'tool': tool_name, 'args': tool_args, 'result': tool_result}, ensure_ascii=True)}\n\n"
            "Continue the same task. Respond with exactly one JSON object matching "
            "the schema: either request another tool or return a final message."
        )

    def parse_tool_args(self, response):
        if "args" in response and isinstance(response["args"], dict):
            return response["args"]
        args_json = response.get("args_json", "{}")
        try:
            args = json.loads(args_json or "{}")
        except json.JSONDecodeError:
            return {"_parse_error": f"invalid args_json: {args_json}"}
        if isinstance(args, dict):
            return args
        return {"_parse_error": f"args_json must decode to an object: {args_json}"}

    def run_codex_once(self, prompt, session, structured=False):
        fd, output_path = tempfile.mkstemp(prefix="xpagent-codex-", suffix=".txt")
        os.close(fd)
        schema_path = ""
        if structured:
            fd, schema_path = tempfile.mkstemp(
                prefix="xpagent-schema-", suffix=".json"
            )
            os.close(fd)
            with open(schema_path, "w", encoding="utf-8") as f:
                json.dump(TOOL_RESPONSE_SCHEMA, f)

        thread_id = self.current_codex_thread_id(session)
        command = list(self.codex_command)
        if thread_id:
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
            if self.codex_ignore_user_config:
                command.append("--ignore-user-config")
            if structured:
                command.extend(["--output-schema", schema_path])
            if self.codex_model:
                command.extend(["--model", self.codex_model])
            command.extend(
                [
                    thread_id,
                    "-",
                ]
            )
        else:
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
            if self.codex_ignore_user_config:
                command.append("--ignore-user-config")
            if structured:
                command.extend(["--output-schema", schema_path])
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
            if schema_path:
                try:
                    os.unlink(schema_path)
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
            body, gui_metadata = split_gui_metadata(body)
            if gui_metadata:
                session["gui_metadata"] = gui_metadata

            print(f"{peer[0]} #{frame_id} {frame_type} {length} bytes", flush=True)

            if frame_type == "BYE":
                send_frame(conn, "BYE", frame_id, "bye")
                return
            if frame_type == "PROJECTS":
                send_frame(conn, "PROJECTS", frame_id, backend.project_metadata(session))
                continue
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
    parser.add_argument(
        "--codex-use-user-config",
        action="store_true",
        help="Load normal Codex user config/plugins instead of the minimal bridge config.",
    )
    parser.add_argument("--codex-timeout", type=int, default=120)
    parser.add_argument(
        "--system-prompt",
        default="You are the host-side LLM adapter for Agentic WinXP.",
    )
    parser.add_argument("--xpilot-api", default="http://127.0.0.1:7780")
    parser.add_argument(
        "--disable-xp-tools",
        action="store_true",
        help="Disable Codex JSON tool loop and return plain Codex text.",
    )
    parser.add_argument("--xp-tool-max-steps", type=int, default=8)
    parser.add_argument("--xp-tool-result-limit", type=int, default=12000)
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
