#!/usr/bin/env python3
import argparse
import os
import shlex
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request


DEFAULT_API = "http://127.0.0.1:7780"
DEFAULT_TIMEOUT_MS = 30000
DEFAULT_MAX_OUTPUT = 1024 * 1024


def request(api, method, path, data=None):
    req = urllib.request.Request(api + path, data=data, method=method)
    try:
        with urllib.request.urlopen(req) as response:
            return response.status, response.headers, response.read()
    except urllib.error.HTTPError as exc:
        return exc.code, exc.headers, exc.read()
    except urllib.error.URLError as exc:
        return 599, {}, f"{exc}\n".encode("utf-8")


def q(value):
    return urllib.parse.quote(value, safe="")


def api_status(api):
    return request(api, "GET", "/status")


def api_text_get(api, path):
    return request(api, "GET", path)


def api_text_post(api, path, text):
    return request(api, "POST", path, text.encode("utf-8"))


def api_run(api, command, cwd="", timeout_ms=DEFAULT_TIMEOUT_MS,
            max_output=DEFAULT_MAX_OUTPUT):
    query = urllib.parse.urlencode(
        {
            "cwd": cwd,
            "timeout_ms": str(timeout_ms),
            "max_output": str(max_output),
        }
    )
    return request(api, "POST", f"/run?{query}", command.encode("utf-8"))


def api_get(api, xp_path):
    return request(api, "GET", f"/file?path={q(xp_path)}")


def api_put(api, xp_path, data, append=False):
    query = f"path={q(xp_path)}"
    if append:
        query += "&append=1"
    return request(api, "POST", f"/file?{query}", data)


def print_body(body):
    sys.stdout.buffer.write(body)
    if body and not body.endswith(b"\n"):
        sys.stdout.write("\n")


def run_or_print(api, command, cwd="", timeout_ms=DEFAULT_TIMEOUT_MS,
                 max_output=DEFAULT_MAX_OUTPUT):
    status, headers, body = api_run(api, command, cwd, timeout_ms, max_output)
    sys.stdout.buffer.write(body)
    exit_code = int(headers.get("X-XPilot-Exit-Code", "1"))
    return exit_code if status == 200 else 1


def get_cwd(api):
    status, _, body = api_text_get(api, "/cwd")
    if status != 200:
        return ""
    return body.decode("utf-8", "replace").strip()


def set_cwd(api, path):
    status, _, body = api_text_post(api, "/cwd", path)
    print_body(body)
    return status == 200


def shell(api):
    print("XPilot shell. Builtins: cd, pwd, ls, cat, get, put, edit, ping, info, exit")
    print("Use normal commands for everything else; they run through cmd.exe /C.")
    while True:
        cwd = get_cwd(api) or "?"
        try:
            line = input(f"xp {cwd}> ")
        except EOFError:
            print()
            return 0
        except KeyboardInterrupt:
            print()
            continue

        line = line.strip()
        if not line:
            continue
        try:
            parts = shlex.split(line, posix=False)
        except ValueError as exc:
            print(exc, file=sys.stderr)
            continue
        if not parts:
            continue

        cmd = parts[0].lower()
        if cmd in ("exit", "quit"):
            return 0
        if cmd == "pwd":
            print(cwd)
            continue
        if cmd == "cd":
            set_cwd(api, parts[1] if len(parts) > 1 else "")
            continue
        if cmd == "ls":
            target = parts[1] if len(parts) > 1 else "."
            run_or_print(api, f'dir /a "{target}"')
            continue
        if cmd in ("cat", "type"):
            if len(parts) < 2:
                print("usage: cat <xp-path>", file=sys.stderr)
                continue
            status, _, body = api_get(api, parts[1])
            if status == 200:
                sys.stdout.buffer.write(body)
                if body and not body.endswith(b"\n"):
                    print()
            else:
                sys.stderr.buffer.write(body)
            continue
        if cmd == "get":
            if len(parts) < 2:
                print("usage: get <xp-path> [local-path]", file=sys.stderr)
                continue
            local = parts[2] if len(parts) > 2 else os.path.basename(parts[1])
            status, _, body = api_get(api, parts[1])
            if status != 200:
                sys.stderr.buffer.write(body)
                continue
            with open(local, "wb") as f:
                f.write(body)
            print(f"wrote {local}")
            continue
        if cmd == "put":
            if len(parts) < 3:
                print("usage: put <local-path> <xp-path>", file=sys.stderr)
                continue
            with open(parts[1], "rb") as f:
                data = f.read()
            status, _, body = api_put(api, parts[2], data)
            print_body(body)
            continue
        if cmd == "edit":
            if len(parts) < 2:
                print("usage: edit <xp-path>", file=sys.stderr)
                continue
            edit_file(api, parts[1])
            continue
        if cmd == "ping":
            status, _, body = request(api, "POST", "/ping", b"")
            print_body(body)
            continue
        if cmd == "info":
            status, _, body = api_text_get(api, "/info")
            print_body(body)
            continue

        run_or_print(api, line)


