"""Dependency-free, strict map adapters sharing a canonical warehouse schema.

Coordinates are zero-based; dimensions are centimeters. Never infer geometry from
an unsupported drawing, image, or arbitrary text: ambiguous input must be fixed.
"""
import csv
import io
import json
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass

MAX_BYTES = 4 * 1024 * 1024
MAX_CELLS = 40000
DIRECTIONS = {'left': 1, 'up': 2, 'right': 4, 'down': 8}
ALIASES = {'sx': 'left', 'dx': 'right', 'dw': 'down'}
TYPES = {'floor': 0, 'rack': 1, 'transit_rack': 2, 'oneway': 3, 'wall': 4}


class ImportError(ValueError):
    """Actionable input error, safe to display in Studio or batch output."""


def integer(value, name, low=0, high=MAX_CELLS):
    if isinstance(value, bool) or not re.fullmatch(r'[+-]?\d+', str(value)):
        raise ImportError(f'{name}: expected an integer, got {value!r}')
    if len(str(value).lstrip('+-').lstrip('0')) > 10:
        raise ImportError(f'{name}: integer is too large')
    value = int(value)
    if not low <= value <= high:
        raise ImportError(f'{name}: must be between {low} and {high}')
    return value


def mask(value, name):
    if isinstance(value, int) and not isinstance(value, bool):
        return integer(value, name, 0, 15)
    if isinstance(value, dict):
        if any(type(v) is not bool for v in value.values()):
            raise ImportError(f'{name}: direction values must be booleans')
        if set(value) - set(DIRECTIONS):
            raise ImportError(f'{name}: unknown direction')
        value = [k for k, v in value.items() if v]
    if isinstance(value, str):
        if value.isdecimal():
            return integer(value, name, 0, 15)
        value = value.split()
    if not isinstance(value, list):
        raise ImportError(f'{name}: expected direction names or a bitmask')
    result = 0
    for direction in value:
        if not isinstance(direction, str):
            raise ImportError(f'{name}: expected direction names')
        direction = ALIASES.get(direction, direction)
        if direction not in DIRECTIONS:
            raise ImportError(f'{name}: unknown direction {direction!r}')
        result |= DIRECTIONS[direction]
    return result


@dataclass
class ImportedMap:
    format: str
    data: dict
    warnings: list

    def to_xml(self):
        d = self.data
        root = ET.Element('Magazzino', numRighe=str(d['rows']), numColonne=str(d['cols']),
                          dimColonne=str(d['cell_width_cm']), dimRighe=str(d['cell_height_cm']))
        for c in d['cells']:
            attrs = {k: str(c[v]) for k, v in [('x', 'x'), ('y', 'y'), ('Tipo', 'type')]}
            if c['type'] in (1, 2):
                attrs.update(Corsia=str(c['corsia']), Campata=str(c['campata']))
            # Explicit masks preserve empty movement/access sets on reimport.
            attrs['movements'] = str(c['movements'])
            if 'accessible_sides' in c:
                attrs['accessible_sides'] = str(c['accessible_sides'])
            elem = ET.SubElement(root, 'Cella', attrs)
            for field, tag in [('movements', 'SpostamentoPermesso'), ('accessible_sides', 'LatoAccessibile')]:
                for name, bit in DIRECTIONS.items():
                    if c.get(field, 0) & bit:
                        ET.SubElement(elem, tag).text = {'left': 'sx', 'right': 'dx', 'down': 'dw'}.get(name, name)
        return ET.tostring(root, encoding='unicode')


def _xml(content):
    if re.search(r'<!\s*(DOCTYPE|ENTITY)', content, re.I):
        raise ImportError('XML document types and entities are not supported')
    root = ET.fromstring(content)
    if root.tag not in ('Magazzino', 'Deposito'):
        raise ImportError('XML root must be Magazzino or Deposito')
    dim = root.find('Deposito')
    attrs = dict(root.attrib)
    if dim is not None:
        attrs.update(dim.attrib)
    data = {'rows': attrs.get('numRighe'), 'cols': attrs.get('numColonne'),
            'cell_width_cm': attrs.get('dimColonne', 350), 'cell_height_cm': attrs.get('dimRighe', 200), 'cells': []}
    for e in root.iter('Cella'):
        c = {'x': e.get('x'), 'y': e.get('y'), 'type': e.get('Tipo', 0)}
        for old, new in [('Corsia', 'corsia'), ('Campata', 'campata')]:
            if old in e.attrib:
                c[new] = e.get(old)
        for field, tag in [('movements', 'SpostamentoPermesso'), ('accessible_sides', 'LatoAccessibile')]:
            if field in e.attrib:
                c[field] = integer(e.get(field), field, 0, 15)
            elif e.findall(tag):
                c[field] = [(v.text or '').strip() for v in e.findall(tag)]
        data['cells'].append(c)
    return data


