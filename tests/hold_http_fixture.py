"""Loopback-only server that records requests and holds their response headers."""
import argparse
import http.server
import json
from pathlib import Path
import threading
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", type=Path, required=True)
    args = parser.parse_args()
    stop = threading.Event()
    lock = threading.Lock()
    state = {"port": 0, "requests": 0, "completed": 0, "last_started_unix": None}

    def publish():
        args.state.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.state.with_suffix(".tmp")
        temporary.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
        temporary.replace(args.state)

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/shutdown":
                self.send_response(200)
                self.end_headers()
                stop.set()
                return
            if self.path != "/hold":
                self.send_error(404)
                return
            with lock:
                state["requests"] += 1
                state["last_started_unix"] = time.time()
                publish()
            stop.wait(60)
            try:
                self.send_response(200)
                self.send_header("Content-Length", "0")
                self.end_headers()
            except (BrokenPipeError, ConnectionResetError):
                pass
            finally:
                with lock:
                    state["completed"] += 1
                    publish()

        def log_message(self, *_):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    state["port"] = server.server_port
    publish()
    threading.Thread(target=server.serve_forever, daemon=True).start()
    print(json.dumps(state), flush=True)
    stop.wait(180)
    server.shutdown()
    server.server_close()


if __name__ == "__main__":
    main()