def edit_file(api, xp_path):
    status, _, body = api_get(api, xp_path)
    if status != 200:
        sys.stderr.buffer.write(body)
        return 1

    suffix = os.path.splitext(xp_path.replace("\\", "/"))[1] or ".txt"
    fd, local_path = tempfile.mkstemp(prefix="xpilot-edit-", suffix=suffix)
    os.close(fd)
    with open(local_path, "wb") as f:
        f.write(body)

    editor = os.environ.get("EDITOR", "vi")
    before = os.path.getmtime(local_path)
    subprocess.call([editor, local_path])
    after = os.path.getmtime(local_path)
    if after == before:
        print("unchanged")
        return 0

    with open(local_path, "rb") as f:
        data = f.read()
    status, _, put_body = api_put(api, xp_path, data)
    print_body(put_body)
    return 0 if status == 200 else 1


def xp_join(base, rel):
    rel = rel.replace("/", "\\")
    if base.endswith("\\"):
        return base + rel
    return base + "\\" + rel


def put_dir(api, local_dir, xp_dir):
    count = 0

    for root, dirs, files in os.walk(local_dir):
        rel_root = os.path.relpath(root, local_dir)
        xp_root = xp_dir if rel_root == "." else xp_join(xp_dir, rel_root)
        run_or_print(api, f'if not exist "{xp_root}" mkdir "{xp_root}"')
        for name in files:
            local_path = os.path.join(root, name)
            xp_path = xp_join(xp_root, name)
            with open(local_path, "rb") as f:
                data = f.read()
            status, _, body = api_put(api, xp_path, data)
            if status != 200:
                sys.stderr.buffer.write(body)
                return 1
            count += 1
            print(f"put {local_path} -> {xp_path}")

    print(f"putdir complete: {count} files")
    return 0


def get_dir(api, xp_dir, local_dir):
    status, _, body = api_run(api, f'dir /a-d /s /b "{xp_dir}"')
    if status != 200:
        sys.stderr.buffer.write(body)
        return 1

    lines = body.decode("mbcs" if sys.platform == "win32" else "latin-1",
                        "replace").splitlines()
    base = xp_dir.rstrip("\\/")
    count = 0
    os.makedirs(local_dir, exist_ok=True)

    for xp_path in lines:
        xp_path = xp_path.strip()
        if not xp_path:
            continue
        rel = xp_path
        if xp_path.lower().startswith(base.lower() + "\\"):
            rel = xp_path[len(base) + 1:]
        local_path = os.path.join(local_dir, rel.replace("\\", os.sep))
        os.makedirs(os.path.dirname(local_path), exist_ok=True)
        status, _, data = api_get(api, xp_path)
        if status != 200:
            sys.stderr.buffer.write(data)
            return 1
        with open(local_path, "wb") as f:
            f.write(data)
        count += 1
        print(f"got {xp_path} -> {local_path}")

    print(f"getdir complete: {count} files")
    return 0


