#!/usr/bin/env python3
"""Tiny static file server that sends the COOP/COEP headers required to run the
GSSquared web build (which is compiled with pthreads / SharedArrayBuffer).

Usage:
    python3 assets/web/serve.py [port] [directory]
    python3 assets/web/serve.py [port] [directory] --pack-port N --pack-dir DIR

Defaults: port 8000, directory = current working directory. Point your browser
at http://localhost:<port>/GSSquared.html

--pack-port / --pack-dir start a second server that stands in for arQyv. It
serves .gs2pack files with the CORS headers the cross-origin player needs,
sends a fake X-GS2-Save-Token, and accepts PUT of the rewritten archive.
Create a file named reject-put in the pack directory to make every PUT return
401 (expired save token).
"""
import argparse
import sys
import threading
from functools import partial
from http.server import BaseHTTPRequestHandler, SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PACK_MAX_BYTES = 200 * 1024 * 1024
DEV_SAVE_TOKEN = "dev-save-token"


class COOPCOEPRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # Required for SharedArrayBuffer (pthreads) to be available.
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # Local rebuilds keep the same filenames. Force every cache off so the
        # next reload picks up the files just written.
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


def pack_cors(handler):
    handler.send_header("Access-Control-Allow-Origin", "*")
    handler.send_header("Vary", "Origin")
    handler.send_header("Access-Control-Expose-Headers", "X-GS2-Save-Token, Content-Length")
    handler.send_header("Cache-Control", "no-store")


def safe_pack_path(root: Path, url_path: str):
    rel = url_path.split("?", 1)[0].split("#", 1)[0]
    if rel.startswith("/"):
        rel = rel[1:]
    if not rel:
        return None
    parts = Path(rel).parts
    if ".." in parts or parts[0] == "/":
        return None
    full = (root / rel).resolve()
    try:
        full.relative_to(root.resolve())
    except ValueError:
        return None
    return full


class PackRequestHandler(BaseHTTPRequestHandler):
    pack_dir = Path(".")

    def log_message(self, fmt, *args):
        sys.stderr.write("[pack] " + (fmt % args) + "\n")

    def _send(self, status, body=b"", content_type="application/octet-stream", extra=None):
        data = body if isinstance(body, bytes) else body.encode("utf-8")
        self.send_response(status)
        pack_cors(self)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        if extra:
            for key, value in extra:
                self.send_header(key, value)
        self.end_headers()
        if self.command != "HEAD" and data:
            self.wfile.write(data)

    def do_OPTIONS(self):
        self.send_response(204)
        pack_cors(self)
        self.send_header("Access-Control-Allow-Methods", "GET, PUT, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, X-GS2-Save-Token")
        self.send_header("Access-Control-Max-Age", "600")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        path = safe_pack_path(self.pack_dir, self.path)
        if path is None or not path.is_file() or path.suffix.lower() != ".gs2pack":
            self._send(404, b"not found", "text/plain")
            return
        data = path.read_bytes()
        if len(data) > PACK_MAX_BYTES:
            self._send(413, b"too large", "text/plain")
            return
        self._send(200, data, "application/x-tar", [("X-GS2-Save-Token", DEV_SAVE_TOKEN)])

    def do_PUT(self):
        path = safe_pack_path(self.pack_dir, self.path)
        if path is None or path.suffix.lower() != ".gs2pack":
            self._send(404, b"not found", "text/plain")
            return
        if (self.pack_dir / "reject-put").is_file():
            self._send(401, b"expired", "text/plain")
            return
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length < 0 or length > PACK_MAX_BYTES:
            self._send(413, b"too large", "text/plain")
            return
        body = self.rfile.read(length)
        if len(body) != length:
            self._send(400, b"short body", "text/plain")
            return
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(body)
        self.send_response(204)
        pack_cors(self)
        self.send_header("Content-Length", "0")
        self.end_headers()


def serve_packs(port, directory):
    root = Path(directory).resolve()
    handler = type("BoundPackHandler", (PackRequestHandler,), {"pack_dir": root})
    httpd = ThreadingHTTPServer(("127.0.0.1", port), handler)
    print(f"Pack server {root} on http://127.0.0.1:{port} (CORS, PUT)")
    print(f"  token: {DEV_SAVE_TOKEN}")
    print(f"  401 when {root / 'reject-put'} exists")
    httpd.serve_forever()


def main():
    parser = argparse.ArgumentParser(description="Serve the GSSquared web build with COOP/COEP.")
    parser.add_argument("port", nargs="?", type=int, default=8000)
    parser.add_argument("directory", nargs="?", default=".")
    parser.add_argument("--pack-port", type=int, help="Second port for cross-origin .gs2pack GET/PUT")
    parser.add_argument("--pack-dir", help="Directory of .gs2pack files for --pack-port")
    args = parser.parse_args()
    if (args.pack_port is None) != (args.pack_dir is None):
        parser.error("--pack-port and --pack-dir must be given together")

    pack_thread = None
    if args.pack_port is not None:
        pack_thread = threading.Thread(
            target=serve_packs, args=(args.pack_port, args.pack_dir), daemon=True
        )
        pack_thread.start()

    handler = partial(COOPCOEPRequestHandler, directory=args.directory)
    httpd = ThreadingHTTPServer(("0.0.0.0", args.port), handler)
    print(f"Serving {args.directory} on http://localhost:{args.port} (COOP/COEP enabled)")
    print(f"  -> http://localhost:{args.port}/GSSquared.html")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nshutting down")
        httpd.shutdown()


if __name__ == "__main__":
    main()
