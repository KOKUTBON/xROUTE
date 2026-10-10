"""
xROUTE Python Bindings
High-performance warehouse routing and AGV fleet dispatch library.
"""

import ctypes
import os
import math
import sys
from typing import List, Tuple, Optional

# Find shared library
_lib_names = ["libxroute.dylib", "libxroute.so", "libxroute.dll", "xroute.dll"]
_lib_path = None
_search_dirs = [
    os.path.dirname(__file__),
    os.path.join(os.path.dirname(__file__), "..", "..", "build"),
    os.path.join(os.path.dirname(__file__), "..", "..", "bin"),
    os.path.join(os.path.dirname(__file__), "..", ".."),
]

for d in _search_dirs:
    for name in _lib_names:
        p = os.path.abspath(os.path.join(d, name))
        if os.path.isfile(p):
            _lib_path = p
            break
    if _lib_path:
        break

_lib = None
if _lib_path:
    try:
        _lib = ctypes.CDLL(_lib_path)
    except Exception as e:
        sys.stderr.write(f"Warning: Could not load xROUTE library at {_lib_path}: {e}\n")


class _Point(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int)]


class _Route(ctypes.Structure):
    _fields_ = [
        ("points", ctypes.POINTER(_Point)),
        ("length", ctypes.c_int),
        ("total_cost", ctypes.c_int),
        ("distance_meters", ctypes.c_double),
        ("estimated_time_sec", ctypes.c_double),
        ("turns_count", ctypes.c_int),
    ]


class _Operator(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_int),
        ("x", ctypes.c_int),
        ("y", ctypes.c_int),
        ("name", ctypes.c_char_p),
    ]


class _FleetResult(ctypes.Structure):
    _fields_ = [
        ("best_operator_index", ctypes.c_int),
        ("best_operator_id", ctypes.c_int),
        ("start_point", _Point),
        ("target_cell", _Point),
        ("distance_cost", ctypes.c_int),
        ("distance_meters", ctypes.c_double),
        ("route", ctypes.POINTER(_Route)),
    ]


class _Options(ctypes.Structure):
    _fields_ = [
        ("turn_penalty_cost", ctypes.c_int),
        ("speed_mps", ctypes.c_double),
        ("turn_time_sec", ctypes.c_double),
        ("allow_diagonal", ctypes.c_bool),
    ]


class Route:
    def __init__(self, points: List[Tuple[int, int]], total_cost: int,
                 distance_meters: float, estimated_time_sec: float, turns: int):
        self.points = points
        self.total_cost = total_cost
        self.distance_meters = distance_meters
        self.estimated_time_sec = estimated_time_sec
        self.turns = turns

    @property
    def steps(self) -> int:
        return len(self.points)

    def __repr__(self):
        return f"<Route {self.steps} steps, {self.distance_meters:.2f}m, {self.estimated_time_sec:.1f}s, turns={self.turns}>"