def _grid(matrix, legacy=False):
    if not isinstance(matrix, list) or not matrix or any(not isinstance(row, list) for row in matrix):
        raise ImportError('Grid must be a list of rows')
    if not matrix[0] or any(len(row) != len(matrix[0]) for row in matrix):
        raise ImportError('Grid must be nonempty and rectangular; check row lengths')
    rows, cols = len(matrix), len(matrix[0]) - int(legacy)
    if cols <= 0 or rows * cols > MAX_CELLS:
        raise ImportError(f'Grid must contain 1–{MAX_CELLS} cells')
    cells = []
    arrows = {'<': 'left', '^': 'up', '>': 'right', 'v': 'down'}
    for y, row in enumerate(matrix):
        aisle = integer(row[0] or 0, f'row {y + 1} aisle') if legacy else y + 1
        for x, raw in enumerate(row[int(legacy):]):
            token = str(raw).strip()
            c = {'x': x, 'y': y}
            if token in ('', '0', '.', '-', 'floor'):
                continue
            if token in ('#', 'x', 'X', 'wall'):
                c['type'] = 4
            elif token in arrows:
                c.update(type=3, movements=[arrows[token]])
            elif token.isdecimal() and int(token) > 0:
                c.update(type=1, corsia=aisle, campata=int(token))
            else:
                raise ImportError(f'Row {y + 1}, column {x + 1}: unknown grid token {token!r}')
            cells.append(c)
    return {'rows': rows, 'cols': cols, 'cells': cells}


def _table(content, fmt):
    delimiter = '\t' if fmt == 'tsv' else max(',;\t', key=content.splitlines()[0].count)
    table = list(csv.reader(io.StringIO(content), delimiter=delimiter, strict=True))
    if not table:
        raise ImportError('Empty table')
    header = [v.strip().lower() for v in table[0]]
    if {'x', 'y', 'type'} <= set(header):
        if len(set(header)) != len(header):
            raise ImportError('Duplicate table column name')
        if any(len(row) != len(header) for row in table[1:]):
            raise ImportError('Every record must have the same columns as the header')
        cells = [{k: v.strip() for k, v in zip(header, row) if v.strip()} for row in table[1:]]
        if not cells:
            raise ImportError('Coordinate table contains no cells')
        return {'rows': max(integer(c.get('y'), 'y') for c in cells) + 1,
                'cols': max(integer(c.get('x'), 'x') for c in cells) + 1, 'cells': cells}
    # Legacy format has dimensions then an entirely empty separator row.
    if len(table) > 1 and not any(v.strip() for v in table[1]) and len(table[0]) >= 2:
        rows = integer(table[0][0], 'rows', 1)
        cols = integer(table[0][1], 'cols', 1)
        matrix = table[2:]
        # Legacy spreadsheet exports commonly include one trailing empty column.
        matrix = [r[:-1] if len(r) == cols + 2 and not r[-1].strip() else r for r in matrix]
        if len(matrix) != rows or any(len(r) != cols + 1 for r in matrix):
            raise ImportError('Legacy CSV dimensions do not match its rows/columns (including aisle column)')
        return _grid(matrix, legacy=True)
    return _grid(table)


