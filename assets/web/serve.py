#!/usr/bin/env python3
"""Tiny static file server that sends the COOP/COEP headers required to run the
GSSquared web build (which is compiled with pthreads / SharedArrayBuffer).

Usage:
    python3 assets/web/serve.py [port] [directory]

Defaults: port 8000, directory = current working directory. Point your browser
at http://localhost:<port>/GSSquared.html
"""
import sys
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


class COOPCOEPRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        # Required for SharedArrayBuffer (pthreads) to be available.
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        # Avoid stale .wasm/.js/.data during development.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    directory = sys.argv[2] if len(sys.argv) > 2 else "."
    handler = partial(COOPCOEPRequestHandler, directory=directory)
    httpd = ThreadingHTTPServer(("0.0.0.0", port), handler)
    print(f"Serving {directory} on http://localhost:{port} (COOP/COEP enabled)")
    print(f"  -> http://localhost:{port}/GSSquared.html")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nshutting down")
        httpd.shutdown()


if __name__ == "__main__":
    main()
