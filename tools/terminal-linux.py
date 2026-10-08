#!/usr/bin/env python3
"""Portable Linux PTY bridge used by EduCode's terminal."""

import fcntl
import json
import os
import selectors
import signal
import struct
import sys
import termios


def send(message):
    sys.stdout.write(json.dumps(message, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def main():
    root = sys.argv[1]
    python = sys.argv[2]
    environment = os.environ.copy()
    bin_dir = os.path.dirname(python)
    environment["VIRTUAL_ENV"] = os.path.dirname(bin_dir)
    environment["PATH"] = bin_dir + os.pathsep + environment.get("PATH", "")
    environment.pop("PYTHONHOME", None)
    tools = environment.get("EDUCODE_PYTHON_TOOLS")
    if tools:
        environment["PYTHONPATH"] = tools + (
            os.pathsep + environment["PYTHONPATH"]
            if environment.get("PYTHONPATH")
            else ""
        )
    shell = environment.get("SHELL") or "/bin/bash"
    pid, master = os.forkpty()
    if pid == 0:
        os.chdir(root)
        os.execvpe(shell, [shell, "-i"], environment)

    selector = selectors.DefaultSelector()
    selector.register(master, selectors.EVENT_READ, "pty")
    selector.register(sys.stdin.fileno(), selectors.EVENT_READ, "stdin")
    pending = b""

    def stop(_signum=None, _frame=None):
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

    signal.signal(signal.SIGTERM, stop)
    try:
        while True:
            for key, _mask in selector.select():
                if key.data == "pty":
                    try:
                        data = os.read(master, 65536)
                    except OSError:
                        data = b""
                    if not data:
                        status = os.waitpid(pid, 0)[1]
                        code = os.waitstatus_to_exitcode(status)
                        send({"data": f"\r\nShell завершён · {code}\r\n"})
                        return 0
                    send({"data": data.decode("utf-8", errors="replace")})
                    continue

                chunk = os.read(sys.stdin.fileno(), 65536)
                if not chunk:
                    stop()
                    return 0
                pending += chunk
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    try:
                        message = json.loads(line)
                        if "input" in message:
                            os.write(master, str(message["input"]).encode("utf-8"))
                        if "cols" in message:
                            size = struct.pack(
                                "HHHH",
                                max(2, min(200, int(message.get("rows", 20)))),
                                max(10, min(500, int(message["cols"]))),
                                0,
                                0,
                            )
                            fcntl.ioctl(master, termios.TIOCSWINSZ, size)
                    except Exception as error:
                        send({"error": str(error)})
    finally:
        selector.close()
        os.close(master)


if __name__ == "__main__":
    raise SystemExit(main())
