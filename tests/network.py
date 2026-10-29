"""Controlled loopback fixtures. Certificates and private keys exist only in TEMP."""
import ctypes
import http.server
import pathlib
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/hold/"):
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.OpenEventW.restype = ctypes.c_void_p
            kernel.OpenEventW.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_wchar_p]
            kernel.SetEvent.argtypes = [ctypes.c_void_p]
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            event = kernel.OpenEventW(2, False, self.path[len("/hold/"):])
            if not event:
                raise OSError(ctypes.get_last_error(), "Cannot open host synchronization event")
            kernel.SetEvent(event)
            kernel.CloseHandle(event)
        if self.path == "/slow" or self.path.startswith("/hold/"):
            time.sleep(2)
        code = 503 if self.path == "/error" else 302 if self.path in ("/redirect", "/downgrade") else 200
        try:
            self.send_response(code)
            self.send_header("Location", "http://127.0.0.1:1/should-not-be-followed")
            self.send_header("Content-Length", "0")
            self.end_headers()
        except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
            pass

    def log_message(self, *_):
        pass


class IPv6(http.server.ThreadingHTTPServer):
    address_family = socket.AF_INET6


def main():
    openssl = shutil.which("openssl") or r"C:\Program Files\Git\usr\bin\openssl.exe"
    if not pathlib.Path(openssl).exists():
        raise RuntimeError("OpenSSL is required to generate the local untrusted TLS fixture")
    with tempfile.TemporaryDirectory(prefix="netwatch-network-") as folder:
        key, cert = pathlib.Path(folder) / "key.pem", pathlib.Path(folder) / "cert.pem"
        subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(key), "-out", str(cert), "-days", "1", "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost"], check=True, capture_output=True)
        servers = [http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler), http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler), IPv6(("::1", 0), Handler)]
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(cert, key)
        servers[1].socket = tls.wrap_socket(servers[1].socket, server_side=True)
        closed = socket.socket()
        closed.bind(("127.0.0.1", 0))
        try:
            for server in servers:
                server.daemon_threads = True
                threading.Thread(target=server.serve_forever, daemon=True).start()
            if len(sys.argv) > 2:
                args = [sys.argv[1], sys.argv[2], sys.argv[3], "--http", str(servers[0].server_port)]
            else:
                args = [sys.argv[1], "network", str(servers[0].server_port), str(closed.getsockname()[1]), str(servers[0].server_port), str(servers[1].server_port), str(servers[2].server_port)]
            subprocess.run(args, check=True, timeout=45)
        finally:
            closed.close()
            for server in servers:
                server.shutdown()
                server.server_close()


if __name__ == "__main__":
    main()
