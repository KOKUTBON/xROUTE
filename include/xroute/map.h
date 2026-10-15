/**
 * @file map.h
 * @brief Warehouse map representation, cell querying, and graph structure.
 */

#ifndef XROUTE_MAP_H
#define XROUTE_MAP_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to warehouse map */
typedef struct xroute_map_s xroute_map_t;

/**
 * @brief Create a new warehouse map.
 * @param rows Number of rows (Y dimension).
 * @param cols Number of columns (X dimension).
 * @param cell_width_cm Physical width of cell in cm (dimColonne).
 * @param cell_height_cm Physical height of cell in cm (dimRighe).
 * @return Pointer to newly allocated map, or NULL on error.
 */
xroute_map_t *xroute_map_create(int rows, int cols, int cell_width_cm,
                                int cell_height_cm);

/**
 * @brief Free a warehouse map and all its associated resources.
 */
void xroute_map_free(xroute_map_t *map);

/**
 * @brief Clone an existing warehouse map.
 */
xroute_map_t *xroute_map_clone(const xroute_map_t *map);

/**
 * @brief Get dimensions of the warehouse map.
 */
int xroute_map_get_rows(const xroute_map_t *map);
int xroute_map_get_cols(const xroute_map_t *map);
int xroute_map_get_cell_width_cm(const xroute_map_t *map);
int xroute_map_get_cell_height_cm(const xroute_map_t *map);

/**
 * @brief Convert (x, y) coordinates to linear node index.
 * @note Uses 0-based indexing: 0 <= x < cols, 0 <= y < rows.
 */
int xroute_map_xy_to_node(int x, int y, int cols);

/**
 * @brief Convert linear node index to (x, y) coordinates.
 */
void xroute_map_node_to_xy(int node_id, int cols, int *out_x, int *out_y);

/**
 * @brief Set cell type and allowed movements for a specific cell.
 */
xroute_status_t xroute_map_set_cell(xroute_map_t *map, int x, int y,
                                    xroute_cell_type_t type,
                                    uint8_t allowed_movements);

/**
 * @brief Get cell type and movement flags.
 */
xroute_cell_type_t xroute_map_get_cell_type(const xroute_map_t *map, int x,
                                            int y);
uint8_t xroute_map_get_allowed_movements(const xroute_map_t *map, int x, int y);

/**
 * @brief Add or update a warehouse rack/bay.
 */
xroute_status_t xroute_map_add_bay(xroute_map_t *map, int x, int y,
                                   xroute_cell_type_t type, int corsia,
                                   int campata, uint8_t accessible_sides,
                                   uint8_t allowed_movements);

/**
 * @brief Look up a bay by aisle and bay index (corsia, campata).
 * @return Pointer to bay information, or NULL if not found.
 */
const xroute_bay_t *xroute_map_find_bay(const xroute_map_t *map, int corsia,
                                        int campata);

/**
 * @brief Get the list of all bays in the warehouse.
 * @param out_count Pointer to store total number of bays.
 * @return Pointer to array of bays (internal storage, do not free).
 */
const xroute_bay_t *xroute_map_get_all_bays(const xroute_map_t *map,
                                            int *out_count);

/**
 * @brief Retrieve all accessible neighbor cells from which a bay can be
 * loaded/unloaded.
 * @param map The warehouse map.
 * @param bay Target bay.
 * @param out_points Array of size at least 4 to store accessible neighbor
 * points.
 * @return Number of valid accessible neighbor points found (0 to 4).
 */
int xroute_map_get_bay_access_points(const xroute_map_t *map,
                                     const xroute_bay_t *bay,
                                     xroute_point_t out_points[4]);

/**
 * @brief Set or clear a temporary dynamic obstacle on a cell.
 * @param is_blocked true to mark cell as impassable, false to restore normal
 * status.
 */
xroute_status_t xroute_map_set_dynamic_obstacle(xroute_map_t *map, int x, int y,
                                                bool is_blocked);

/**
 * @brief Check if a cell is traversable.
 */
bool xroute_map_is_traversable(const xroute_map_t *map, int x, int y);

/**
 * @brief Check if movement is allowed directly from (from_x, from_y) to (to_x,
 * to_y).
 */
bool xroute_map_can_transit(const xroute_map_t *map, int from_x, int from_y,
                            int to_x, int to_y);

/**
 * @brief Get step cost in centimeters between two adjacent cells.
 * @return Cost in cm, or -1 if movement is invalid or blocked.
 */
int xroute_map_step_cost(const xroute_map_t *map, int from_x, int from_y,
                         int to_x, int to_y);

#ifdef __cplusplus
}
#endif

#endif /* XROUTE_MAP_H */
