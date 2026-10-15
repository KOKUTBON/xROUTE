# Automated warehouse imports

Studio, `Warehouse.load` / `load_string`, and `python -m xroute` use the same
validated import pipeline. Content determines the format, not the filename.
The existing native C XML/CSV functions remain available as legacy interfaces;
they do not implement these additional formats or strict validation rules.

## Start and automate

From the repository root, with Python 3.9+ and a C99 compiler installed:

```sh
make studio                         # builds libraries and starts localhost:8000
make check                          # native + Python + HTTP integration tests
PYTHONPATH=python python3 -m xroute validate examples/maps/warehouse.json
PYTHONPATH=python python3 -m xroute convert examples/maps/grid.txt --to xml -o /tmp/map.xml
PYTHONPATH=python python3 -m xroute route examples/maps/grid.txt --start 0 0 --goal 6 4
```

Use `-` as input to read stdin. Validation emits JSON; invalid input returns
exit code 1 and a JSON error on stderr. Validation and conversion do not require
the native library. Routing and Studio require `make all` (automatic with
`make studio`). No pip dependencies or remote parsing service are required.
Studio's existing styling still uses Tailwind and FontAwesome CDNs.

## Supported formats

| Input | Interpretation |
| --- | --- |
| XML | `Magazzino` / `Deposito`, including nested Deposito dimensions and Cella records |
| JSON object | Explicit rows, cols, metric dimensions, sparse cell records |
| JSON grid | Rectangular array of arrays, or an object with a `grid` array |
| CSV / TSV | Comma, semicolon, or tab-delimited rectangular grid; quoted fields supported |
| Coordinate table | Header containing `x,y,type`, optional `corsia,campata,movements,accessible_sides` |
| Legacy CSV | Dimensions row, empty separator row, then aisle identifier + cell values per row |
| ASCII text | One row per line; one character per cell |

All coordinates are zero-based: x is column, y is row. Sparse unspecified cells
are open floor. Coordinate tables infer their extent from maximum x/y; use JSON
for additional empty border rows or columns. JSON/XML dimensions take precedence
over import options. Otherwise dimensions default to 350 × 200 cm and can be
set with `--cell-width-cm` and `--cell-height-cm` in batch commands, Python
arguments, or the corresponding API fields.

### Grid legend

- `.`, `-`, `0`, or an empty delimited field: floor.
- `#`, `x`, `X`: wall/obstacle.
- `<`, `^`, `>`, `v`: left/up/right/down one-way cell.
- Positive integers: rack bay numbers. Aisle defaults to the one-based row
  number. Legacy CSV uses its explicit first-column aisle number.

ASCII uses single digits for bay numbers. CSV, TSV, and JSON grids support
multiple-digit bay numbers. Spaces inside an ASCII row represent floor;
leading/trailing whitespace is trimmed. Use `.` to retain empty borders.
Rack access sides are inferred from adjacent traversable cells when omitted;
Studio reports this inference for review. Explicit empty access sets remain empty.

### Canonical JSON

```json
{
  "rows": 3,
  "cols": 5,
  "cell_width_cm": 100,
  "cell_height_cm": 150,
  "cells": [
    {"x": 2, "y": 1, "type": "rack", "corsia": 1, "campata": 10,
     "accessible_sides": ["up", "down"]},
    {"x": 4, "y": 1, "type": "oneway", "movements": ["up"]}
  ]
}
```

Cell types are `floor` (0), `rack` (1), `transit_rack` (2), `oneway` (3), and
`wall` (4). Directions accept name lists, space-separated names, boolean
objects keyed by direction, or a bitmask: left=1, up=2, right=4, down=8.
Normalized exports use numeric types and masks. Grid rack access is inferred;
JSON/XML can specify it explicitly. A missing movement set on traversable
cells means all directions; an explicit empty set means no outgoing movement.

Normalized JSON preserves dimensions, bay identifiers, movement masks, and
access sides. XML output includes standard direction child tags plus numeric
`movements` and `accessible_sides` attributes so explicit empty sets survive
reimport. Older C XML readers ignore those attributes, so use the new import
pipeline for exact round trips involving empty masks or restricted floor cells.
Dynamic hazards are session state, not part of exported static maps.

## Python and HTTP

```python
from xroute import Warehouse

with Warehouse.load("examples/maps/warehouse.json") as warehouse:
    print(warehouse.import_report.warnings)
    print(warehouse.find_path((0, 0), (6, 4)))
```

`POST /api/parse` accepts `content`, optional `format` (`auto` by default), and
cell dimensions. It returns `map` for rendering, `format` for detection results,
`warnings`, `normalized_content`, and `normalized_format`. Pass the normalized
content/format to `/api/route` or `/api/fleet` to preserve the imported scale.
Both endpoints also accept raw supported input with `map_format: "auto"`.
Both apply `blocked_cells: [[x,y], ...]`. Fleet vehicle IDs must be unique;
at most 256 vehicles may be submitted. `/api/convert` accepts `content` (or the
legacy `csv` field) and returns normalized `map`, `xml`, and `warnings`.
Empty uploads are errors; the demo is available explicitly at `/api/default_map`.

## Validation boundaries

Maps are limited to 4 MiB UTF-8 text (including normalized output) and 40,000 cells; HTTP JSON requests are
limited to 8 MiB including escaping. Dimensions must be positive integer
centimeters, at most 10,000 per axis. Cost bounds are checked before native
routing. Invalid tokens, ragged grids, duplicate coordinates or bay identifiers,
out-of-bounds coordinates, malformed XML/JSON, and invalid directions fail with
a diagnostic. XML DTDs/entities and binary inputs are rejected. Inaccessible bays
and inferred access sides are reported as warnings. This is topology validation,
not a guarantee that every directed route is reachable or physically safe.

Images, PDFs, CAD, geospatial polygons, and arbitrary vendor schemas need explicit
adapters or export to a supported structured format. They are never silently
interpreted as drivable warehouse geometry. Studio is a local planning tool,
not a collision-avoidance controller or authenticated production service.
