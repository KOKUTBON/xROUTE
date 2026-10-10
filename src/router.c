/**
 * @file router.c
 * @brief Routing engine: A* shortest paths, fleet dispatch, and turn penalties.
 */

#include "xroute/router.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define INF_COST (INT_MAX / 4)

struct xroute_router_s {
  const xroute_map_t *map;
};

xroute_options_t xroute_default_options(void) {
  xroute_options_t opts;
  opts.turn_penalty_cost = 0; /* No extra penalty by default */
  opts.speed_mps =
      1.5; /* Standard forklift cruising speed: 1.5 m/s (~5.4 km/h) */
  opts.turn_time_sec = 2.0; /* 2 seconds per 90-degree turn */
  opts.allow_diagonal = false;
  return opts;
}

xroute_router_t *xroute_router_create(const xroute_map_t *map) {
  if (!map)
    return NULL;
  xroute_router_t *router = (xroute_router_t *)malloc(sizeof(xroute_router_t));
  if (!router)
    return NULL;
  router->map = map;
  return router;
}

void xroute_router_free(xroute_router_t *router) { free(router); }

void xroute_route_free(xroute_route_t *route) {
  if (!route)
    return;
  free(route->points);
  free(route);
}

void xroute_fleet_result_free(xroute_fleet_result_t *result) {
  if (!result)
    return;
  if (result->route) {
    xroute_route_free(result->route);
    result->route = NULL;
  }
}

/* Binary min-heap priority queue for A*. */
typedef struct {
  int node_id;
  int dir; /* 0: initial/any, 1: LEFT, 2: UP, 3: RIGHT, 4: DOWN */
  int f_score;
} pq_element_t;

typedef struct {
  pq_element_t *elements;
  int size;
  int capacity;
} min_heap_t;

static min_heap_t *heap_create(int capacity) {
  if (capacity <= 0)
    return NULL;
  min_heap_t *h = (min_heap_t *)malloc(sizeof(min_heap_t));
  if (!h)
    return NULL;
  h->elements = (pq_element_t *)malloc(capacity * sizeof(pq_element_t));
  if (!h->elements) {
    free(h);
    return NULL;
  }
  h->size = 0;
  h->capacity = capacity;
  return h;
}

static void heap_free(min_heap_t *h) {
  if (!h)
    return;
  free(h->elements);
  free(h);
}

static bool heap_push(min_heap_t *h, int node_id, int dir, int f_score) {
  if (h->size >= h->capacity) {
    if (h->capacity > INT_MAX / 2)
      return false;
    h->capacity *= 2;
    pq_element_t *new_elements = (pq_element_t *)realloc(
        h->elements, (size_t)h->capacity * sizeof(pq_element_t));
    if (!new_elements)
      return false;
    h->elements = new_elements;
  }
  int i = h->size++;
  while (i > 0) {
    int parent = (i - 1) / 2;
    if (h->elements[parent].f_score <= f_score)
      break;
    h->elements[i] = h->elements[parent];
    i = parent;
  }
  h->elements[i].node_id = node_id;
  h->elements[i].dir = dir;
  h->elements[i].f_score = f_score;
  return true;
}

static bool heap_pop(min_heap_t *h, int *out_node, int *out_dir,
                     int *out_score) {
  if (h->size == 0)
    return false;
  *out_node = h->elements[0].node_id;
  *out_dir = h->elements[0].dir;
  if (out_score)
    *out_score = h->elements[0].f_score;

  pq_element_t last = h->elements[--h->size];
  if (h->size == 0)
    return true;

  int i = 0;
  while (2 * i + 1 < h->size) {
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    int smallest = (right < h->size &&
                    h->elements[right].f_score < h->elements[left].f_score)
                       ? right
                       : left;
    if (h->elements[smallest].f_score >= last.f_score)
      break;
    h->elements[i] = h->elements[smallest];
    i = smallest;
  }
  h->elements[i] = last;
  return true;
}

/* Admissible Manhattan heuristic scaled by cell dimensions */
static inline int heuristic(int x1, int y1, int x2, int y2, int w_cm,
                            int h_cm) {
  return abs(x2 - x1) * w_cm + abs(y2 - y1) * h_cm;
}

