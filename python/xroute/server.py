"""
xROUTE Studio - Automated Web Server & REST API
Zero-dependency HTTP server linking xROUTE C99 engine to interactive Web UI.
"""

import http.server
import json
import os
import sys
import math
import urllib.parse
from typing import Dict, Any

# Ensure xroute is importable
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
import xroute
from xroute.ingest import import_map, integer, MAX_BYTES

WEB_DIR = os.path.join(os.path.dirname(__file__), "web")

# Sample default warehouse XML if user hasn't loaded a map yet
DEFAULT_WAREHOUSE_XML = """<?xml version="1.0" encoding="UTF-8"?>
<Magazzino numRighe="12" numColonne="16" dimColonne="350" dimRighe="200">
\t<!-- Storage Racks Corsia 1 -->
\t<Cella x="2" y="2" Tipo="1" Corsia="1" Campata="1"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="3" y="2" Tipo="1" Corsia="1" Campata="2"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="4" y="2" Tipo="1" Corsia="1" Campata="3"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="5" y="2" Tipo="1" Corsia="1" Campata="4"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="6" y="2" Tipo="1" Corsia="1" Campata="5"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="7" y="2" Tipo="1" Corsia="1" Campata="6"><LatoAccessibile>up</LatoAccessibile></Cella>

\t<Cella x="2" y="3" Tipo="1" Corsia="1" Campata="7"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="3" y="3" Tipo="1" Corsia="1" Campata="8"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="4" y="3" Tipo="1" Corsia="1" Campata="9"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="5" y="3" Tipo="1" Corsia="1" Campata="10"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="6" y="3" Tipo="1" Corsia="1" Campata="11"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="7" y="3" Tipo="1" Corsia="1" Campata="12"><LatoAccessibile>dw</LatoAccessibile></Cella>

\t<!-- Storage Racks Corsia 2 -->
\t<Cella x="2" y="6" Tipo="1" Corsia="2" Campata="1"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="3" y="6" Tipo="1" Corsia="2" Campata="2"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="4" y="6" Tipo="1" Corsia="2" Campata="3"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="5" y="6" Tipo="1" Corsia="2" Campata="4"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="6" y="6" Tipo="1" Corsia="2" Campata="5"><LatoAccessibile>up</LatoAccessibile></Cella>
\t<Cella x="7" y="6" Tipo="1" Corsia="2" Campata="6"><LatoAccessibile>up</LatoAccessibile></Cella>

\t<Cella x="2" y="7" Tipo="1" Corsia="2" Campata="7"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="3" y="7" Tipo="1" Corsia="2" Campata="8"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="4" y="7" Tipo="1" Corsia="2" Campata="9"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="5" y="7" Tipo="1" Corsia="2" Campata="10"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="6" y="7" Tipo="1" Corsia="2" Campata="11"><LatoAccessibile>dw</LatoAccessibile></Cella>
\t<Cella x="7" y="7" Tipo="1" Corsia="2" Campata="12"><LatoAccessibile>dw</LatoAccessibile></Cella>

\t<!-- Traversable Racks & Direction Constraints -->
\t<Cella x="10" y="4" Tipo="2" Corsia="3" Campata="1">
\t\t<LatoAccessibile>up</LatoAccessibile><LatoAccessibile>dw</LatoAccessibile>
\t\t<SpostamentoPermesso>dx</SpostamentoPermesso><SpostamentoPermesso>dw</SpostamentoPermesso>
\t</Cella>

\t<!-- Obstacles -->
\t<Cella x="0" y="5" Tipo="4"/>
\t<Cella x="1" y="5" Tipo="4"/>
\t<Cella x="14" y="5" Tipo="4"/>
\t<Cella x="15" y="5" Tipo="4"/>
</Magazzino>"""


class StudioRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path in ["", "/", "/index.html"]:
            index_path = os.path.join(WEB_DIR, "index.html")
            if os.path.exists(index_path):
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                with open(index_path, "rb") as f:
                    self.wfile.write(f.read())
            else:
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                self.wfile.write(b"<h1>xROUTE Studio Web UI</h1><p>index.html not found</p>")
            return
        elif parsed.path == "/api/default_map":
            self.send_json_response({"status": "ok", "xml": DEFAULT_WAREHOUSE_XML})
            return
        else:
            self.send_error(404, "Not Found")

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        try:
            content_len = int(self.headers.get("Content-Length", "0"))
            if content_len <= 0 or content_len > MAX_BYTES * 2:
                self.send_json_response({"status": "error", "message": "Request must be 1 byte–8 MiB"}, 413)
                return
            req = json.loads(self.rfile.read(content_len).decode("utf-8"))
            if not isinstance(req, dict):
                raise ValueError("Request must be a JSON object")
        except (ValueError, UnicodeError, RecursionError) as exc:
            self.send_json_response({"status": "error", "message": str(exc)}, 400)
            return

        if parsed.path == "/api/parse":
            self.handle_parse(req)
        elif parsed.path == "/api/route":
            self.handle_route(req)
        elif parsed.path == "/api/fleet":
            self.handle_fleet(req)
        elif parsed.path == "/api/convert":
            self.handle_convert(req)
        else:
            self.send_error(404, "Endpoint not found")

    def send_json_response(self, data: Dict[str, Any], status: int = 200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode("utf-8"))

    def handle_parse(self, req: Dict[str, Any]):
        try:
            imported = import_map(req.get("content", ""), req.get("format", "auto"),
                                  req.get("cell_width_cm", 350), req.get("cell_height_cm", 200))
            with xroute.Warehouse.from_import(imported) as wh:
                map_dict = wh.to_dict()
            self.send_json_response({"status": "ok", "format": imported.format,
                                     "normalized_content": json.dumps(imported.data),
                                     "normalized_format": "json", "warnings": imported.warnings,
                                     "map": map_dict})
        except (ValueError, TypeError, RuntimeError, MemoryError) as exc:
            self.send_json_response({"status": "error", "message": str(exc)}, 400)

    @staticmethod
    def load_request_map(req):
        return xroute.Warehouse.load_string(
            req.get("map_content", DEFAULT_WAREHOUSE_XML), format=req.get("map_format", "auto"),
            cell_width_cm=req.get("cell_width_cm", 350), cell_height_cm=req.get("cell_height_cm", 200))

    @staticmethod
    def point(value, wh):
        if not isinstance(value, (list, tuple)) or len(value) != 2:
            raise ValueError("Position must be [x, y]")
        return (integer(value[0], "x", 0, wh.cols - 1), integer(value[1], "y", 0, wh.rows - 1))

    def prepare_route(self, req, wh):
        penalty = integer(req.get("turn_penalty", 0), "turn_penalty", 0, 10000)
        speed = req.get("speed", 1.5)
        if isinstance(speed, bool) or not isinstance(speed, (int, float)) or not math.isfinite(speed) or speed <= 0:
            raise ValueError("speed must be a finite positive number")
        if wh.rows * wh.cols * 5 * (max(wh.cell_width_cm, wh.cell_height_cm) + 2 * penalty) >= (2**31 - 1) // 4:
            raise ValueError("Map scale and turn penalty exceed native routing cost capacity")
        blocked = req.get("blocked_cells", [])
        if not isinstance(blocked, list):
            raise ValueError("blocked_cells must be a list of [x, y] positions")
        for position in blocked:
            wh.set_dynamic_obstacle(*self.point(position, wh))
        return penalty, speed

    def handle_route(self, req: Dict[str, Any]):
        start = req.get("start")
        goal = req.get("goal")
        corsia = req.get("corsia")
        campata = req.get("campata")

        if not start:
            self.send_json_response({"status": "error", "message": "Missing start position"}, status=400)
            return

        wh = None
        try:
            wh = self.load_request_map(req)
            turn_penalty, speed = self.prepare_route(req, wh)
            start = self.point(start, wh)
            if goal is not None:
                goal = self.point(goal, wh)
            if corsia is not None:
                corsia = integer(corsia, "corsia", 1)
            if campata is not None:
                campata = integer(campata, "campata", 1)

            if corsia is not None and campata is not None and corsia > 0:
                route = wh.route_to_bay(tuple(start), int(corsia), int(campata),
                                        turn_penalty_cost=int(turn_penalty), speed_mps=float(speed))
            elif goal is not None:
                route = wh.find_path(tuple(start), tuple(goal),
                                     turn_penalty_cost=int(turn_penalty), speed_mps=float(speed))
            else:
                self.send_json_response({"status": "error", "message": "Must provide goal coordinates or target bay"}, status=400)
                return

            self.send_json_response({
                "status": "ok",
                "route": {
                    "points": route.points,
                    "steps": route.steps,
                    "distance_meters": route.distance_meters,
                    "estimated_time_sec": route.estimated_time_sec,
                    "total_cost": route.total_cost,
                    "turns": route.turns
                }
            })
        except Exception as e:
            self.send_json_response({"status": "error", "message": str(e)}, status=400)
        finally:
            if wh is not None:
                wh.close()

    def handle_fleet(self, req: Dict[str, Any]):
        corsia = req.get("corsia")
        campata = req.get("campata")
        forklifts = req.get("forklifts", [])

        if corsia is None or campata is None or not forklifts:
            self.send_json_response({"status": "error", "message": "Missing target bay or fleet list"}, status=400)
            return

        wh = None
        try:
            wh = self.load_request_map(req)
            turn_penalty, speed = self.prepare_route(req, wh)
            corsia = integer(corsia, "corsia", 1)
            campata = integer(campata, "campata", 1)
            if not isinstance(forklifts, list) or len(forklifts) > 256:
                raise ValueError("forklifts must be a list of at most 256 vehicles")
            op_tuples = []
            for i, f in enumerate(forklifts):
                if not isinstance(f, dict):
                    raise ValueError("Each forklift must be an object with x and y")
                position = self.point([f.get("x"), f.get("y")], wh)
                op_tuples.append((integer(f.get("id", i + 1), "forklift id", 1), *position))
            if len({f[0] for f in op_tuples}) != len(op_tuples):
                raise ValueError("Forklift IDs must be unique")
            res = wh.find_nearest_operator(op_tuples, int(corsia), int(campata),
                                          turn_penalty_cost=int(turn_penalty), speed_mps=float(speed))

            self.send_json_response({
                "status": "ok",
                "best_operator_id": res["best_operator_id"],
                "best_operator_index": res["best_operator_index"],
                "start": res["start"],
                "target_cell": res["target_cell"],
                "distance_meters": res["distance_meters"],
                "route": {
                    "points": res["route"].points,
                    "steps": res["route"].steps,
                    "distance_meters": res["route"].distance_meters,
                    "estimated_time_sec": res["route"].estimated_time_sec,
                    "turns": res["route"].turns
                }
            })
        except Exception as e:
            self.send_json_response({"status": "error", "message": str(e)}, status=400)
        finally:
            if wh is not None:
                wh.close()

    def handle_convert(self, req: Dict[str, Any]):
        try:
            imported = import_map(req.get("content", req.get("csv", "")), req.get("format", "auto"),
                                  req.get("cell_width_cm", 350), req.get("cell_height_cm", 200))
            self.send_json_response({"status": "ok", "xml": imported.to_xml(),
                                     "map": imported.data, "warnings": imported.warnings})
        except (ValueError, TypeError) as exc:
            self.send_json_response({"status": "error", "message": str(exc)}, 400)


def start_server(port: int = 8000):
    server_address = ("127.0.0.1", port)
    httpd = http.server.HTTPServer(server_address, StudioRequestHandler)
    print("=" * 60)
    print(f"🚀 xROUTE Studio running at: http://localhost:{port}")
    print("=" * 60)
    print("Drop an XML, JSON, CSV/TSV, or text grid map to visualize and plan routes automatically.")
    print("Press Ctrl+C to stop.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping xROUTE Studio.")
        httpd.server_close()


if __name__ == "__main__":
    p = 8000
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        p = int(sys.argv[1])
    start_server(p)