def normalize(data, fmt, width, height):
    if isinstance(data, list):
        data = _grid(data)
    if not isinstance(data, dict):
        raise ImportError('JSON must be a map object or a rectangular grid')
    if 'grid' in data:
        data = {**data, **_grid(data['grid'])}
    rows = integer(data.get('rows'), 'rows', 1)
    cols = integer(data.get('cols'), 'cols', 1)
    if rows * cols > MAX_CELLS:
        raise ImportError(f'Map exceeds the {MAX_CELLS} cell limit')
    out = {'rows': rows, 'cols': cols,
           'cell_width_cm': integer(data.get('cell_width_cm', width), 'cell_width_cm', 1, 10000),
           'cell_height_cm': integer(data.get('cell_height_cm', height), 'cell_height_cm', 1, 10000), 'cells': []}
    cells = data.get('cells', [])
    if not isinstance(cells, list):
        raise ImportError('cells must be a list of coordinate records')
    occupied, bays = {}, set()
    for index, raw in enumerate(cells):
        if not isinstance(raw, dict):
            raise ImportError(f'Cell {index + 1}: expected an object')
        x = integer(raw.get('x'), f'cell {index + 1} x', 0, cols - 1)
        y = integer(raw.get('y'), f'cell {index + 1} y', 0, rows - 1)
        if (x, y) in occupied:
            raise ImportError(f'Duplicate cell at ({x}, {y})')
        t = raw.get('type', 0)
        t = integer(TYPES.get(t, t) if isinstance(t, str) else t, 'cell type', 0, 4)
        c = {'x': x, 'y': y, 'type': t,
             'movements': mask(raw.get('movements', 0 if t in (1, 4) else 15), 'movements')}
        if t in (1, 4) and c['movements']:
            raise ImportError(f'Blocked cell ({x}, {y}) cannot have allowed movements')
        if t in (1, 2):
            c.update(corsia=integer(raw.get('corsia'), 'corsia', 1),
                     campata=integer(raw.get('campata'), 'campata', 1))
            key = (c['corsia'], c['campata'])
            if key in bays:
                raise ImportError(f'Duplicate aisle/bay identifier {key}')
            bays.add(key)
            if 'accessible_sides' in raw:
                c['accessible_sides'] = mask(raw['accessible_sides'], 'accessible_sides')
        occupied[x, y] = c
        out['cells'].append(c)
    warnings = []
    inferred = 0
    for c in out['cells']:
        if c['type'] not in (1, 2):
            continue
        available = 0
        for dx, dy, bit in [(-1, 0, 1), (0, -1, 2), (1, 0, 4), (0, 1, 8)]:
            x, y = c['x'] + dx, c['y'] + dy
            if 0 <= x < cols and 0 <= y < rows and occupied.get((x, y), {}).get('type', 0) not in (1, 4):
                available |= bit
        if 'accessible_sides' not in c:
            c['accessible_sides'] = available
            inferred += 1
        if not c['accessible_sides'] & available:
            warnings.append(f"Bay {c['corsia']}/{c['campata']} has no traversable access neighbor")
    if inferred:
        warnings.insert(0, f'Inferred access sides for {inferred} bays from adjacent traversable cells; review before use')
    if 5 * rows * cols * max(out['cell_width_cm'], out['cell_height_cm']) >= (2**31 - 1) // 4:
        raise ImportError('Map dimensions and scale exceed native routing cost capacity')
    if sum(c['type'] in (1, 4) for c in out['cells']) == rows * cols:
        warnings.append('Map contains no traversable cells')
    if len(json.dumps(out).encode('utf-8')) > MAX_BYTES:
        raise ImportError('Expanded map exceeds the 4 MiB normalized map limit')
    return ImportedMap(fmt, out, warnings)


def import_map(content, format='auto', cell_width_cm=350, cell_height_cm=200):
    """Detect and validate UTF-8 XML, JSON, CSV/TSV, or ASCII grids."""
    if not isinstance(content, str) or not content.strip():
        raise ImportError('Provide nonempty map text')
    if len(content.encode('utf-8')) > MAX_BYTES:
        raise ImportError('Map exceeds the 4 MiB text limit')
    content = content.strip().lstrip('\ufeff').strip()
    if '\x00' in content:
        raise ImportError('Binary input is unsupported; export a structured UTF-8 map')
    if not isinstance(format, str):
        raise ImportError('format must be a string')
    fmt = format.lower()
    if fmt == 'auto':
        if content.startswith(('{', '[')):
            fmt = 'json'
        elif re.fullmatch(r'[.0-9#xX<>^v\- \r\n]+', content):
            fmt = 'ascii'
        elif content.startswith('<') and re.match(r'<[!?A-Za-z]', content):
            fmt = 'xml'
        elif any(c in content for c in ',;\t'):
            fmt = 'tsv' if '\t' in content else 'csv'
        else:
            fmt = 'ascii'
    try:
        if fmt == 'xml':
            data = _xml(content)
        elif fmt == 'json':
            data = json.loads(content)
        elif fmt in ('csv', 'tsv'):
            data = _table(content, fmt)
        elif fmt in ('ascii', 'txt'):
            data = _grid([list(line.strip()) for line in content.splitlines()])
            fmt = 'ascii'
        else:
            raise ImportError('Unsupported format; use XML, JSON, CSV, TSV, or ASCII')
        return normalize(data, fmt, cell_width_cm, cell_height_cm)
    except (ET.ParseError, json.JSONDecodeError, csv.Error, RecursionError) as exc:
        raise ImportError(f'Invalid {fmt.upper()}: {exc}') from exc
