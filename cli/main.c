/**
 * @file main.c
 * @brief Modern, cross-platform CLI tool for xROUTE.
 */

#include "xroute/xroute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int compare_doubles(const void *a, const void *b) {
  const double left = *(const double *)a;
  const double right = *(const double *)b;
  return (left > right) - (left < right);
}

static void print_usage(const char *prog) {
  printf("xROUTE v%s - Warehouse Routing & Fleet Dispatch CLI\n\n",
         XROUTE_VERSION_STRING);
  printf("Usage:\n");
  printf("  %s serve   [port] (Launches interactive xROUTE Studio Web UI on "
         "port 8000)\n",
         prog);
  printf("  %s route   --map <map.xml|map.csv> --start <x,y> --goal <x,y> "
         "[options]\n",
         prog);
  printf("  %s bay     --map <map.xml|map.csv> --start <x,y> --corsia <c> "
         "--campata <b> [options]\n",
         prog);
  printf("  %s fleet   --map <map.xml|map.csv> --corsia <c> --campata <b> "
         "--forklifts <x1,y1> [x2,y2 ...] [options]\n",
         prog);
  printf("  %s convert --csv <in.csv> --xml <out.xml>\n", prog);
  printf("  %s view    --map <map.xml|map.csv>\n", prog);
  printf("  %s bench   --map <map.xml|map.csv> [num_queries]\n\n", prog);
  printf("Options:\n");
  printf("  --visualize           Print ASCII map with overlaid path\n");
  printf("  --svg <file.svg>      Export vector SVG visual map with route\n");
  printf(
      "  --turn-penalty <cm>   Add cost penalty per 90-degree turn (in cm)\n");
  printf("  --speed <m/s>         Set vehicle speed in m/s (default 1.5)\n");
  printf("  --block <x,y>         Add dynamic temporary obstacle at x,y\n");
}

