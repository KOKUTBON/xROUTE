/**
 * @file map.c
 * @brief Implementation of warehouse map data structure and topology
 * operations.
 */

#include "xroute/map.h"
#include <stdlib.h>
#include <string.h>

struct xroute_map_s {
  int rows;
  int cols;
  int cell_width_cm;
  int cell_height_cm;

  xroute_cell_type_t *types;
  uint8_t *allowed_moves;
  bool *dynamic_obstacles;

  xroute_bay_t *bays;
  int bay_count;
  int bay_capacity;
  int *
      cell_to_bay; /**< Maps linear node_id to bay index in bays array, or -1 */
};

int xroute_map_xy_to_node(int x, int y, int cols) { return y * cols + x; }

void xroute_map_node_to_xy(int node_id, int cols, int *out_x, int *out_y) {
  if (cols <= 0)
    return;
  if (out_x)
    *out_x = node_id % cols;
  if (out_y)
    *out_y = node_id / cols;
}

xroute_map_t *xroute_map_create(int rows, int cols, int cell_width_cm,
                                int cell_height_cm) {
  if (rows <= 0 || cols <= 0)
    return NULL;
  if ((size_t)rows > SIZE_MAX / (size_t)cols)
    return NULL;
  if (cell_width_cm <= 0)
    cell_width_cm = 100;
  if (cell_height_cm <= 0)
    cell_height_cm = 100;

  xroute_map_t *map = (xroute_map_t *)calloc(1, sizeof(xroute_map_t));
  if (!map)
    return NULL;

  map->rows = rows;
  map->cols = cols;
  map->cell_width_cm = cell_width_cm;
  map->cell_height_cm = cell_height_cm;

  int total_cells = rows * cols;
  map->types =
      (xroute_cell_type_t *)calloc(total_cells, sizeof(xroute_cell_type_t));
  map->allowed_moves = (uint8_t *)malloc(total_cells * sizeof(uint8_t));
  map->dynamic_obstacles = (bool *)calloc(total_cells, sizeof(bool));
  map->cell_to_bay = (int *)malloc(total_cells * sizeof(int));

  if (!map->types || !map->allowed_moves || !map->dynamic_obstacles ||
      !map->cell_to_bay) {
    xroute_map_free(map);
    return NULL;
  }

  /* By default, all cells are standard transit corridors with all 4 directions
   * allowed */
  for (int i = 0; i < total_cells; ++i) {
    map->types[i] = XROUTE_CELL_TRANSIT;
    map->allowed_moves[i] = XROUTE_DIR_ALL;
    map->cell_to_bay[i] = -1;
  }

  map->bay_capacity = 32;
  map->bays = (xroute_bay_t *)malloc(map->bay_capacity * sizeof(xroute_bay_t));
  if (!map->bays) {
    xroute_map_free(map);
    return NULL;
  }
  map->bay_count = 0;

  return map;
}

void xroute_map_free(xroute_map_t *map) {
  if (!map)
    return;
  free(map->types);
  free(map->allowed_moves);
  free(map->dynamic_obstacles);
  free(map->cell_to_bay);
  free(map->bays);
  free(map);
}

xroute_map_t *xroute_map_clone(const xroute_map_t *map) {
  if (!map)
    return NULL;
  xroute_map_t *clone = xroute_map_create(
      map->rows, map->cols, map->cell_width_cm, map->cell_height_cm);
  if (!clone)
    return NULL;

  int total_cells = map->rows * map->cols;
  memcpy(clone->types, map->types, total_cells * sizeof(xroute_cell_type_t));
  memcpy(clone->allowed_moves, map->allowed_moves,
         total_cells * sizeof(uint8_t));
  memcpy(clone->dynamic_obstacles, map->dynamic_obstacles,
         total_cells * sizeof(bool));
  memcpy(clone->cell_to_bay, map->cell_to_bay, total_cells * sizeof(int));

  if (map->bay_count > clone->bay_capacity) {
    xroute_bay_t *new_bays = (xroute_bay_t *)realloc(
        clone->bays, map->bay_count * sizeof(xroute_bay_t));
    if (!new_bays) {
      xroute_map_free(clone);
      return NULL;
    }
    clone->bays = new_bays;
    clone->bay_capacity = map->bay_count;
  }
  clone->bay_count = map->bay_count;
  memcpy(clone->bays, map->bays, map->bay_count * sizeof(xroute_bay_t));

  return clone;
}

