# Routing Algorithms & Kinematic State Space: `xROUTE`

This document provides formal mathematical and algorithmic specifications of the A* search space, kinematic turn penalties, memory complexity, and empirical topology comparisons in `xROUTE`.

---

## 1. Mathematical Formulation: 3D State Space $(x, y, \text{heading})$

In standard planar graph search, a node is represented purely by its two-dimensional spatial position $u = (x, y)$. However, physical material handling vehicles (e.g., automated forklifts and Automated Guided Vehicles / AGVs) experience significant kinematic deceleration, pivot delays, and mechanical wear during 90-degree and 180-degree turns.

To model these physical costs without post-hoc smoothing, `xROUTE` expands the search graph into a **three-dimensional state space**:

$$S = V \times D$$

where:
- $V = \{(x, y) \mid 0 \le x < \text{cols}, 0 \le y < \text{rows}\}$ is the spatial grid of cell coordinates.
- $D = \{0, 1, 2, 3, 4\}$ is the discrete vehicle orientation:
  - $d = 0$: Initial unoriented / stationary state (at the query start cell).
  - $d = 1$: Heading **West** / Left ($\Delta x = -1, \Delta y = 0$).
  - $d = 2$: Heading **North** / Up ($\Delta x = 0, \Delta y = -1$).
  - $d = 3$: Heading **East** / Right ($\Delta x = +1, \Delta y = 0$).
  - $d = 4$: Heading **South** / Down ($\Delta x = 0, \Delta y = +1$).

### State Transitions and Edge Costs

Let current state be $s = (u, d)$ and candidate next state be $s' = (v, d')$, where $v$ is an orthogonal neighbor of $u$ reached by moving in direction $d' \in \{1, 2, 3, 4\}$.

