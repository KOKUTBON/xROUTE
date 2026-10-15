/**
 * @file visualizer.c
 * @brief Terminal ASCII rendering, path overlays, and SVG vector graphic
 * export.
 */

#include "xroute/visualizer.h"
#include <stdlib.h>
#include <string.h>

static char get_cell_char(const xroute_map_t *map, int x, int y) {
  xroute_cell_type_t t = xroute_map_get_cell_type(map, x, y);
  switch (t) {
  case XROUTE_CELL_TRANSIT:
    return '-';
  case XROUTE_CELL_RACK_BLOCKED:
    return 'c';
  case XROUTE_CELL_RACK_TRANSIT:
    return 'T';
  case XROUTE_CELL_RESTRICTED:
    return 's';
  case XROUTE_CELL_OBSTACLE:
    return 'x';
  default:
    return '?';
  }
}

xroute_status_t xroute_visualizer_render_ascii(const xroute_map_t *map,
                                               const xroute_route_t *route,
                                               char **out_str) {
  if (!map || !out_str)
    return XROUTE_ERR_INVALID_PARAM;
  *out_str = NULL;

  int rows = xroute_map_get_rows(map);
  int cols = xroute_map_get_cols(map);

  /* Allocate 2D grid of chars */
  char *grid = (char *)malloc(rows * cols * sizeof(char));
  if (!grid)
    return XROUTE_ERR_NO_MEMORY;

  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      grid[y * cols + x] = get_cell_char(map, x, y);
    }
  }

  /* Overlay route if provided */
  if (route && route->points && route->length > 0) {
    for (int i = 0; i < route->length; ++i) {
      int rx = route->points[i].x;
      int ry = route->points[i].y;
      if (rx >= 0 && rx < cols && ry >= 0 && ry < rows) {
        grid[ry * cols + rx] = 'O';
      }
    }
    /* Mark Start and Goal */
    int sx = route->points[0].x;
    int sy = route->points[0].y;
    int gx = route->points[route->length - 1].x;
    int gy = route->points[route->length - 1].y;
    grid[sy * cols + sx] = 'S';
    grid[gy * cols + gx] = 'G';
  }

  /* Estimate string buffer size: header + each row + linebreaks */
  size_t line_len = 8 + cols * 2 + 2;
  size_t total_buf = (rows + 5) * line_len + 512;
  char *buf = (char *)malloc(total_buf);
  if (!buf) {
    free(grid);
    return XROUTE_ERR_NO_MEMORY;
  }

  size_t offset = 0;
  offset += snprintf(buf + offset, total_buf - offset, "    ");
  for (int x = 0; x < cols; ++x) {
    offset += snprintf(buf + offset, total_buf - offset, "%d ", x % 10);
  }
  offset += snprintf(buf + offset, total_buf - offset, "\n   +");
  for (int x = 0; x < cols; ++x) {
    offset += snprintf(buf + offset, total_buf - offset, "--");
  }
  offset += snprintf(buf + offset, total_buf - offset, "\n");

  for (int y = 0; y < rows; ++y) {
    offset += snprintf(buf + offset, total_buf - offset, "%2d |", y);
    for (int x = 0; x < cols; ++x) {
      offset +=
          snprintf(buf + offset, total_buf - offset, "%c ", grid[y * cols + x]);
    }
    offset += snprintf(buf + offset, total_buf - offset, "\n");
  }

  if (route) {
    offset += snprintf(
        buf + offset, total_buf - offset,
        "\nLegend: S=Start, G=Goal, O=Path, c=Rack, T=TransitRack, s=OneWay, "
        "x=Obstacle\n"
        "Length: %d steps | Distance: %.2f m | Time: %.1f s | Turns: %d\n",
        route->length, route->distance_meters, route->estimated_time_sec,
        route->turns_count);
  }

  free(grid);
  *out_str = buf;
  return XROUTE_SUCCESS;
}

xroute_status_t xroute_visualizer_print_map(const xroute_map_t *map,
                                            FILE *stream) {
  if (!map)
    return XROUTE_ERR_INVALID_PARAM;
  if (!stream)
    stream = stdout;

  char *str = NULL;
  xroute_status_t st = xroute_visualizer_render_ascii(map, NULL, &str);
  if (st == XROUTE_SUCCESS && str) {
    fputs(str, stream);
    free(str);
  }
  return st;
}