def main():
    parser = argparse.ArgumentParser(description="CLI for the XP xpilot bridge.")
    parser.add_argument("--api", default=DEFAULT_API)
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status")
    sub.add_parser("ping")
    sub.add_parser("info")
    sub.add_parser("pwd")

    cd = sub.add_parser("cd")
    cd.add_argument("xp_path", nargs="?")

    run = sub.add_parser("run")
    run.add_argument("--cwd", default="")
    run.add_argument("-t", "--timeout", type=float, default=30.0,
                     help="timeout in seconds")
    run.add_argument("-m", "--max-output", type=int, default=DEFAULT_MAX_OUTPUT)
    run.add_argument("command", nargs=argparse.REMAINDER)

    ls = sub.add_parser("ls")
    ls.add_argument("xp_path", nargs="?", default=".")

    cat = sub.add_parser("cat")
    cat.add_argument("xp_path")

    stat = sub.add_parser("stat")
    stat.add_argument("xp_path")

    get = sub.add_parser("get")
    get.add_argument("xp_path")
    get.add_argument("local_path", nargs="?")

    put = sub.add_parser("put")
    put.add_argument("local_path")
    put.add_argument("xp_path")

    putdir = sub.add_parser("putdir")
    putdir.add_argument("local_dir")
    putdir.add_argument("xp_dir")

    getdir = sub.add_parser("getdir")
    getdir.add_argument("xp_dir")
    getdir.add_argument("local_dir")

    write = sub.add_parser("write")
    write.add_argument("xp_path")

    append = sub.add_parser("append")
    append.add_argument("xp_path")
    append.add_argument("text", nargs=argparse.REMAINDER)

    edit = sub.add_parser("edit")
    edit.add_argument("xp_path")

    mkdir = sub.add_parser("mkdir")
    mkdir.add_argument("xp_path")

    rm = sub.add_parser("rm")
    rm.add_argument("xp_path")

    rmdir = sub.add_parser("rmdir")
    rmdir.add_argument("xp_path")

    sub.add_parser("shell")
    sub.add_parser("quit")

    args = parser.parse_args()
    api = args.api

    if args.cmd == "status":
        status, _, body = api_status(api)
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "ping":
        status, _, body = request(api, "POST", "/ping", b"")
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "info":
        status, _, body = api_text_get(api, "/info")
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "pwd":
        status, _, body = api_text_get(api, "/cwd")
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "cd":
        status, _, body = api_text_post(api, "/cwd", args.xp_path or "")
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "run":
        command = " ".join(args.command)
        if not command:
            print("missing command", file=sys.stderr)
            return 2
        return run_or_print(
            api,
            command,
            cwd=args.cwd,
            timeout_ms=int(args.timeout * 1000),
            max_output=args.max_output,
        )

    if args.cmd == "ls":
        return run_or_print(api, f'dir /a "{args.xp_path}"')

    if args.cmd == "cat":
        status, _, body = api_get(api, args.xp_path)
        if status != 200:
            sys.stderr.buffer.write(body)
            return 1
        sys.stdout.buffer.write(body)
        return 0

    if args.cmd == "stat":
        status, _, body = api_text_get(api, f"/stat?path={q(args.xp_path)}")
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "get":
        status, _, body = api_get(api, args.xp_path)
        if status != 200:
            sys.stderr.buffer.write(body)
            return 1
        if args.local_path:
            with open(args.local_path, "wb") as f:
                f.write(body)
        else:
            sys.stdout.buffer.write(body)
        return 0

    if args.cmd == "put":
        with open(args.local_path, "rb") as f:
            data = f.read()
        status, _, body = api_put(api, args.xp_path, data)
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "putdir":
        return put_dir(api, args.local_dir, args.xp_dir)

    if args.cmd == "getdir":
        return get_dir(api, args.xp_dir, args.local_dir)

    if args.cmd == "write":
        data = sys.stdin.buffer.read()
        status, _, body = api_put(api, args.xp_path, data)
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "append":
        text = " ".join(args.text)
        if not text:
            text = sys.stdin.read()
        status, _, body = api_put(api, args.xp_path, text.encode("utf-8"), append=True)
        print_body(body)
        return 0 if status == 200 else 1

    if args.cmd == "edit":
        return edit_file(api, args.xp_path)

    if args.cmd == "mkdir":
        return run_or_print(api, f'mkdir "{args.xp_path}"')

    if args.cmd == "rm":
        return run_or_print(api, f'del /f /q "{args.xp_path}"')

    if args.cmd == "rmdir":
        return run_or_print(api, f'rmdir /s /q "{args.xp_path}"')

    if args.cmd == "shell":
        return shell(api)

    if args.cmd == "quit":
        status, _, body = request(api, "POST", "/quit", b"")
        print_body(body)
        return 0 if status == 200 else 1

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