The transition cost $c(s, s')$ is defined as:

$$c((u, d), (v, d')) = \text{step\_cost}(u, v) + \text{turn\_penalty}(d, d')$$

where:
$$\text{step\_cost}(u, v) = \begin{cases} \text{cell\_width\_cm} & \text{if moving horizontally } (d' \in \{1, 3\}) \\ \text{cell\_height\_cm} & \text{if moving vertically } (d' \in \{2, 4\}) \end{cases}$$

$$\text{turn\_penalty}(d, d') = \begin{cases} 0 & \text{if } d = 0 \text{ (initial step from stationary)} \\ 0 & \text{if } d = d' \text{ (continuing along current heading)} \\ \text{turn\_penalty\_cost} & \text{if } d \ne 0 \text{ and } d \ne d' \text{ (90° or 180° turn)} \end{cases}$$

### Admissibility & Consistency of Heuristic

`xROUTE` employs a dimension-weighted Manhattan distance heuristic:

$$h(x, y) = |x_{\text{goal}} - x| \cdot \text{cell\_width\_cm} + |y_{\text{goal}} - y| \cdot \text{cell\_height\_cm}$$

#### Admissibility Proof ($h(u) \le h^*(u)$)
For any cell $u = (x, y)$, the minimum physical distance to $(x_{\text{goal}}, y_{\text{goal}})$ requires at least $|x_{\text{goal}} - x|$ horizontal steps of length $\text{cell\_width\_cm}$ and $|y_{\text{goal}} - y|$ vertical steps of length $\text{cell\_height\_cm}$. Since turn penalties $\text{turn\_penalty\_cost} \ge 0$, adding turn penalties only increases the true cost $h^*(u) \ge h(u)$. At the goal node, $h(\text{goal}) = 0$. Therefore, $h$ never overestimates the true remaining cost.

#### Consistency / Monotonicity Proof ($h(u) \le c(u, v) + h(v)$)
For any valid transition from $u$ to orthogonal neighbor $v$:
- If moving horizontally by $\pm 1$: $|x_{\text{goal}} - x| \le |x_{\text{goal}} - (x \pm 1)| + 1$, hence $h(u) - h(v) \le \text{cell\_width\_cm} \le c(u, v)$.
- If moving vertically by $\pm 1$: $|y_{\text{goal}} - y| \le |y_{\text{goal}} - (y \pm 1)| + 1$, hence $h(u) - h(v) \le \text{cell\_height\_cm} \le c(u, v)$.

Because $h$ is monotone and consistent, A* never needs to re-open closed states, guaranteeing optimal paths in minimum expansions.

---

## 2. Memory Footprint Formula per Grid Cell

`xROUTE` is designed for low-latency embedded and high-throughput dispatch workloads. Memory allocations occur within a single contiguous block per query rather than fragmented node pointers.

### Per-Query State Space Tables

During a search over $N = \text{rows} \times \text{cols}$ cells:

| Configuration | States per Cell ($\text{num\_dirs}$) | Search Arrays Allocated | Bytes per Cell |
| :--- | :---: | :--- | :---: |
| **Standard 2D** (`turn_penalty_cost == 0`) | $1$ | `g_cost` (4B) + `parent_node` (4B) + `parent_dir` (4B) | **12 B / cell** |
| **Kinematic 3D** (`turn_penalty_cost > 0`) | $5$ | `5 * [g_cost (4B) + parent_node (4B) + parent_dir (4B)]` | **60 B / cell** |

#### Formal Memory Formula

$$\text{Memory}_{\text{query}} = (\text{rows} \times \text{cols} \times \text{num\_dirs} \times 12 \text{ bytes}) + \text{Heap}_{\text{capacity}} \times 12 \text{ bytes}$$

Where:
- $\text{num\_dirs} = 5$ if $\text{turn\_penalty\_cost} > 0$, else $1$.
- `pq_element_t` is 12 bytes (`int node_id`, `int dir`, `int f_score`).
- Initial heap capacity is 1,024 elements ($12.288\text{ KB}$), expanding dynamically by $2\times$ up to $|S|$ only if the priority queue fills.

### Footprint Across Warehouse Scales

| Scale | Grid Dimensions | Cell Count ($N$) | 2D Footprint | 3D Kinematic Footprint |
| :--- | :--- | :--- | :--- | :--- |
| **Small Depot** | $50 \times 50$ | 2,500 | 30 KB | 150 KB |
| **Distribution Hub** | $200 \times 200$ | 40,000 | 480 KB | 2.40 MB |
| **Regional Logistics** | $500 \times 500$ | 250,000 | 3.00 MB | 15.00 MB |
| **Mega-Fulfillment** | $1,000 \times 1,000$ | 1,000,000 | 12.00 MB | 60.00 MB |

---

## 3. Dense vs. Sparse Aisle Grid Topology Comparisons

Warehouse floor plans exhibit distinct geometric characteristics that affect search tree expansion:

```
Dense Aisle Layout (Narrow Corridors)        Sparse / Open Floor Layout
+---+---+---+---+---+---+---+---+            +---+---+---+---+---+---+---+---+
| . | # | # | . | # | # | . | . |            | . | . | . | . | . | . | . | . |
| . | # | # | . | # | # | . | . |            | . | . | . | . | . | . | . | . |
| . | # | # | . | # | # | . | . |            | . | . | . | # | # | . | . | . |
| . | . | . | . | . | . | . | . |            | . | . | . | # | # | . | . | . |
+---+---+---+---+---+---+---+---+            +---+---+---+---+---+---+---+---+
```

### Characteristics Comparison

| Characteristic | Dense Aisle Layout | Sparse / Open Floor Layout |
| :--- | :--- | :--- |
| **Obstacle Density** | 50% – 75% (racks, shelves, walls) | 10% – 25% (staging bays, column pillars) |
| **Effective Branching Factor ($b$)** | $1.1 - 1.8$ (guided along corridors) | $2.8 - 4.0$ (open 4-way choices) |
| **Search Tree Morphology** | Narrow, canalized channel | Expanding convex wavefront |
| **Turn Penalty Influence** | Strongly penalizes crossing between bays; locks AGVs into main thoroughfares | Prunes jagged zig-zag paths; steers search beam directly along orientation axes |
| **Node Expansion Count** | Low (non-traversable cells pruned immediately) | Moderate to high (wide search frontier) |
| **Empirical Query Latency** | **~5 – 25 µs** | **~30 – 120 µs** |

### Algorithmic Observations

1. **Canalization in Dense Aisles**: In dense layouts, the graph connectivity naturally mirrors tree-like or ladder-like corridors. Early rejection of non-traversable cells (`step_cost < 0`) means the A* frontier advances strictly along aisle axes. Even with 3D kinematic expansion, the majority of candidate $(u, d)$ transitions are pruned before heap insertion.

2. **Tie-Breaking in Sparse Floors**: In open areas without turn penalties, Manhattan distance produces a large number of equivalent-cost paths with varying numbers of arbitrary turns, creating a broad diamond-shaped search frontier. With $\text{turn\_penalty\_cost} > 0$, any turn incurs a penalty, strictly favoring states that preserve current heading ($d = d'$). This acts as a natural search beam concentrator, drastically reducing unnecessary frontier expansion.