xroute_status_t xroute_visualizer_print_route(const xroute_map_t *map,
                                              const xroute_route_t *route,
                                              FILE *stream) {
  if (!map || !route)
    return XROUTE_ERR_INVALID_PARAM;
  if (!stream)
    stream = stdout;

  char *str = NULL;
  xroute_status_t st = xroute_visualizer_render_ascii(map, route, &str);
  if (st == XROUTE_SUCCESS && str) {
    fputs(str, stream);
    free(str);
  }
  return st;
}

xroute_status_t xroute_visualizer_export_svg(const xroute_map_t *map,
                                             const xroute_route_t *route,
                                             const char *filepath) {
  if (!map || !filepath)
    return XROUTE_ERR_INVALID_PARAM;

  FILE *fp = fopen(filepath, "w");
  if (!fp)
    return XROUTE_ERR_IO;

  int rows = xroute_map_get_rows(map);
  int cols = xroute_map_get_cols(map);
  int cell_size = 28;
  int margin = 40;
  int width = cols * cell_size + 2 * margin;
  int height = rows * cell_size + 2 * margin;

  fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(fp,
          "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" "
          "height=\"%d\" viewBox=\"0 0 %d %d\">\n",
          width, height, width, height);
  fprintf(fp, "<style>\n");
  fprintf(fp, "  .bg { fill: #f8fafc; }\n");
  fprintf(fp, "  .grid-line { stroke: #e2e8f0; stroke-width: 1; }\n");
  fprintf(fp, "  .transit { fill: #ffffff; }\n");
  fprintf(fp, "  .rack { fill: #3b82f6; rx: 4; }\n");
  fprintf(fp, "  .rack-t { fill: #60a5fa; rx: 4; }\n");
  fprintf(fp, "  .obstacle { fill: #ef4444; rx: 4; }\n");
  fprintf(fp, "  .restricted { fill: #fef08a; }\n");
  fprintf(fp, "  .path-line { fill: none; stroke: #10b981; stroke-width: 4; "
              "stroke-linecap: round; stroke-linejoin: round; }\n");
  fprintf(fp, "  .node-start { fill: #059669; }\n");
  fprintf(fp, "  .node-goal { fill: #d97706; }\n");
  fprintf(fp, "  .label { font-family: monospace; font-size: 10px; fill: "
              "#64748b; text-anchor: middle; dominant-baseline: middle; }\n");
  fprintf(fp, "</style>\n");

  fprintf(fp, "<rect width=\"100%%\" height=\"100%%\" class=\"bg\"/>\n");

  /* Draw grid cells */
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      int cx = margin + x * cell_size;
      int cy = margin + y * cell_size;
      xroute_cell_type_t t = xroute_map_get_cell_type(map, x, y);

      const char *cls = "transit";
      if (t == XROUTE_CELL_RACK_BLOCKED)
        cls = "rack";
      else if (t == XROUTE_CELL_RACK_TRANSIT)
        cls = "rack-t";
      else if (t == XROUTE_CELL_OBSTACLE)
        cls = "obstacle";
      else if (t == XROUTE_CELL_RESTRICTED)
        cls = "restricted";

      fprintf(fp,
              "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" class=\"%s\" "
              "stroke=\"#cbd5e1\" stroke-width=\"1\"/>\n",
              cx, cy, cell_size, cell_size, cls);
    }
  }

  /* Draw route polyline */
  if (route && route->points && route->length > 1) {
    fprintf(fp, "<polyline class=\"path-line\" points=\"");
    for (int i = 0; i < route->length; ++i) {
      int px = margin + route->points[i].x * cell_size + cell_size / 2;
      int py = margin + route->points[i].y * cell_size + cell_size / 2;
      fprintf(fp, "%d,%d ", px, py);
    }
    fprintf(fp, "\"/>\n");

    /* Mark start circle */
    int sx = margin + route->points[0].x * cell_size + cell_size / 2;
    int sy = margin + route->points[0].y * cell_size + cell_size / 2;
    fprintf(fp, "<circle cx=\"%d\" cy=\"%d\" r=\"7\" class=\"node-start\"/>\n",
            sx, sy);

    /* Mark goal circle */
    int gx =
        margin + route->points[route->length - 1].x * cell_size + cell_size / 2;
    int gy =
        margin + route->points[route->length - 1].y * cell_size + cell_size / 2;
    fprintf(fp, "<circle cx=\"%d\" cy=\"%d\" r=\"7\" class=\"node-goal\"/>\n",
            gx, gy);
  }

  fprintf(fp, "</svg>\n");
  fclose(fp);
  return XROUTE_SUCCESS;
}