xroute_status_t xroute_router_find_path(const xroute_router_t *router,
                                        xroute_point_t start,
                                        xroute_point_t goal,
                                        const xroute_options_t *user_opts,
                                        xroute_route_t **out_route) {
  if (!router || !router->map || !out_route)
    return XROUTE_ERR_INVALID_PARAM;
  *out_route = NULL;

  const xroute_map_t *map = router->map;
  int cols = xroute_map_get_cols(map);
  int rows = xroute_map_get_rows(map);
  int w_cm = xroute_map_get_cell_width_cm(map);
  int h_cm = xroute_map_get_cell_height_cm(map);

  if (start.x < 0 || start.x >= cols || start.y < 0 || start.y >= rows ||
      goal.x < 0 || goal.x >= cols || goal.y < 0 || goal.y >= rows) {
    return XROUTE_ERR_OUT_OF_BOUNDS;
  }

  if (!xroute_map_is_traversable(map, start.x, start.y) ||
      !xroute_map_is_traversable(map, goal.x, goal.y)) {
    return XROUTE_ERR_NO_PATH;
  }

  xroute_options_t opts = user_opts ? *user_opts : xroute_default_options();
  if (opts.turn_penalty_cost < 0 || !isfinite(opts.speed_mps) ||
      !isfinite(opts.turn_time_sec) || opts.turn_time_sec < 0.0 ||
      opts.allow_diagonal) {
    return XROUTE_ERR_INVALID_PARAM;
  }

  int start_node = xroute_map_xy_to_node(start.x, start.y, cols);
  int goal_node = xroute_map_xy_to_node(goal.x, goal.y, cols);

  if (start_node == goal_node) {
    xroute_route_t *r = (xroute_route_t *)calloc(1, sizeof(xroute_route_t));
    if (!r)
      return XROUTE_ERR_NO_MEMORY;
    r->points = (xroute_point_t *)malloc(sizeof(xroute_point_t));
    r->points[0] = start;
    r->length = 1;
    r->total_cost = 0;
    r->distance_meters = 0.0;
    r->estimated_time_sec = 0.0;
    r->turns_count = 0;
    *out_route = r;
    return XROUTE_SUCCESS;
  }

  /*
   * If turn_penalty_cost > 0, state space is (node, dir) where dir in 0..4.
   * Otherwise, state space is simply (node).
   */
  int num_dirs = (opts.turn_penalty_cost > 0) ? 5 : 1;
  int num_states = rows * cols * num_dirs;

  int *g_cost = (int *)malloc(num_states * sizeof(int));
  int *parent_node = (int *)malloc(num_states * sizeof(int));
  int *parent_dir = (int *)malloc(num_states * sizeof(int));

  if (!g_cost || !parent_node || !parent_dir) {
    free(g_cost);
    free(parent_node);
    free(parent_dir);
    return XROUTE_ERR_NO_MEMORY;
  }

  for (int i = 0; i < num_states; ++i) {
    g_cost[i] = INF_COST;
    parent_node[i] = -1;
    parent_dir[i] = -1;
  }

  min_heap_t *heap = heap_create(1024);
  if (!heap) {
    free(g_cost);
    free(parent_node);
    free(parent_dir);
    return XROUTE_ERR_NO_MEMORY;
  }

  int start_state = start_node * num_dirs + 0;
  g_cost[start_state] = 0;
  int h_start = heuristic(start.x, start.y, goal.x, goal.y, w_cm, h_cm);
  if (!heap_push(heap, start_node, 0, h_start)) {
    heap_free(heap);
    free(g_cost);
    free(parent_node);
    free(parent_dir);
    return XROUTE_ERR_NO_MEMORY;
  }

  static const int delta[4][2] = {
      {-1, 0}, /* 1: LEFT */
      {0, -1}, /* 2: UP */
      {1, 0},  /* 3: RIGHT */
      {0, 1}   /* 4: DOWN */
  };

  int best_end_dir = -1;
  int best_end_cost = INF_COST;

  while (heap->size > 0) {
    int u_node, u_dir, u_score;
    heap_pop(heap, &u_node, &u_dir, &u_score);

    int u_state = u_node * num_dirs + u_dir;
    int current_g = g_cost[u_state];

    if (u_node == goal_node) {
      best_end_dir = u_dir;
      best_end_cost = current_g;
      break;
    }

    int ux, uy;
    xroute_map_node_to_xy(u_node, cols, &ux, &uy);

    for (int k = 0; k < 4; ++k) {
      int vx = ux + delta[k][0];
      int vy = uy + delta[k][1];
      int step_c = xroute_map_step_cost(map, ux, uy, vx, vy);
      if (step_c < 0)
        continue;

      int move_dir = k + 1;
      int turn_penalty = 0;
      if (num_dirs > 1 && u_dir != 0 && u_dir != move_dir) {
        turn_penalty = opts.turn_penalty_cost;
      }

      int new_g = current_g + step_c + turn_penalty;
      int v_node = xroute_map_xy_to_node(vx, vy, cols);
      int v_dir = (num_dirs > 1) ? move_dir : 0;
      int v_state = v_node * num_dirs + v_dir;

      if (new_g < g_cost[v_state]) {
        g_cost[v_state] = new_g;
        parent_node[v_state] = u_node;
        parent_dir[v_state] = u_dir;
        int h = heuristic(vx, vy, goal.x, goal.y, w_cm, h_cm);
        if (!heap_push(heap, v_node, v_dir, new_g + h)) {
          heap_free(heap);
          free(g_cost);
          free(parent_node);
          free(parent_dir);
          return XROUTE_ERR_NO_MEMORY;
        }
      }
    }
  }

  heap_free(heap);

  if (best_end_dir == -1) {
    free(g_cost);
    free(parent_node);
    free(parent_dir);
    return XROUTE_ERR_NO_PATH;
  }

  /* Reconstruct path */
  int capacity = 128;
  xroute_point_t *temp_pts =
      (xroute_point_t *)malloc(capacity * sizeof(xroute_point_t));
  if (!temp_pts) {
    free(g_cost);
    free(parent_node);
    free(parent_dir);
    return XROUTE_ERR_NO_MEMORY;
  }
  int path_len = 0;

  int curr_node = goal_node;
  int curr_dir = best_end_dir;

  while (curr_node != -1) {
    if (path_len >= capacity) {
      if (capacity > INT_MAX / 2) {
        free(temp_pts);
        free(g_cost);
        free(parent_node);
        free(parent_dir);
        return XROUTE_ERR_NO_MEMORY;
      }
      capacity *= 2;
      xroute_point_t *new_pts = (xroute_point_t *)realloc(
          temp_pts, (size_t)capacity * sizeof(xroute_point_t));
      if (!new_pts) {
        free(temp_pts);
        free(g_cost);
        free(parent_node);
        free(parent_dir);
        return XROUTE_ERR_NO_MEMORY;
      }
      temp_pts = new_pts;
    }
    xroute_map_node_to_xy(curr_node, cols, &temp_pts[path_len].x,
                          &temp_pts[path_len].y);
    path_len++;

    int curr_state = curr_node * num_dirs + curr_dir;
    int p_node = parent_node[curr_state];
    int p_dir = parent_dir[curr_state];

    curr_node = p_node;
    curr_dir = p_dir;
  }

  free(g_cost);
  free(parent_node);
  free(parent_dir);

  /* Allocate and populate route (reverse order) */
  xroute_route_t *route = (xroute_route_t *)calloc(1, sizeof(xroute_route_t));
  if (!route) {
    free(temp_pts);
    return XROUTE_ERR_NO_MEMORY;
  }
  route->points = (xroute_point_t *)malloc(path_len * sizeof(xroute_point_t));
  route->length = path_len;
  route->total_cost = best_end_cost;

  for (int i = 0; i < path_len; ++i) {
    route->points[i] = temp_pts[path_len - 1 - i];
  }
  free(temp_pts);

  /* Count turns and compute metrics */
  int turns = 0;
  for (int i = 1; i < path_len - 1; ++i) {
    int dx1 = route->points[i].x - route->points[i - 1].x;
    int dy1 = route->points[i].y - route->points[i - 1].y;
    int dx2 = route->points[i + 1].x - route->points[i].x;
    int dy2 = route->points[i + 1].y - route->points[i].y;
    if (dx1 != dx2 || dy1 != dy2) {
      turns++;
    }
  }
  route->turns_count = turns;

  /* Total physical distance in meters (excluding virtual turn penalty costs) */
  double dist_m = 0.0;
  for (int i = 0; i < path_len - 1; ++i) {
    int dx = abs(route->points[i + 1].x - route->points[i].x);
    int dy = abs(route->points[i + 1].y - route->points[i].y);
    dist_m += (dx * w_cm + dy * h_cm) / 100.0;
  }
  route->distance_meters = dist_m;

  double speed = (opts.speed_mps > 0.1) ? opts.speed_mps : 1.5;
  route->estimated_time_sec = (dist_m / speed) + (turns * opts.turn_time_sec);

  *out_route = route;
  return XROUTE_SUCCESS;
}

