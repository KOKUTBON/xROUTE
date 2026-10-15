const test = require('node:test');
const assert = require('node:assert/strict');
const { handle, parse } = require('../../demo/demo-api.js');

const map = { rows: 5, cols: 7, cell_width_cm: 120, cell_height_cm: 100, cells: [
  { x: 2, y: 2, type: 'rack', corsia: 1, campata: 1, accessible_sides: ['up'] },
  { x: 3, y: 2, type: 'rack', corsia: 1, campata: 2 },
  { x: 5, y: 3, type: 'wall' },
  { x: 0, y: 1, type: 'oneway', movements: ['down'] }
] };
const request = { map_content: JSON.stringify(map), map_format: 'json', speed: 1.5, turn_penalty: 50 };

test('parse produces the Studio schema and a canonical export that can be reimported', () => {
  const result = handle('/api/parse', { content: request.map_content });
  assert.equal(result.map.cells[2][2].bay.accessible_sides.up, true);
  assert.equal(result.map.cells[1][0].movements.down, true);
  assert.equal(result.map.bays.length, 2);
  assert.deepEqual(parse(result.normalized_content, 'json').map, result.map);
  assert.ok(result.warnings.some(w => w.includes('Inferred')));
});

test('routing returns physical distance, turn cost and time', () => {
  const { route } = handle('/api/route', { ...request, start: [0, 0], goal: [3, 1] });
  assert.deepEqual(route.points[0], [0, 0]);
  assert.deepEqual(route.points.at(-1), [3, 1]);
  assert.equal(route.distance_meters, 4.6);
  assert.equal(route.turns, 1);
  assert.equal(route.total_cost, 510);
  assert.equal(route.estimated_time_sec, 4.6 / 1.5 + 2);
  assert.equal(route.steps, 5);
});

test('hazards reroute and unreachable or blocked positions fail', () => {
  const input = { ...request, start: [1, 0], goal: [4, 0] };
  const direct = handle('/api/route', input).route;
  const detour = handle('/api/route', { ...input, blocked_cells: [[2, 0]] }).route;
  assert.ok(detour.distance_meters > direct.distance_meters);
  assert.ok(detour.points.every(p => p.join(',') !== '2,0'));
  assert.throws(() => handle('/api/route', { ...input, blocked_cells: [[4, 0]] }), /No path/);
  assert.throws(() => handle('/api/route', { ...input, goal: [2, 2] }), /No path/);
  const sealed = { rows: 1, cols: 3, cells: [{ x: 1, y: 0, type: 'wall' }] };
  assert.throws(() => handle('/api/route', { map_content: JSON.stringify(sealed), start: [0, 0], goal: [2, 0] }), /No path/);
});

test('one-way restrictions apply to departures', () => {
  const result = handle('/api/route', { ...request, start: [0, 1], goal: [1, 1] }).route;
  assert.deepEqual(result.points[1], [0, 2]);
  assert.ok(result.points.length > 2);
});

test('bay routing selects an accessible neighbor; fleet skips unreachable vehicles', () => {
  const input = { ...request, corsia: 1, campata: 1 };
  assert.deepEqual(handle('/api/route', { ...input, start: [0, 0] }).route.points.at(-1), [2, 1]);
  const result = handle('/api/fleet', { ...input, forklifts: [{ id: 10, x: 0, y: 0 }, { id: 20, x: 2, y: 1 }, { id: 30, x: 5, y: 3 }] });
  assert.equal(result.best_operator_id, 20);
  assert.equal(result.best_operator_index, 1);
  assert.deepEqual(result.start, [2, 1]);
  assert.deepEqual(result.target_cell, [2, 1]);
  assert.equal(result.route.distance_meters, 0);
  assert.throws(() => handle('/api/fleet', { ...input, forklifts: [{ id: 1, x: 0, y: 0 }, { id: 1, x: 1, y: 0 }] }), /unique/);
});

test('imports support ASCII, CSV/TSV, legacy CSV, coordinate tables and JSON grids', () => {
  assert.equal(parse('...\n.#.\n...').map.cells[1][1].type, 4);
  assert.equal(parse('0,0,0\n0,1,0').map.bays.length, 1);
  assert.equal(parse('0\t0\n0\t#').map.cells[1][1].type, 4);
  assert.equal(parse('2;3\n;;;\n1;0;1;0\n2;0;0;0').map.bays[0].corsia, 1);
  assert.equal(parse('x,y,type\n2,3,wall').map.rows, 4);
  assert.equal(parse(JSON.stringify({ grid: [['.', '>'], ['#', '.']] })).map.cells[0][1].movements.right, true);
});

test('conversion preserves explicit movement and access masks', () => {
  const result = handle('/api/convert', { content: request.map_content });
  assert.ok(result.xml.includes('movements="8"'));
  assert.ok(result.xml.includes('accessible_sides="2"'));
  assert.equal(result.map.rows, 5);
});

test('invalid requests and malformed imports are rejected', () => {
  assert.throws(() => parse('..\n.'), /rectangular/);
  assert.throws(() => parse(JSON.stringify({ rows: 201, cols: 201 })), /40000/);
  assert.throws(() => parse(JSON.stringify({ rows: 2, cols: 2, cells: [{ x: 0, y: 0 }, { x: 0, y: 0 }] })), /Duplicate/);
  assert.throws(() => handle('/api/route', { ...request, start: [-1, 0], goal: [0, 0] }), /integer/);
  assert.throws(() => handle('/api/route', { ...request, start: [0, 0], goal: [0, 0], speed: -1 }), /speed/);
  assert.throws(() => handle('/api/route', { ...request, start: [0, 0], goal: [0, 0], blocked_cells: 'bad' }), /list/);
  assert.throws(() => handle('/api/missing'), /Endpoint/);
});