int xroute_map_get_rows(const xroute_map_t *map) { return map ? map->rows : 0; }

int xroute_map_get_cols(const xroute_map_t *map) { return map ? map->cols : 0; }

int xroute_map_get_cell_width_cm(const xroute_map_t *map) {
  return map ? map->cell_width_cm : 0;
}

int xroute_map_get_cell_height_cm(const xroute_map_t *map) {
  return map ? map->cell_height_cm : 0;
}

xroute_status_t xroute_map_set_cell(xroute_map_t *map, int x, int y,
                                    xroute_cell_type_t type,
                                    uint8_t allowed_movements) {
  if (!map)
    return XROUTE_ERR_INVALID_PARAM;
  if (x < 0 || x >= map->cols || y < 0 || y >= map->rows)
    return XROUTE_ERR_OUT_OF_BOUNDS;

  int idx = xroute_map_xy_to_node(x, y, map->cols);
  map->types[idx] = type;
  map->allowed_moves[idx] = allowed_movements;
  return XROUTE_SUCCESS;
}

xroute_cell_type_t xroute_map_get_cell_type(const xroute_map_t *map, int x,
                                            int y) {
  if (!map || x < 0 || x >= map->cols || y < 0 || y >= map->rows)
    return XROUTE_CELL_OBSTACLE;
  int idx = xroute_map_xy_to_node(x, y, map->cols);
  return map->types[idx];
}

uint8_t xroute_map_get_allowed_movements(const xroute_map_t *map, int x,
                                         int y) {
  if (!map || x < 0 || x >= map->cols || y < 0 || y >= map->rows)
    return XROUTE_DIR_NONE;
  int idx = xroute_map_xy_to_node(x, y, map->cols);
  return map->allowed_moves[idx];
}

xroute_status_t xroute_map_add_bay(xroute_map_t *map, int x, int y,
                                   xroute_cell_type_t type, int corsia,
                                   int campata, uint8_t accessible_sides,
                                   uint8_t allowed_movements) {
  if (!map)
    return XROUTE_ERR_INVALID_PARAM;
  if (x < 0 || x >= map->cols || y < 0 || y >= map->rows)
    return XROUTE_ERR_OUT_OF_BOUNDS;

  int idx = xroute_map_xy_to_node(x, y, map->cols);
  map->types[idx] = type;
  map->allowed_moves[idx] = allowed_movements;

  /* Check if bay already exists at (x, y) */
  int existing_bay_idx = map->cell_to_bay[idx];
  if (existing_bay_idx >= 0 && existing_bay_idx < map->bay_count) {
    map->bays[existing_bay_idx].corsia = corsia;
    map->bays[existing_bay_idx].campata = campata;
    map->bays[existing_bay_idx].type = type;
    map->bays[existing_bay_idx].accessible_sides = accessible_sides;
    map->bays[existing_bay_idx].allowed_movements = allowed_movements;
    return XROUTE_SUCCESS;
  }

  if (map->bay_count >= map->bay_capacity) {
    int new_cap = map->bay_capacity * 2;
    xroute_bay_t *new_bays =
        (xroute_bay_t *)realloc(map->bays, new_cap * sizeof(xroute_bay_t));
    if (!new_bays)
      return XROUTE_ERR_NO_MEMORY;
    map->bays = new_bays;
    map->bay_capacity = new_cap;
  }

  int new_idx = map->bay_count++;
  map->bays[new_idx].x = x;
  map->bays[new_idx].y = y;
  map->bays[new_idx].corsia = corsia;
  map->bays[new_idx].campata = campata;
  map->bays[new_idx].type = type;
  map->bays[new_idx].accessible_sides = accessible_sides;
  map->bays[new_idx].allowed_movements = allowed_movements;

  map->cell_to_bay[idx] = new_idx;
  return XROUTE_SUCCESS;
}

const xroute_bay_t *xroute_map_find_bay(const xroute_map_t *map, int corsia,
                                        int campata) {
  if (!map)
    return NULL;
  for (int i = 0; i < map->bay_count; ++i) {
    if (map->bays[i].corsia == corsia && map->bays[i].campata == campata) {
      return &map->bays[i];
    }
  }
  return NULL;
}