xroute_status_t xroute_router_route_to_bay(const xroute_router_t *router,
                                           xroute_point_t start, int corsia,
                                           int campata,
                                           const xroute_options_t *opts,
                                           xroute_route_t **out_route) {
  if (!router || !router->map || !out_route)
    return XROUTE_ERR_INVALID_PARAM;
  *out_route = NULL;

  const xroute_bay_t *bay = xroute_map_find_bay(router->map, corsia, campata);
  if (!bay)
    return XROUTE_ERR_BAY_NOT_FOUND;

  xroute_point_t access_pts[4];
  int num_pts = xroute_map_get_bay_access_points(router->map, bay, access_pts);
  if (num_pts == 0) {
    return XROUTE_ERR_NO_PATH;
  }

  xroute_route_t *best_route = NULL;
  int best_cost = INF_COST;

  for (int i = 0; i < num_pts; ++i) {
    xroute_route_t *candidate = NULL;
    xroute_status_t st =
        xroute_router_find_path(router, start, access_pts[i], opts, &candidate);
    if (st == XROUTE_SUCCESS && candidate) {
      if (candidate->total_cost < best_cost) {
        if (best_route)
          xroute_route_free(best_route);
        best_route = candidate;
        best_cost = candidate->total_cost;
      } else {
        xroute_route_free(candidate);
      }
    }
  }

  if (!best_route) {
    return XROUTE_ERR_NO_PATH;
  }

  *out_route = best_route;
  return XROUTE_SUCCESS;
}

