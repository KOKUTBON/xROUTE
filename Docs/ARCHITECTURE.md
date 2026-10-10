# Architecture & Design Specification: `xROUTE`

## 1. Overview & Problem Definition

In modern automated warehouses and distribution centers, material handling equipment (such as manually driven forklifts and autonomous Automated Guided Vehicles / AGVs) must navigate grid topologies with strict physical and operational constraints:
- **Structural Racks & Bays ("Campate")**: Storage aisles consist of racks that occupy grid cells. Forklifts cannot drive through non-traversable racks (Type 1), but can load/unload goods only from designated accessible sides (`up`, `down`, `left`, `right`).
- **One-Way Aisles & Restricted Movements**: To avoid traffic bottlenecks and collisions, certain aisles only permit motion in specific directions (`<SpostamentoPermesso>`).
- **Physical Scaling**: Aisles and cell depths have differing metric dimensions (`dimColonne` width vs `dimRighe` height in centimeters).
- **Vehicle Kinematics**: Forklifts lose significant time and energy making 90-degree and 180-degree turns.
- **Dynamic Fleets**: In multi-vehicle operations, dispatchers must rapidly determine which vehicle among $N$ candidates can service an order with minimal transit time.

xROUTE provides an in-memory routing and dispatch engine written in C99 with zero mandatory runtime dependencies, accessible via a pure C ABI, CLI, and Python.

---

## 2. Core Architectural Components

```
+-------------------------------------------------------------+
|                     Application Layer                       |
|   C Applications  |  Python (ctypes)  |  xROUTE / Scripts   |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                     Public API (xroute.h)                    |
|   map.h   |   router.h   |   parser.h   |   visualizer.h   |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                     Routing & Algorithms                    |
|  - A* Heuristic Search (Binary Min-Heap)                    |
|  - Dimension-Weighted Manhattan Heuristic                   |
|  - Kinematic Turn Penalty State Expansion (x, y, heading)   |
|  - Nearest-Vehicle Fleet Dispatcher                         |
|  - Dynamic obstacle layer and directed topology              |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                       Map & Graph Model                     |
|  - xroute_map_t Grid & Directed Cell Adjacency Matrix           |
|  - Cell Types (Transit, Blocked Rack, Transit Rack, Obstacle)|
|  - Directional Constraints (XROUTE_DIR_LEFT/UP/RIGHT/DOWN)      |
|  - Dynamic Obstacle Layer                                   |
|  - Bay Index Lookup & Accessible Neighbors                  |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                     Parsers & Exporters                     |
|  - Native XML Reader & Writer (Magazzino / Deposito Schema) |
|  - Native CSV Matrix Reader                                 |
|  - CSV -> XML Converter                                     |
|  - ASCII Terminal Renderer                                  |
|  - SVG Vector Graphic Exporter                              |
+-------------------------------------------------------------+
```

---

## 3. Data Structures & Representation

### Map Model (`xroute_map_t`)
- Linear flat arrays for $O(1)$ memory locality:
  - `types[y * cols + x]`: `xroute_cell_type_t` enum.
  - `allowed_moves[y * cols + x]`: 8-bit bitmask of `XROUTE_DIR_*` movement permissions.
  - `dynamic_obstacles[y * cols + x]`: boolean flag for temporary hazards or traffic.
  - `bays[]`: array of structured racks with `(corsia, campata, accessible_sides)`.
  - `cell_to_bay[]`: inverse lookup table from cell index to bay structure index.

### Coordinate Systems
- **Internal Representation**: 0-based coordinates $(x \in [0, \text{cols}-1], y \in [0, \text{rows}-1])$.
- **Linear Node Index**: $\text{node\_id} = y \times \text{cols} + x$.
- Coordinates are exposed consistently as zero-based `(x, y)` pairs throughout the public API.

---

## 4. Algorithmic Implementation

### A* Pathfinding Engine
- **Open Set**: Array-backed binary min-heap with $O(\log V)$ insertions and extractions.
- **Metric Heuristic**:
  $$h(x, y) = |x_{\text{goal}} - x| \cdot \text{cell\_width\_cm} + |y_{\text{goal}} - y| \cdot \text{cell\_height\_cm}$$
  Since cell dimensions are strictly positive and the grid graph only allows orthogonal steps of lengths `cell_width_cm` or `cell_height_cm`, this heuristic is admissible and consistent, guaranteeing optimal shortest paths without overestimating.

### Kinematic Turn Penalty Modeling
When `opts.turn_penalty_cost > 0`:
- The search state is expanded from $(u)$ to $(u, d)$ where $d \in \{1: \text{LEFT}, 2: \text{UP}, 3: \text{RIGHT}, 4: \text{DOWN}\}$.
- If moving to adjacent node $v$ requires a direction $d_{\text{new}} \neq d_{\text{curr}}$ (and $d_{\text{curr}} \neq 0$), an additional cost penalty `turn_penalty_cost` is added to $g(v)$.
- This encourages the vehicle to stay in straight corridors rather than zig-zagging, reflecting real-world industrial AGV steering overhead.

### Fleet Dispatcher (`xroute_router_find_nearest_operator`)
- Input: Target rack `(corsia, campata)` and $K$ active forklifts.
- Execution:
  1. Identifies all accessible neighbor cells around the rack.
  2. Evaluates shortest reachable trajectories for each vehicle.
  3. Returns the closest vehicle, the optimal loading/unloading neighbor cell, the total distance in meters, and the computed route.

---

## 5. Performance Benchmark

Performance is workload- and machine-dependent. Run `make benchmark` to measure the bundled map. The benchmark records the map, query count, success count, median, p95, and mean throughput. It uses a deterministic random seed and includes route allocation and release in each sample. Record hardware, OS, compiler, flags, map topology, and methodology with any published result.

The library deliberately does not build or ship all-pairs distance matrices. Routes are calculated on demand, so dynamic obstacles and directed aisle changes are reflected in the next query.
