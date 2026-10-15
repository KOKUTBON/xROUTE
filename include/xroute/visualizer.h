/**
 * @file visualizer.h
 * @brief Terminal ASCII rendering, path overlays, and SVG export.
 */

#ifndef XROUTE_VISUALIZER_H
#define XROUTE_VISUALIZER_H

#include "map.h"
#include "types.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Print map layout to stream with coordinates.
 * @param map The warehouse map.
 * @param stream Output file stream (e.g., stdout).
 */
xroute_status_t xroute_visualizer_print_map(const xroute_map_t *map,
                                            FILE *stream);

/**
 * @brief Print map with overlaid route path using ANSI characters.
 * @param map The warehouse map.
 * @param route The route to overlay.
 * @param stream Output file stream.
 */
xroute_status_t xroute_visualizer_print_route(const xroute_map_t *map,
                                              const xroute_route_t *route,
                                              FILE *stream);

/**
 * @brief Generate ASCII string representation of map with optional route.
 * @param map The warehouse map.
 * @param route Optional route to overlay (pass NULL if none).
 * @param out_str Pointer to receive allocated string. Caller must free() it.
 */
xroute_status_t xroute_visualizer_render_ascii(const xroute_map_t *map,
                                               const xroute_route_t *route,
                                               char **out_str);

/**
 * @brief Export warehouse map and route as vector SVG graphics file.
 * @param map The warehouse map.
 * @param route Optional route to overlay.
 * @param filepath Output SVG file path.
 */
xroute_status_t xroute_visualizer_export_svg(const xroute_map_t *map,
                                             const xroute_route_t *route,
                                             const char *filepath);

#ifdef __cplusplus
}
#endif

#endif /* XROUTE_VISUALIZER_H */