static xroute_map_t *load_map_auto(const char *path) {
  xroute_map_t *map = NULL;
  if (strstr(path, ".csv") || strstr(path, ".CSV")) {
    if (xroute_map_load_csv(path, 350, 200, &map) != XROUTE_SUCCESS)
      return NULL;
  } else {
    if (xroute_map_load_xml(path, &map) != XROUTE_SUCCESS)
      return NULL;
  }
  return map;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "serve") == 0) {
    int port = (argc >= 3) ? atoi(argv[2]) : 8000;
    char launch_cmd[256];
    snprintf(launch_cmd, sizeof(launch_cmd), "python3 -m xroute.ui %d", port);
    printf("Starting xROUTE Studio Web UI on port %d...\n", port);
    return system(launch_cmd);
  }

  if (strcmp(cmd, "convert") == 0) {
    const char *csv_path = NULL;
    const char *xml_path = NULL;
    for (int i = 2; i < argc; ++i) {
      if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc)
        csv_path = argv[++i];
      else if (strcmp(argv[i], "--xml") == 0 && i + 1 < argc)
        xml_path = argv[++i];
    }
    if (!csv_path || !xml_path) {
      printf("Error: Missing --csv or --xml argument.\n");
      return 1;
    }
    xroute_status_t st = xroute_csv_to_xml(csv_path, xml_path, 350, 200);
    if (st != XROUTE_SUCCESS) {
      printf("Error converting CSV to XML: status code %d\n", st);
      return 1;
    }
    printf("Successfully converted '%s' -> '%s'\n", csv_path, xml_path);
    return 0;
  }

  if (strcmp(cmd, "view") == 0) {
    const char *map_path = NULL;
    for (int i = 2; i < argc; ++i) {
      if (strcmp(argv[i], "--map") == 0 && i + 1 < argc)
        map_path = argv[++i];
    }
    if (!map_path) {
      printf("Error: Missing --map argument.\n");
      return 1;
    }
    xroute_map_t *map = load_map_auto(map_path);
    if (!map) {
      printf("Error: Failed to load map '%s'\n", map_path);
      return 1;
    }
    printf("Warehouse Map: %dx%d cells, cell size: %dx%d cm\n\n",
           xroute_map_get_cols(map), xroute_map_get_rows(map),
           xroute_map_get_cell_width_cm(map),
           xroute_map_get_cell_height_cm(map));
    xroute_visualizer_print_map(map, stdout);
    xroute_map_free(map);
    return 0;
  }

  if (strcmp(cmd, "route") == 0 || strcmp(cmd, "bay") == 0) {
    const char *map_path = NULL;
    xroute_point_t start = {-1, -1};
    xroute_point_t goal = {-1, -1};
    int corsia = -1, campata = -1;
    bool visualize = false;
    const char *svg_out = NULL;
    xroute_options_t opts = xroute_default_options();

    for (int i = 2; i < argc; ++i) {
      if (strcmp(argv[i], "--map") == 0 && i + 1 < argc)
        map_path = argv[++i];
      else if (strcmp(argv[i], "--start") == 0 && i + 1 < argc)
        sscanf(argv[++i], "%d,%d", &start.x, &start.y);
      else if (strcmp(argv[i], "--goal") == 0 && i + 1 < argc)
        sscanf(argv[++i], "%d,%d", &goal.x, &goal.y);
      else if (strcmp(argv[i], "--corsia") == 0 && i + 1 < argc)
        corsia = atoi(argv[++i]);
      else if (strcmp(argv[i], "--campata") == 0 && i + 1 < argc)
        campata = atoi(argv[++i]);
      else if (strcmp(argv[i], "--visualize") == 0)
        visualize = true;
      else if (strcmp(argv[i], "--svg") == 0 && i + 1 < argc)
        svg_out = argv[++i];
      else if (strcmp(argv[i], "--turn-penalty") == 0 && i + 1 < argc)
        opts.turn_penalty_cost = atoi(argv[++i]);
      else if (strcmp(argv[i], "--speed") == 0 && i + 1 < argc)
        opts.speed_mps = atof(argv[++i]);
    }

    if (!map_path || start.x < 0 || start.y < 0) {
      printf("Error: Missing required arguments (--map, --start).\n");
      return 1;
    }

    xroute_map_t *map = load_map_auto(map_path);
    if (!map) {
      printf("Error: Failed to load map '%s'\n", map_path);
      return 1;
    }

    xroute_router_t *router = xroute_router_create(map);
    xroute_route_t *route = NULL;
    xroute_status_t st = XROUTE_SUCCESS;

    if (strcmp(cmd, "bay") == 0) {
      if (corsia < 0 || campata < 0) {
        printf("Error: Missing --corsia or --campata for bay route.\n");
        xroute_router_free(router);
        xroute_map_free(map);
        return 1;
      }
      st = xroute_router_route_to_bay(router, start, corsia, campata, &opts,
                                      &route);
    } else {
      if (goal.x < 0 || goal.y < 0) {
        printf("Error: Missing --goal coordinate.\n");
        xroute_router_free(router);
        xroute_map_free(map);
        return 1;
      }
      st = xroute_router_find_path(router, start, goal, &opts, &route);
    }

    if (st != XROUTE_SUCCESS || !route) {
      printf("Routing failed: error code %d (no reachable path found).\n", st);
      xroute_router_free(router);
      xroute_map_free(map);
      return 1;
    }

    printf("Optimal Route Found:\n");
    printf("  Path length:    %d steps\n", route->length);
    printf("  Total cost:     %d cm\n", route->total_cost);
    printf("  Distance:       %.2f meters\n", route->distance_meters);
    printf("  Estimated time: %.2f seconds\n", route->estimated_time_sec);
    printf("  Turns:          %d\n\n", route->turns_count);

    printf("Steps:\n");
    for (int i = 0; i < route->length; ++i) {
      printf("  [%2d] (x=%d, y=%d)\n", i, route->points[i].x,
             route->points[i].y);
    }

    if (visualize) {
      printf("\nVisual Map:\n");
      xroute_visualizer_print_route(map, route, stdout);
    }

    if (svg_out) {
      xroute_visualizer_export_svg(map, route, svg_out);
      printf("\nExported route to SVG: %s\n", svg_out);
    }

    xroute_route_free(route);
    xroute_router_free(router);
    xroute_map_free(map);
    return 0;
  }

  if (strcmp(cmd, "fleet") == 0) {
    const char *map_path = NULL;
    int corsia = -1, campata = -1;
    xroute_operator_t forklifts[64];
    int num_forklifts = 0;
    bool visualize = false;

    for (int i = 2; i < argc; ++i) {
      if (strcmp(argv[i], "--map") == 0 && i + 1 < argc)
        map_path = argv[++i];
      else if (strcmp(argv[i], "--corsia") == 0 && i + 1 < argc)
        corsia = atoi(argv[++i]);
      else if (strcmp(argv[i], "--campata") == 0 && i + 1 < argc)
        campata = atoi(argv[++i]);
      else if (strcmp(argv[i], "--visualize") == 0)
        visualize = true;
      else if (strcmp(argv[i], "--forklifts") == 0) {
        while (i + 1 < argc && argv[i + 1][0] != '-') {
          i++;
          int fx = -1, fy = -1;
          if (sscanf(argv[i], "%d,%d", &fx, &fy) == 2) {
            forklifts[num_forklifts].id = num_forklifts + 1;
            forklifts[num_forklifts].x = fx;
            forklifts[num_forklifts].y = fy;
            forklifts[num_forklifts].name = "Forklift";
            num_forklifts++;
          }
        }
      }
    }

    if (!map_path || corsia < 0 || campata < 0 || num_forklifts == 0) {
      printf(
          "Error: Missing --map, --corsia, --campata, or --forklifts list.\n");
      return 1;
    }

    xroute_map_t *map = load_map_auto(map_path);
    if (!map) {
      printf("Error: Failed to load map '%s'\n", map_path);
      return 1;
    }

    xroute_router_t *router = xroute_router_create(map);
    xroute_fleet_result_t result;
    xroute_options_t opts = xroute_default_options();

    xroute_status_t st = xroute_router_find_nearest_operator(
        router, forklifts, num_forklifts, corsia, campata, &opts, &result);
    if (st != XROUTE_SUCCESS) {
      printf("Fleet search failed: error code %d\n", st);
      xroute_router_free(router);
      xroute_map_free(map);
      return 1;
    }

    printf("Nearest Forklift Operator: #%d (at x=%d, y=%d)\n",
           result.best_operator_id, result.start_point.x, result.start_point.y);
    printf("  Distance to Bay (Corsia %d, Campata %d): %.2f meters (%d cm)\n",
           corsia, campata, result.distance_meters, result.distance_cost);
    printf("  Target Bay Access Cell: (x=%d, y=%d)\n", result.target_cell.x,
           result.target_cell.y);
    printf("  Estimated Travel Time:  %.1f s\n",
           result.route ? result.route->estimated_time_sec : 0.0);

    if (visualize && result.route) {
      printf("\nPath Visualization:\n");
      xroute_visualizer_print_route(map, result.route, stdout);
    }

    xroute_fleet_result_free(&result);
    xroute_router_free(router);
    xroute_map_free(map);
    return 0;
  }

  if (strcmp(cmd, "bench") == 0) {
    /* Accept both "bench --map <map> [n]" and the legacy "bench <map> [n]". */
    int arg = 2;
    if (arg < argc && strcmp(argv[arg], "--map") == 0)
      arg++;
    if (arg >= argc) {
      print_usage(argv[0]);
      return 1;
    }
    const char *map_path = argv[arg++];
    int queries = (arg < argc) ? atoi(argv[arg]) : 1000;
    if (queries <= 0) {
      printf("Error: num_queries must be a positive integer\n");
      return 1;
    }

    xroute_map_t *map = load_map_auto(map_path);
    if (!map) {
      printf("Error: Could not load map '%s'\n", map_path);
      return 1;
    }

    int cols = xroute_map_get_cols(map);
    int rows = xroute_map_get_rows(map);
    printf("Benchmarking A* Routing Engine on %s (%dx%d = %d cells)...\n",
           map_path, cols, rows, cols * rows);

    xroute_router_t *router = xroute_router_create(map);
    xroute_options_t opts = xroute_default_options();

    double *samples_ms =
        (double *)malloc((size_t)queries * sizeof(*samples_ms));
    if (!samples_ms) {
      xroute_router_free(router);
      xroute_map_free(map);
      printf("Error: unable to allocate benchmark samples\n");
      return 1;
    }
    int successful = 0;

    srand(42);
    for (int i = 0; i < queries; ++i) {
      xroute_point_t s = {rand() % cols, rand() % rows};
      xroute_point_t g = {rand() % cols, rand() % rows};
      xroute_route_t *r = NULL;
      clock_t t0 = clock();
      if (xroute_router_find_path(router, s, g, &opts, &r) == XROUTE_SUCCESS &&
          r) {
        successful++;
        xroute_route_free(r);
      }
      samples_ms[i] = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    }
    qsort(samples_ms, (size_t)queries, sizeof(*samples_ms), compare_doubles);
    double total_ms = 0.0;
    for (int i = 0; i < queries; ++i)
      total_ms += samples_ms[i];
    int p95_index = (int)(0.95 * (queries - 1));

    printf("Results:\n");
    printf("  Queries tested:     %d\n", queries);
    printf("  Valid paths found:  %d\n", successful);
    printf("  Method:             deterministic rand seed 42; one query per "
           "sample\n");
    printf("  Median query time:   %.3f ms\n", samples_ms[queries / 2]);
    printf("  P95 query time:      %.3f ms\n", samples_ms[p95_index]);
    printf("  Mean throughput:     %.0f queries/second\n\n",
           queries / (total_ms > 0.0 ? total_ms / 1000.0 : 1e-6));

    free(samples_ms);
    xroute_router_free(router);
    xroute_map_free(map);
    return 0;
  }

  print_usage(argv[0]);
  return 1;
}
