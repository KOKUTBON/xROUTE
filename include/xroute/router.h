/**
 * @file router.h
 * @brief Routing engine (A* shortest paths and fleet dispatch).
 */

#ifndef XROUTE_ROUTER_H
#define XROUTE_ROUTER_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque router handle */
typedef struct xroute_router_s xroute_router_t;

/**
 * @brief Initialize a default options structure.
 */
xroute_options_t xroute_default_options(void);

/**
 * @brief Create a router instance for a given map.
 * @note The router references the map. The map must not be freed while the
 * router is in use.
 */
xroute_router_t *xroute_router_create(const xroute_map_t *map);

/**
 * @brief Free router instance.
 */
void xroute_router_free(xroute_router_t *router);

/**
 * @brief Find the optimal path between two arbitrary traversable cells using
 * A*.
 * @param router The router instance.
 * @param start Starting coordinates.
 * @param goal Target coordinates.
 * @param opts Routing options (turn penalty, speed, etc.). Pass NULL for
 * defaults.
 * @param out_route Pointer to receive the allocated route. Free with
 * xroute_route_free().
 * @return XROUTE_SUCCESS on success, error code otherwise.
 */
xroute_status_t xroute_router_find_path(const xroute_router_t *router,
                                        xroute_point_t start,
                                        xroute_point_t goal,
                                        const xroute_options_t *opts,
                                        xroute_route_t **out_route);

/**
 * @brief Find the optimal path from a start position to a warehouse rack
 * (corsia, campata). Automatically selects the closest accessible side cell of
 * the rack.
 * @param router The router instance.
 * @param start Forklift start coordinates.
 * @param corsia Target aisle.
 * @param campata Target bay.
 * @param opts Routing options. Pass NULL for defaults.
 * @param out_route Pointer to receive allocated route.
 * @return XROUTE_SUCCESS on success, error code otherwise.
 */
xroute_status_t xroute_router_route_to_bay(const xroute_router_t *router,
                                           xroute_point_t start, int corsia,
                                           int campata,
                                           const xroute_options_t *opts,
                                           xroute_route_t **out_route);

/**
 * @brief Find the nearest forklift operator from a fleet to a target rack.
 * @param router The router instance.
 * @param operators Array of candidate forklifts/operators.
 * @param num_operators Number of operators in array.
 * @param corsia Target aisle.
 * @param campata Target bay.
 * @param opts Routing options.
 * @param out_result Result structure populated with best operator index,
 * distance, and route.
 * @return XROUTE_SUCCESS on success, error code otherwise.
 */
xroute_status_t xroute_router_find_nearest_operator(
    const xroute_router_t *router, const xroute_operator_t *operators,
    int num_operators, int corsia, int campata, const xroute_options_t *opts,
    xroute_fleet_result_t *out_result);

/**
 * @brief Free an allocated route structure.
 */
void xroute_route_free(xroute_route_t *route);

/**
 * @brief Free an allocated fleet result structure and internal route.
 */
void xroute_fleet_result_free(xroute_fleet_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* XROUTE_ROUTER_H */
