/* Browser-only API adapter for the static demo. Never loaded by native Studio. */
(function (root) {
  'use strict';
  const directions = [['left', -1, 0, 1], ['up', 0, -1, 2], ['right', 1, 0, 4], ['down', 0, 1, 8]];
  const types = { floor: 0, rack: 1, transit_rack: 2, oneway: 3, wall: 4 };
  const aliases = { sx: 'left', dx: 'right', dw: 'down' };
  const limit = 4 * 1024 * 1024;
  const fail = message => { throw new Error(message); };
  function integer(value, name, low = 0, high = 40000) {
    if (typeof value === 'boolean' || !/^[+-]?\d+$/.test(String(value)) ||
        !Number.isSafeInteger(Number(value)) || Number(value) < low || Number(value) > high) {
      fail(`${name} must be an integer between ${low} and ${high}`);
    }
    return Number(value);
  }
  function mask(value) {
    if (typeof value === 'number' || (typeof value === 'string' && /^\d+$/.test(value))) {
      return integer(value, 'Direction mask', 0, 15);
    }
    if (typeof value === 'string') value = value.trim().split(/\s+/);
    if (value && !Array.isArray(value) && typeof value === 'object') {
      if (Object.values(value).some(v => typeof v !== 'boolean')) fail('Direction values must be booleans');
      if (Object.keys(value).some(k => !directions.some(d => d[0] === k))) fail('Unknown direction');
      value = Object.keys(value).filter(k => value[k]);
    }
    if (!Array.isArray(value)) fail('Expected direction names or a bitmask');
    return value.reduce((result, name) => {
      const direction = directions.find(d => d[0] === (aliases[name] || name));
      if (!direction) fail(`Unknown direction: ${name}`);
      return result | direction[3];
    }, 0);
  }
  const sideFlags = bits => Object.fromEntries(directions.map(d => [d[0], !!(bits & d[3])]));
  function grid(matrix, legacy = false) {
    if (!Array.isArray(matrix) || !matrix.length || !Array.isArray(matrix[0]) || !matrix[0].length ||
        matrix.some(row => !Array.isArray(row) || row.length !== matrix[0].length)) fail('Grid must be nonempty and rectangular');
    const cols = matrix[0].length - Number(legacy), rows = matrix.length;
    if (cols < 1 || cols * rows > 40000) fail('Map exceeds the 40000 cell limit');
    const cells = [], arrows = { '<': 'left', '^': 'up', '>': 'right', v: 'down' };
    matrix.forEach((row, y) => {
      const aisle = legacy ? integer(row[0] || 0, 'Aisle') : y + 1;
      row.slice(Number(legacy)).forEach((raw, x) => {
        const token = String(raw).trim(), c = { x, y };
        if (['', '0', '.', '-', 'floor'].includes(token)) return;
        if (['#', 'x', 'X', 'wall'].includes(token)) c.type = 4;
        else if (arrows[token]) Object.assign(c, { type: 3, movements: [arrows[token]] });
        else if (/^\d+$/.test(token) && Number(token) > 0) Object.assign(c, { type: 1, corsia: aisle, campata: Number(token) });
        else fail(`Unknown grid token at (${x}, ${y}): ${token}`);
        cells.push(c);
      });
    });
    return { rows, cols, cells };
  }
  function xml(content) {
    if (/<!\s*(DOCTYPE|ENTITY)/i.test(content)) fail('XML document types and entities are unsupported');
    if (!root.DOMParser) fail('XML parsing requires a browser');
    const document = new root.DOMParser().parseFromString(content, 'application/xml');
    if (document.querySelector('parsererror')) fail('Invalid XML');
    const element = document.documentElement;
    if (!['Magazzino', 'Deposito'].includes(element.tagName)) fail('XML root must be Magazzino or Deposito');
    const dimensions = element.querySelector('Deposito');
    const attr = name => dimensions?.getAttribute(name) ?? element.getAttribute(name);
    return { rows: attr('numRighe'), cols: attr('numColonne'),
      cell_width_cm: attr('dimColonne') ?? 350, cell_height_cm: attr('dimRighe') ?? 200,
      cells: Array.from(element.querySelectorAll('Cella'), cell => {
        const c = { x: cell.getAttribute('x'), y: cell.getAttribute('y'), type: cell.getAttribute('Tipo') ?? 0 };
        for (const [attribute, field] of [['Corsia', 'corsia'], ['Campata', 'campata']]) {
          if (cell.hasAttribute(attribute)) c[field] = cell.getAttribute(attribute);
        }
        for (const [field, tag] of [['movements', 'SpostamentoPermesso'], ['accessible_sides', 'LatoAccessibile']]) {
          const children = Array.from(cell.querySelectorAll(tag));
          if (cell.hasAttribute(field)) c[field] = cell.getAttribute(field);
          else if (children.length) c[field] = children.map(child => child.textContent.trim());
        }
        return c;
      }) };
  }
  // A small CSV reader that preserves quoted fields and empty legacy separator rows.
  function table(content, format) {
    const first = content.split(/\r?\n/)[0];
    const delimiter = format === 'tsv' ? '\t' : [',', ';', '\t'].sort((a, b) => first.split(b).length - first.split(a).length)[0];
    const rows = [], row = [];
    let field = '', quoted = false;
    for (let i = 0; i < content.length; i++) {
      const c = content[i];
      if (c === '"') {
        if (quoted && content[i + 1] === '"') { field += '"'; i++; }
        else quoted = !quoted;
      } else if (!quoted && (c === delimiter || c === '\n')) {
        row.push(field.replace(/\r$/, '')); field = '';
        if (c === '\n') { rows.push(row.splice(0)); }
      } else field += c;
    }
    if (quoted) fail('Unterminated quoted CSV field');
    row.push(field.replace(/\r$/, '')); rows.push(row);
    const header = rows[0].map(v => v.trim().toLowerCase());
    if (['x', 'y', 'type'].every(k => header.includes(k))) {
      if (new Set(header).size !== header.length || rows.length < 2 || rows.slice(1).some(r => r.length !== header.length)) fail('Invalid coordinate table');
      const cells = rows.slice(1).map(r => Object.fromEntries(header.map((k, i) => [k, r[i].trim()]).filter(([, v]) => v !== '')));
      return { rows: Math.max(...cells.map(c => integer(c.y, 'y'))) + 1,
        cols: Math.max(...cells.map(c => integer(c.x, 'x'))) + 1, cells };
    }
    if (rows.length > 1 && rows[1].every(v => !v.trim()) && header.length >= 2) {
      const count = integer(rows[0][0], 'rows', 1), cols = integer(rows[0][1], 'cols', 1);
      const matrix = rows.slice(2).map(r => r.length === cols + 2 && !r.at(-1).trim() ? r.slice(0, -1) : r);
      if (matrix.length !== count || matrix.some(r => r.length !== cols + 1)) fail('Legacy CSV dimensions do not match');
      return grid(matrix, true);
    }
    return grid(rows);
  }
  function parse(content, format = 'auto', width = 350, height = 200) {
    if (typeof content !== 'string' || !content.trim()) fail('Provide nonempty map text');
    if (new TextEncoder().encode(content).length > limit) fail('Map exceeds the 4 MiB text limit');
    content = content.trim().replace(/^\uFEFF/, '').trim();
    if (content.includes('\0')) fail('Binary input is unsupported');
    if (typeof format !== 'string') fail('format must be a string');
    format = format.toLowerCase();
    if (format === 'auto') {
      if (/^[{[]/.test(content)) format = 'json';
      else if (/^[.0-9#xX<>^v\- \r\n]+$/.test(content)) format = 'ascii';
      else if (/^<[!?A-Za-z]/.test(content)) format = 'xml';
      else if (/[,;\t]/.test(content)) format = content.includes('\t') ? 'tsv' : 'csv';
      else format = 'ascii';
    }
    let data;
    if (format === 'json') data = JSON.parse(content);
    else if (format === 'xml') data = xml(content);
    else if (['csv', 'tsv'].includes(format)) data = table(content, format);
    else if (['ascii', 'txt'].includes(format)) { data = grid(content.split(/\r?\n/).map(line => [...line.trim()])); format = 'ascii'; }
    else fail('Unsupported format; use XML, JSON, CSV, TSV, or ASCII');
    if (Array.isArray(data)) data = grid(data);
    if (!data || typeof data !== 'object') fail('Expected a map object or grid');
    if (data.grid) data = { ...data, ...grid(data.grid) };
    const rows = integer(data.rows, 'rows', 1), cols = integer(data.cols, 'cols', 1);
    if (rows * cols > 40000) fail('Map exceeds the 40000 cell limit');
    const canonical = { rows, cols, cell_width_cm: integer(data.cell_width_cm ?? width, 'Cell width', 1, 10000),
      cell_height_cm: integer(data.cell_height_cm ?? height, 'Cell height', 1, 10000), cells: [] };
    if (!Array.isArray(data.cells ?? [])) fail('cells must be a list of coordinate records');
    const occupied = new Map(), bayIds = new Set(), warnings = [];
    for (const raw of data.cells ?? []) {
      if (!raw || typeof raw !== 'object') fail('Expected cell object');
      const x = integer(raw.x, 'x', 0, cols - 1), y = integer(raw.y, 'y', 0, rows - 1);
      const key = `${x},${y}`, type = integer(types[raw.type] ?? raw.type ?? 0, 'Cell type', 0, 4);
      if (occupied.has(key)) fail(`Duplicate cell at (${key})`);
      const cell = { x, y, type, movements: mask(raw.movements ?? ([1, 4].includes(type) ? 0 : 15)) };
      if ([1, 4].includes(type) && cell.movements) fail('Blocked cells cannot have allowed movements');
      if ([1, 2].includes(type)) {
        cell.corsia = integer(raw.corsia, 'corsia', 1); cell.campata = integer(raw.campata, 'campata', 1);
        const id = `${cell.corsia}/${cell.campata}`;
        if (bayIds.has(id)) fail(`Duplicate aisle/bay identifier ${id}`);
        bayIds.add(id);
        if (raw.accessible_sides !== undefined) cell.accessible_sides = mask(raw.accessible_sides);
      }
      occupied.set(key, cell); canonical.cells.push(cell);
    }
    let inferred = 0;
    for (const cell of canonical.cells.filter(c => [1, 2].includes(c.type))) {
      const available = directions.reduce((bits, [, dx, dy, bit]) => {
        const x = cell.x + dx, y = cell.y + dy;
        return x >= 0 && y >= 0 && x < cols && y < rows && ![1, 4].includes(occupied.get(`${x},${y}`)?.type) ? bits | bit : bits;
      }, 0);
      if (cell.accessible_sides === undefined) { cell.accessible_sides = available; inferred++; }
      if (!(cell.accessible_sides & available)) warnings.push(`Bay ${cell.corsia}/${cell.campata} has no traversable access neighbor`);
    }
    if (inferred) warnings.unshift(`Inferred access sides for ${inferred} bays from adjacent traversable cells; review before use`);
    if (new TextEncoder().encode(JSON.stringify(canonical)).length > limit) fail('Expanded map exceeds the 4 MiB limit');
    const bays = canonical.cells.filter(c => [1, 2].includes(c.type)).map(c => ({ ...c,
      accessible_sides: sideFlags(c.accessible_sides), allowed_movements: sideFlags(c.movements) }));
    const bayLookup = new Map(bays.map(b => [`${b.x},${b.y}`, b]));
    const cells = Array.from({ length: rows }, (_, y) => Array.from({ length: cols }, (_, x) => {
      const c = occupied.get(`${x},${y}`) ?? { x, y, type: 0, movements: 15 };
      const cell = { ...c, movements: sideFlags(c.movements) };
      if ([1, 2].includes(c.type)) cell.bay = bayLookup.get(`${x},${y}`);
      return cell;
    }));
    return { canonical, format, warnings, map: { ...canonical, cells, bays } };
  }
  class Heap {
    constructor() { this.items = []; }
    push(item) {
      const a = this.items; let i = a.length; a.push(item);
      while (i > 0) { const p = (i - 1) >> 1; if (a[p].score <= item.score) break; a[i] = a[p]; i = p; }
      a[i] = item;
    }
    pop() {
      const a = this.items, first = a[0], last = a.pop();
      if (a.length) {
        let i = 0;
        while (i * 2 + 1 < a.length) {
          let child = i * 2 + 1;
          if (child + 1 < a.length && a[child + 1].score < a[child].score) child++;
          if (a[child].score >= last.score) break;
          a[i] = a[child]; i = child;
        }
        a[i] = last;
      }
      return first;
    }
  }
  function prepare(req) {
    const imported = parse(req.map_content ?? root.XROUTE_DEMO_SAMPLE, req.map_format ?? 'auto', req.cell_width_cm, req.cell_height_cm);
    const map = imported.map, penalty = integer(req.turn_penalty ?? 0, 'Turn penalty', 0, 10000), speed = req.speed ?? 1.5;
    if (typeof speed !== 'number' || !Number.isFinite(speed) || speed <= 0) fail('speed must be a finite positive number');
    const point = p => {
      if (!Array.isArray(p) || p.length !== 2) fail('Position must be [x, y]');
      return [integer(p[0], 'x', 0, map.cols - 1), integer(p[1], 'y', 0, map.rows - 1)];
    };
    if (!Array.isArray(req.blocked_cells ?? [])) fail('blocked_cells must be a list');
    const blocked = new Set((req.blocked_cells ?? []).map(p => point(p).join(',')));
    const traversable = (x, y) => x >= 0 && y >= 0 && x < map.cols && y < map.rows &&
      ![1, 4].includes(map.cells[y][x].type) && !blocked.has(`${x},${y}`);
    return { map, penalty, speed, point, traversable };
  }
  function path(context, start, goal) {
    const { map, penalty, speed, traversable } = context;
    if (!traversable(...start) || !traversable(...goal)) return null;
    const headingCount = penalty > 0 ? 5 : 1, count = map.rows * map.cols * headingCount;
    const costs = new Float64Array(count).fill(Infinity), parents = new Int32Array(count).fill(-1);
    const key = (x, y, dir) => (y * map.cols + x) * headingCount + dir;
    const heuristic = (x, y) => Math.abs(x - goal[0]) * map.cell_width_cm + Math.abs(y - goal[1]) * map.cell_height_cm;
    const heap = new Heap(), first = key(...start, 0);
    costs[first] = 0; heap.push({ id: first, x: start[0], y: start[1], dir: 0, cost: 0, score: heuristic(...start) });
    while (heap.items.length) {
      const u = heap.pop();
      if (u.cost !== costs[u.id]) continue;
      if (u.x === goal[0] && u.y === goal[1]) {
        const points = [];
        for (let id = u.id; id !== -1; id = parents[id]) {
          const node = Math.floor(id / headingCount); points.push([node % map.cols, Math.floor(node / map.cols)]);
        }
        points.reverse();
        let distance = 0, turns = 0, previous = null;
        for (let i = 1; i < points.length; i++) {
          const dx = points[i][0] - points[i - 1][0], dy = points[i][1] - points[i - 1][1], dir = `${dx},${dy}`;
          distance += dx ? map.cell_width_cm : map.cell_height_cm;
          if (previous !== null && previous !== dir) turns++;
          previous = dir;
        }
        return { points, steps: points.length, distance_meters: distance / 100,
          estimated_time_sec: distance / 100 / speed + turns * 2, total_cost: u.cost, turns };
      }
      directions.forEach(([name, dx, dy], i) => {
        const x = u.x + dx, y = u.y + dy;
        if (!map.cells[u.y][u.x].movements[name] || !traversable(x, y)) return;
        const dir = headingCount > 1 ? i + 1 : 0, id = key(x, y, dir);
        const cost = u.cost + (dx ? map.cell_width_cm : map.cell_height_cm) + (u.dir && u.dir !== dir ? penalty : 0);
        if (cost < costs[id]) { costs[id] = cost; parents[id] = u.id; heap.push({ id, x, y, dir, cost, score: cost + heuristic(x, y) }); }
      });
    }
    return null;
  }
  function bayRoute(context, start, corsia, campata) {
    corsia = integer(corsia, 'corsia', 1); campata = integer(campata, 'campata', 1);
    const bay = context.map.bays.find(b => b.corsia === corsia && b.campata === campata);
    if (!bay) fail('Target bay not found');
    let best = null;
    for (const [name, dx, dy] of directions) {
      if (!bay.accessible_sides[name]) continue;
      const route = path(context, start, [bay.x + dx, bay.y + dy]);
      if (route && (!best || route.total_cost < best.total_cost)) best = route;
    }
    return best;
  }
  function toXML(data) {
    return `<Magazzino numRighe="${data.rows}" numColonne="${data.cols}" dimColonne="${data.cell_width_cm}" dimRighe="${data.cell_height_cm}">` +
      data.cells.map(c => '<Cella ' + Object.entries(c).map(([k, v]) => `${({ type: 'Tipo', corsia: 'Corsia', campata: 'Campata' })[k] || k}="${v}"`).join(' ') + '/>').join('') + '</Magazzino>';
  }
  function handle(endpoint, req = {}, method = 'POST') {
    if (endpoint === '/api/default_map' && method === 'GET') return { status: 'ok', xml: root.XROUTE_DEMO_SAMPLE };
    if (method !== 'POST') fail('Method not allowed');
    if (!req || typeof req !== 'object' || Array.isArray(req)) fail('Request must be an object');
    if (endpoint === '/api/parse' || endpoint === '/api/convert') {
      const imported = parse(req.content ?? req.csv ?? '', req.format ?? 'auto', req.cell_width_cm, req.cell_height_cm);
      if (endpoint === '/api/convert') return { status: 'ok', xml: toXML(imported.canonical), map: imported.canonical, warnings: imported.warnings };
      return { status: 'ok', format: imported.format, normalized_content: JSON.stringify(imported.canonical),
        normalized_format: 'json', warnings: imported.warnings, map: imported.map };
    }
    if (!['/api/route', '/api/fleet'].includes(endpoint)) fail('Endpoint not found');
    const context = prepare(req);
    if (endpoint === '/api/route') {
      const start = context.point(req.start);
      const route = req.corsia != null && req.campata != null ? bayRoute(context, start, req.corsia, req.campata) : path(context, start, context.point(req.goal));
      if (!route) fail('No path found: blocked or disconnected destination');
      return { status: 'ok', route };
    }
    if (!Array.isArray(req.forklifts) || !req.forklifts.length || req.forklifts.length > 256) fail('Provide 1–256 forklifts');
    const ids = new Set();
    const forklifts = req.forklifts.map((f, index) => {
      if (!f || typeof f !== 'object') fail('Each forklift must have x and y');
      const id = integer(f.id ?? index + 1, 'Forklift ID', 1), start = context.point([f.x, f.y]);
      if (ids.has(id)) fail('Forklift IDs must be unique');
      ids.add(id); return { id, start, index };
    });
    let best;
    for (const f of forklifts) {
      const route = bayRoute(context, f.start, req.corsia, req.campata);
      if (route && (!best || route.total_cost < best.route.total_cost)) best = { ...f, route };
    }
    if (!best) fail('No forklift can reach the target bay');
    return { status: 'ok', best_operator_id: best.id, best_operator_index: best.index,
      start: best.start, target_cell: best.route.points.at(-1), distance_meters: best.route.distance_meters, route: best.route };
  }
  root.XRouteDemo = { handle, parse };
  if (typeof module !== 'undefined' && module.exports) module.exports = root.XRouteDemo;
  if (!root.document) return;
  const originalFetch = root.fetch.bind(root);
  root.fetch = async function (input, options = {}) {
    const url = new URL(typeof input === 'string' ? input : input.url, root.location.href);
    // Recognize API URLs at either the origin root or the Pages project subpath.
    const apiPath = url.pathname.match(/\/api\/[^/]+$/)?.[0];
    if (url.origin !== root.location.origin || !apiPath) return originalFetch(input, options);
    const method = (options.method ?? input.method ?? 'GET').toUpperCase();
    try {
      const body = options.body ?? (typeof input !== 'string' && method !== 'GET' ? await input.clone().text() : null);
      if (body && new TextEncoder().encode(body).length > limit * 2) fail('Request exceeds 8 MiB');
      return new Response(JSON.stringify(handle(apiPath, body ? JSON.parse(body) : {}, method)), { headers: { 'Content-Type': 'application/json' } });
    } catch (error) {
      return new Response(JSON.stringify({ status: 'error', message: error.message }), { status: 400, headers: { 'Content-Type': 'application/json' } });
    }
  };
})(typeof window !== 'undefined' ? window : globalThis);
