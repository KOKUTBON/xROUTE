"""
Launcher for xROUTE Studio Web UI
Usage: python3 -m xroute.ui [port]
"""

import sys
import webbrowser
from .server import start_server

def main():
    port = 8000
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        port = int(sys.argv[1])
    
    # Optionally open browser
    try:
        webbrowser.open(f"http://localhost:{port}")
    except Exception:
        pass

    start_server(port)

if __name__ == "__main__":
    main()