xroute_status_t xroute_router_find_nearest_operator(
    const xroute_router_t *router, const xroute_operator_t *operators,
    int num_operators, int corsia, int campata, const xroute_options_t *opts,
    xroute_fleet_result_t *out_result) {
  if (!router || !operators || num_operators <= 0 || !out_result) {
    return XROUTE_ERR_INVALID_PARAM;
  }

  memset(out_result, 0, sizeof(xroute_fleet_result_t));
  out_result->best_operator_index = -1;
  out_result->distance_cost = INF_COST;

  const xroute_bay_t *bay = xroute_map_find_bay(router->map, corsia, campata);
  if (!bay)
    return XROUTE_ERR_BAY_NOT_FOUND;

  xroute_point_t access_pts[4];
  int num_pts = xroute_map_get_bay_access_points(router->map, bay, access_pts);
  if (num_pts == 0)
    return XROUTE_ERR_NO_PATH;

  xroute_route_t *best_route = NULL;
  int best_op_idx = -1;
  xroute_point_t best_target_pt = {0, 0};

  for (int op = 0; op < num_operators; ++op) {
    xroute_point_t start = {operators[op].x, operators[op].y};

    for (int p = 0; p < num_pts; ++p) {
      xroute_route_t *candidate = NULL;
      xroute_status_t st = xroute_router_find_path(router, start, access_pts[p],
                                                   opts, &candidate);
      if (st == XROUTE_SUCCESS && candidate) {
        if (candidate->total_cost < out_result->distance_cost) {
          if (best_route)
            xroute_route_free(best_route);
          best_route = candidate;
          out_result->distance_cost = candidate->total_cost;
          out_result->distance_meters = candidate->distance_meters;
          best_op_idx = op;
          best_target_pt = access_pts[p];
        } else {
          xroute_route_free(candidate);
        }
      }
    }
  }

  if (best_op_idx == -1 || !best_route) {
    return XROUTE_ERR_NO_PATH;
  }

  out_result->best_operator_index = best_op_idx;
  out_result->best_operator_id = operators[best_op_idx].id;
  out_result->start_point.x = operators[best_op_idx].x;
  out_result->start_point.y = operators[best_op_idx].y;
  out_result->target_cell = best_target_pt;
  out_result->route = best_route;

  return XROUTE_SUCCESS;
}