class Warehouse:
    def __init__(self, _map_ptr):
        self._map = _map_ptr
        self._router = None
        if _lib and self._map:
            _lib.xroute_router_create.argtypes = [ctypes.c_void_p]
            _lib.xroute_router_create.restype = ctypes.c_void_p
            self._router = _lib.xroute_router_create(self._map)

    def close(self):
        """Release native resources; safe to call repeatedly."""
        if _lib:
            if self._router:
                _lib.xroute_router_free.argtypes = [ctypes.c_void_p]
                _lib.xroute_router_free(self._router)
                self._router = None
            if self._map:
                _lib.xroute_map_free.argtypes = [ctypes.c_void_p]
                _lib.xroute_map_free(self._map)
                self._map = None

    def __del__(self):
        self.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    @classmethod
    def from_import(cls, imported):
        """Build the native map only after all input validation succeeds."""
        if not _lib:
            raise RuntimeError("Native library unavailable. Run make all first.")
        d = imported.data
        _lib.xroute_map_create.argtypes = [ctypes.c_int] * 4
        _lib.xroute_map_create.restype = ctypes.c_void_p
        _lib.xroute_map_set_cell.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 3 + [ctypes.c_uint8]
        _lib.xroute_map_add_bay.argtypes = [ctypes.c_void_p] + [ctypes.c_int] * 5 + [ctypes.c_uint8] * 2
        ptr = _lib.xroute_map_create(d['rows'], d['cols'], d['cell_width_cm'], d['cell_height_cm'])
        if not ptr:
            raise MemoryError("Unable to allocate warehouse map")
        wh = cls(ctypes.c_void_p(ptr))
        try:
            if not wh._router:
                raise MemoryError("Unable to allocate router")
            for c in d['cells']:
                args = [wh._map, c['x'], c['y'], c['type']]
                if c['type'] in (1, 2):
                    status = _lib.xroute_map_add_bay(*args, c['corsia'], c['campata'],
                                                   c['accessible_sides'], c['movements'])
                else:
                    status = _lib.xroute_map_set_cell(*args, c['movements'])
                if status != 0:
                    raise RuntimeError(f"Failed to build cell: code {status}")
            wh.import_report = imported
            return wh
        except Exception:
            wh.close()
            raise

    @classmethod
    def load(cls, filepath, **options):
        """Automatically detect and validate a UTF-8 map file."""
        from .ingest import MAX_BYTES
        with open(filepath, 'r', encoding='utf-8-sig') as source:
            return cls.load_string(source.read(MAX_BYTES + 1), **options)

    @classmethod
    def load_string(cls, content, **options):
        from .ingest import import_map
        return cls.from_import(import_map(content, **options))

    @classmethod
    def load_xml(cls, filepath: str) -> "Warehouse":
        if not _lib:
            raise RuntimeError("xROUTE native library not loaded")
        _lib.xroute_map_load_xml.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        _lib.xroute_map_load_xml.restype = ctypes.c_int
        m_ptr = ctypes.c_void_p()
        st = _lib.xroute_map_load_xml(filepath.encode("utf-8"), ctypes.byref(m_ptr))
        if st != 0:
            raise IOError(f"Failed to load XML warehouse map: code {st}")
        return cls(m_ptr)

    @classmethod
    def load_csv(cls, filepath: str, cell_width_cm: int = 350, cell_height_cm: int = 200) -> "Warehouse":
        if not _lib:
            raise RuntimeError("xROUTE native library not loaded")
        _lib.xroute_map_load_csv.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
        _lib.xroute_map_load_csv.restype = ctypes.c_int
        m_ptr = ctypes.c_void_p()
        st = _lib.xroute_map_load_csv(filepath.encode("utf-8"), cell_width_cm, cell_height_cm, ctypes.byref(m_ptr))
        if st != 0:
            raise IOError(f"Failed to load CSV warehouse map: code {st}")
        return cls(m_ptr)

    @classmethod
    def load_xml_string(cls, xml_text: str) -> "Warehouse":
        if not _lib:
            raise RuntimeError("xROUTE native library not loaded")
        _lib.xroute_map_load_xml_string.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        _lib.xroute_map_load_xml_string.restype = ctypes.c_int
        m_ptr = ctypes.c_void_p()
        st = _lib.xroute_map_load_xml_string(xml_text.encode("utf-8"), ctypes.byref(m_ptr))
        if st != 0:
            raise IOError(f"Failed to load XML string: code {st}")
        return cls(m_ptr)

    @classmethod
    def load_csv_string(cls, csv_text: str, cell_width_cm: int = 350, cell_height_cm: int = 200) -> "Warehouse":
        import tempfile
        with tempfile.NamedTemporaryFile("w", delete=False, suffix=".csv") as tf:
            tf.write(csv_text)
            tmp_path = tf.name
        try:
            wh = cls.load_csv(tmp_path, cell_width_cm, cell_height_cm)
        finally:
            if os.path.exists(tmp_path):
                os.remove(tmp_path)
        return wh

    @property
    def rows(self) -> int:
        return _lib.xroute_map_get_rows(self._map) if _lib else 0

    @property
    def cols(self) -> int:
        return _lib.xroute_map_get_cols(self._map) if _lib else 0

    @property
    def cell_width_cm(self) -> int:
        return _lib.xroute_map_get_cell_width_cm(self._map) if _lib else 350

    @property
    def cell_height_cm(self) -> int:
        return _lib.xroute_map_get_cell_height_cm(self._map) if _lib else 200

    def _point(self, value):
        from .ingest import integer
        if not self._map or not self._router:
            raise ValueError("Warehouse is closed")
        if not isinstance(value, (list, tuple)) or len(value) != 2:
            raise ValueError("Position must be [x, y]")
        return (integer(value[0], "x", 0, self.cols - 1),
                integer(value[1], "y", 0, self.rows - 1))

    def _options(self, penalty, speed):
        from .ingest import integer
        if not self._map or not self._router:
            raise ValueError("Warehouse is closed")
        penalty = integer(penalty, "turn_penalty_cost", 0, 10000)
        if isinstance(speed, bool) or not isinstance(speed, (int, float)) or not math.isfinite(speed) or speed <= 0:
            raise ValueError("speed_mps must be a finite positive number")
        if not math.isfinite(5 * self.rows * self.cols * max(self.cell_width_cm, self.cell_height_cm) / speed):
            raise ValueError("speed_mps is too small to compute finite travel times")
        if self.rows * self.cols * 5 * (max(self.cell_width_cm, self.cell_height_cm) + 2 * penalty) >= (2**31 - 1) // 4:
            raise ValueError("Map scale and turn penalty exceed native routing cost capacity")
        return _Options(turn_penalty_cost=penalty, speed_mps=speed, turn_time_sec=2.0, allow_diagonal=False)

    def find_path(self, start: Tuple[int, int], goal: Tuple[int, int],
                  turn_penalty_cost: int = 0, speed_mps: float = 1.5) -> Route:
        opts = self._options(turn_penalty_cost, speed_mps)
        p_start = _Point(*self._point(start))
        p_goal = _Point(*self._point(goal))
        r_ptr = ctypes.POINTER(_Route)()

        _lib.xroute_router_find_path.argtypes = [
            ctypes.c_void_p, _Point, _Point, ctypes.POINTER(_Options), ctypes.POINTER(ctypes.POINTER(_Route))
        ]
        _lib.xroute_router_find_path.restype = ctypes.c_int

        st = _lib.xroute_router_find_path(self._router, p_start, p_goal, ctypes.byref(opts), ctypes.byref(r_ptr))
        if st != 0 or not r_ptr:
            raise ValueError(f"No path found: code {st}")

        pts = [(r_ptr.contents.points[i].x, r_ptr.contents.points[i].y) for i in range(r_ptr.contents.length)]
        route = Route(
            points=pts,
            total_cost=r_ptr.contents.total_cost,
            distance_meters=r_ptr.contents.distance_meters,
            estimated_time_sec=r_ptr.contents.estimated_time_sec,
            turns=r_ptr.contents.turns_count
        )
        _lib.xroute_route_free.argtypes = [ctypes.c_void_p]
        _lib.xroute_route_free(r_ptr)
        return route

    def route_to_bay(self, start: Tuple[int, int], corsia: int, campata: int,
                     turn_penalty_cost: int = 0, speed_mps: float = 1.5) -> Route:
        opts = self._options(turn_penalty_cost, speed_mps)
        p_start = _Point(*self._point(start))
        r_ptr = ctypes.POINTER(_Route)()

        _lib.xroute_router_route_to_bay.argtypes = [
            ctypes.c_void_p, _Point, ctypes.c_int, ctypes.c_int,
            ctypes.POINTER(_Options), ctypes.POINTER(ctypes.POINTER(_Route))
        ]
        _lib.xroute_router_route_to_bay.restype = ctypes.c_int

        from .ingest import integer
        corsia = integer(corsia, "corsia", 1)
        campata = integer(campata, "campata", 1)
        st = _lib.xroute_router_route_to_bay(self._router, p_start, corsia, campata, ctypes.byref(opts), ctypes.byref(r_ptr))
        if st != 0 or not r_ptr:
            raise ValueError(f"No path to bay (corsia {corsia}, campata {campata}): code {st}")

        pts = [(r_ptr.contents.points[i].x, r_ptr.contents.points[i].y) for i in range(r_ptr.contents.length)]
        route = Route(
            points=pts,
            total_cost=r_ptr.contents.total_cost,
            distance_meters=r_ptr.contents.distance_meters,
            estimated_time_sec=r_ptr.contents.estimated_time_sec,
            turns=r_ptr.contents.turns_count
        )
        _lib.xroute_route_free.argtypes = [ctypes.c_void_p]
        _lib.xroute_route_free(r_ptr)
        return route

    def find_nearest_operator(self, operators: List[Tuple[int, int, int]], corsia: int, campata: int,
                             turn_penalty_cost: int = 0, speed_mps: float = 1.5):
        opts = self._options(turn_penalty_cost, speed_mps)
        from .ingest import integer
        corsia = integer(corsia, "corsia", 1)
        campata = integer(campata, "campata", 1)
        if not isinstance(operators, (list, tuple)) or not 1 <= len(operators) <= 256:
            raise ValueError("Provide between 1 and 256 operators")
        ids = set()
        op_array = (_Operator * len(operators))()
        for idx, op in enumerate(operators):
            if not isinstance(op, (list, tuple)) or len(op) != 3:
                raise ValueError("Operator must be (id, x, y)")
            op_id = integer(op[0], "operator id", 1)
            if op_id in ids:
                raise ValueError("Operator IDs must be unique")
            ids.add(op_id)
            op_array[idx].id = op_id
            op_array[idx].x, op_array[idx].y = self._point(op[1:])
            op_array[idx].name = b"Forklift"

        res = _FleetResult()
        _lib.xroute_router_find_nearest_operator.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(_Operator), ctypes.c_int, ctypes.c_int, ctypes.c_int,
            ctypes.POINTER(_Options), ctypes.POINTER(_FleetResult)
        ]
        _lib.xroute_router_find_nearest_operator.restype = ctypes.c_int

        st = _lib.xroute_router_find_nearest_operator(self._router, op_array, len(operators), corsia, campata, ctypes.byref(opts), ctypes.byref(res))
        if st != 0 or not res.route:
            raise ValueError(f"Fleet search failed: code {st}")

        r_ptr = res.route
        pts = [(r_ptr.contents.points[i].x, r_ptr.contents.points[i].y) for i in range(r_ptr.contents.length)]
        route = Route(
            points=pts,
            total_cost=r_ptr.contents.total_cost,
            distance_meters=r_ptr.contents.distance_meters,
            estimated_time_sec=r_ptr.contents.estimated_time_sec,
            turns=r_ptr.contents.turns_count
        )
        out = {
            "best_operator_index": res.best_operator_index,
            "best_operator_id": res.best_operator_id,
            "start": (res.start_point.x, res.start_point.y),
            "target_cell": (res.target_cell.x, res.target_cell.y),
            "distance_meters": res.distance_meters,
            "route": route
        }
        _lib.xroute_fleet_result_free.argtypes = [ctypes.POINTER(_FleetResult)]
        _lib.xroute_fleet_result_free(ctypes.byref(res))
        return out

    def set_dynamic_obstacle(self, x: int, y: int, is_blocked: bool = True):
        x, y = self._point((x, y))
        _lib.xroute_map_set_dynamic_obstacle.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_bool]
        _lib.xroute_map_set_dynamic_obstacle.restype = ctypes.c_int
        status = _lib.xroute_map_set_dynamic_obstacle(self._map, x, y, is_blocked)
        if status != 0:
            raise ValueError(f"Invalid obstacle coordinates: ({x}, {y})")

    def print_ascii(self):
        _lib.xroute_visualizer_print_map.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        _lib.xroute_visualizer_print_map(self._map, None)

    def to_dict(self):
        """Export map representation as JSON-serializable dictionary for Web UI."""
        rows = self.rows
        cols = self.cols

        _lib.xroute_map_get_cell_type.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        _lib.xroute_map_get_cell_type.restype = ctypes.c_int

        _lib.xroute_map_get_allowed_movements.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        _lib.xroute_map_get_allowed_movements.restype = ctypes.c_uint8

        class _Bay(ctypes.Structure):
            _fields_ = [
                ("x", ctypes.c_int), ("y", ctypes.c_int),
                ("corsia", ctypes.c_int), ("campata", ctypes.c_int),
                ("type", ctypes.c_int),
                ("accessible_sides", ctypes.c_uint8),
                ("allowed_movements", ctypes.c_uint8)
            ]

        _lib.xroute_map_get_all_bays.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
        _lib.xroute_map_get_all_bays.restype = ctypes.POINTER(_Bay)

        bay_count = ctypes.c_int(0)
        bays_ptr = _lib.xroute_map_get_all_bays(self._map, ctypes.byref(bay_count))

        bays_list = []
        bay_lookup = {}
        for i in range(bay_count.value):
            b = bays_ptr[i]
            item = {
                "x": b.x, "y": b.y,
                "corsia": b.corsia, "campata": b.campata,
                "type": b.type,
                "accessible_sides": {
                    "left": bool(b.accessible_sides & 1),
                    "up": bool(b.accessible_sides & 2),
                    "right": bool(b.accessible_sides & 4),
                    "down": bool(b.accessible_sides & 8)
                },
                "allowed_movements": {
                    "left": bool(b.allowed_movements & 1),
                    "up": bool(b.allowed_movements & 2),
                    "right": bool(b.allowed_movements & 4),
                    "down": bool(b.allowed_movements & 8)
                }
            }
            bays_list.append(item)
            bay_lookup[(b.x, b.y)] = item

        cells = []
        for y in range(rows):
            row = []
            for x in range(cols):
                t = _lib.xroute_map_get_cell_type(self._map, x, y)
                m = _lib.xroute_map_get_allowed_movements(self._map, x, y)
                cell_data = {
                    "x": x, "y": y, "type": t,
                    "movements": {
                        "left": bool(m & 1), "up": bool(m & 2),
                        "right": bool(m & 4), "down": bool(m & 8)
                    }
                }
                if (x, y) in bay_lookup:
                    cell_data["bay"] = bay_lookup[(x, y)]
                row.append(cell_data)
            cells.append(row)

        return {
            "rows": rows,
            "cols": cols,
            "cell_width_cm": self.cell_width_cm,
            "cell_height_cm": self.cell_height_cm,
            "cells": cells,
            "bays": bays_list
        }