const xroute_bay_t *xroute_map_get_all_bays(const xroute_map_t *map,
                                            int *out_count) {
  if (out_count)
    *out_count = map ? map->bay_count : 0;
  return map ? map->bays : NULL;
}

int xroute_map_get_bay_access_points(const xroute_map_t *map,
                                     const xroute_bay_t *bay,
                                     xroute_point_t out_points[4]) {
  if (!map || !bay || !out_points)
    return 0;
  int count = 0;

  /*
   * Side definitions according to Docs/MAP_FORMATS.md:
   * sx: left neighbor (x - 1, y)
   * up: top neighbor (x, y - 1)
   * dx: right neighbor (x + 1, y)
   * dw: bottom neighbor (x, y + 1)
   */
  static const struct {
    uint8_t flag;
    int dx;
    int dy;
  } sides[4] = {{XROUTE_DIR_LEFT, -1, 0},
                {XROUTE_DIR_UP, 0, -1},
                {XROUTE_DIR_RIGHT, 1, 0},
                {XROUTE_DIR_DOWN, 0, 1}};

  for (int i = 0; i < 4; ++i) {
    if (bay->accessible_sides & sides[i].flag) {
      int nx = bay->x + sides[i].dx;
      int ny = bay->y + sides[i].dy;
      if (nx >= 0 && nx < map->cols && ny >= 0 && ny < map->rows) {
        if (xroute_map_is_traversable(map, nx, ny)) {
          out_points[count].x = nx;
          out_points[count].y = ny;
          count++;
        }
      }
    }
  }

  return count;
}

xroute_status_t xroute_map_set_dynamic_obstacle(xroute_map_t *map, int x, int y,
                                                bool is_blocked) {
  if (!map)
    return XROUTE_ERR_INVALID_PARAM;
  if (x < 0 || x >= map->cols || y < 0 || y >= map->rows)
    return XROUTE_ERR_OUT_OF_BOUNDS;

  int idx = xroute_map_xy_to_node(x, y, map->cols);
  map->dynamic_obstacles[idx] = is_blocked;
  return XROUTE_SUCCESS;
}

bool xroute_map_is_traversable(const xroute_map_t *map, int x, int y) {
  if (!map || x < 0 || x >= map->cols || y < 0 || y >= map->rows)
    return false;
  int idx = xroute_map_xy_to_node(x, y, map->cols);

  if (map->dynamic_obstacles[idx])
    return false;

  xroute_cell_type_t t = map->types[idx];
  if (t == XROUTE_CELL_OBSTACLE || t == XROUTE_CELL_RACK_BLOCKED) {
    return false;
  }
  return true;
}

bool xroute_map_can_transit(const xroute_map_t *map, int from_x, int from_y,
                            int to_x, int to_y) {
  if (!xroute_map_is_traversable(map, from_x, from_y))
    return false;
  if (!xroute_map_is_traversable(map, to_x, to_y))
    return false;

  int dx = to_x - from_x;
  int dy = to_y - from_y;

  /* Only orthogonal movements supported for aisles */
  if ((dx == 0 && dy == 0) || (dx != 0 && dy != 0) || abs(dx) > 1 ||
      abs(dy) > 1) {
    return false;
  }

  int from_idx = xroute_map_xy_to_node(from_x, from_y, map->cols);
  uint8_t allowed = map->allowed_moves[from_idx];

  if (dx == -1 && !(allowed & XROUTE_DIR_LEFT))
    return false;
  if (dx == 1 && !(allowed & XROUTE_DIR_RIGHT))
    return false;
  if (dy == -1 && !(allowed & XROUTE_DIR_UP))
    return false;
  if (dy == 1 && !(allowed & XROUTE_DIR_DOWN))
    return false;

  return true;
}

int xroute_map_step_cost(const xroute_map_t *map, int from_x, int from_y,
                         int to_x, int to_y) {
  if (!xroute_map_can_transit(map, from_x, from_y, to_x, to_y)) {
    return -1;
  }
  int dx = abs(to_x - from_x);
  int dy = abs(to_y - from_y);

  if (dx == 1)
    return map->cell_width_cm;
  if (dy == 1)
    return map->cell_height_cm;
  return -1;
}
