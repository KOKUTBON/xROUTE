#!/usr/bin/env python3
"""Build the static Studio demo without importing or compiling the native library."""
import ast
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build" / "demo"


def build():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    web = ROOT / "python" / "xroute" / "web"
    # Reuse the backend's sample without importing xroute (which needs the C library).
    module = ast.parse((web.parent / "server.py").read_text())
    sample = next(ast.literal_eval(node.value) for node in module.body
                  if isinstance(node, ast.Assign)
                  and any(isinstance(t, ast.Name) and t.id == "DEFAULT_WAREHOUSE_XML"
                          for t in node.targets))
    (OUTPUT / "sample-map.js").write_text(
        "window.XROUTE_DEMO_SAMPLE = " + json.dumps(sample) + ";\n")
    html = (web / "index.html").read_text()
    html = html.replace('  <title>',
                        '  <link rel="icon" href="data:image/svg+xml,%3Csvg xmlns=\'http://www.w3.org/2000/svg\' viewBox=\'0 0 32 32\'%3E%3Crect width=\'32\' height=\'32\' rx=\'7\' fill=\'%234f46e5\'/%3E%3Cpath d=\'M8 24V8h16v16\' fill=\'none\' stroke=\'white\' stroke-width=\'4\'/%3E%3C/svg%3E">\n'
                        '  <title>', 1)
    html = html.replace("  <!-- Client-side Application Script -->",
                        '  <script src="./sample-map.js"></script>\n'
                        '  <script src="./demo-api.js"></script>\n'
                        '  <!-- Client-side Application Script -->')
    html = html.replace("<title>xROUTE Studio -", "<title>xROUTE Demo -")
    html = html.replace('>v2.0</span>', '>Browser demo</span>')
    html = html.replace('  <!-- Main Work Area -->',
                        '  <div class="bg-indigo-950 text-indigo-200 text-xs px-6 py-2 border-b border-indigo-800" role="note">'
                        'Interactive demo · APIs are mocked in your browser. '
                        'Maps stay on this device; routing uses a JavaScript simulation of the C engine.</div>\n'
                        '  <!-- Main Work Area -->')
    (OUTPUT / "index.html").write_text(html)
    shutil.copyfile(ROOT / "demo" / "demo-api.js", OUTPUT / "demo-api.js")
    (OUTPUT / ".nojekyll").touch()
    print(f"Static demo ready: {OUTPUT}")


if __name__ == "__main__":
    build()
