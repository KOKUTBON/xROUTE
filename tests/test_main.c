/**
 * @file test_main.c
 * @brief Comprehensive automated unit test suite for xROUTE.
 */

#include "xroute/xroute.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg);                 \
      return false;                                                            \
    }                                                                          \
  } while (0)

#define RUN_TEST(fn)                                                           \
  do {                                                                         \
    g_tests_run++;                                                             \
    printf("Running %s...", #fn);                                              \
    fflush(stdout);                                                            \
    if (fn()) {                                                                \
      g_tests_passed++;                                                        \
      printf(" PASSED\n");                                                     \
    } else {                                                                   \
      printf(" FAILED\n");                                                     \
    }                                                                          \
  } while (0)

static bool test_map_creation_and_bounds(void) {
  xroute_map_t *map = xroute_map_create(10, 20, 300, 150);
  TEST_ASSERT(map != NULL, "Failed to create map");
  TEST_ASSERT(xroute_map_get_rows(map) == 10, "Rows mismatch");
  TEST_ASSERT(xroute_map_get_cols(map) == 20, "Cols mismatch");
  TEST_ASSERT(xroute_map_get_cell_width_cm(map) == 300, "Cell width mismatch");
  TEST_ASSERT(xroute_map_get_cell_height_cm(map) == 150,
              "Cell height mismatch");

  TEST_ASSERT(xroute_map_set_cell(map, 0, 0, XROUTE_CELL_TRANSIT,
                                  XROUTE_DIR_ALL) == XROUTE_SUCCESS,
              "Set cell 0,0");
  TEST_ASSERT(xroute_map_set_cell(map, -1, 0, XROUTE_CELL_TRANSIT,
                                  XROUTE_DIR_ALL) == XROUTE_ERR_OUT_OF_BOUNDS,
              "Negative X should fail");
  TEST_ASSERT(xroute_map_set_cell(map, 20, 0, XROUTE_CELL_TRANSIT,
                                  XROUTE_DIR_ALL) == XROUTE_ERR_OUT_OF_BOUNDS,
              "X >= cols should fail");
  TEST_ASSERT(xroute_map_set_cell(map, 0, 10, XROUTE_CELL_TRANSIT,
                                  XROUTE_DIR_ALL) == XROUTE_ERR_OUT_OF_BOUNDS,
              "Y >= rows should fail");

  /* Coordinate conversions */
  int node = xroute_map_xy_to_node(5, 3, 20);
  int x, y;
  xroute_map_node_to_xy(node, 20, &x, &y);
  TEST_ASSERT(x == 5 && y == 3, "Node conversion mismatch");

  xroute_map_free(map);
  return true;
}

static bool test_cell_movements_and_types(void) {
  xroute_map_t *map = xroute_map_create(5, 5, 100, 100);
  TEST_ASSERT(map != NULL, "Map create failed");

  /* Mark (2, 2) as obstacle */
  xroute_map_set_cell(map, 2, 2, XROUTE_CELL_OBSTACLE, XROUTE_DIR_NONE);
  TEST_ASSERT(!xroute_map_is_traversable(map, 2, 2),
              "Obstacle should not be traversable");
  TEST_ASSERT(!xroute_map_can_transit(map, 1, 2, 2, 2),
              "Cannot transit into obstacle");

  /* Restricted cell at (1, 1): only DOWN allowed */
  xroute_map_set_cell(map, 1, 1, XROUTE_CELL_RESTRICTED, XROUTE_DIR_DOWN);
  TEST_ASSERT(xroute_map_can_transit(map, 1, 1, 1, 2),
              "Movement DOWN from (1,1) should be allowed");
  TEST_ASSERT(!xroute_map_can_transit(map, 1, 1, 2, 1),
              "Movement RIGHT from (1,1) should be disallowed");

  xroute_map_free(map);
  return true;
}

static bool test_xml_parser_basic(void) {
  const char *xml =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<Magazzino>\n"
      "<Deposito id=\"1\" name=\"TestDep\" numRighe=\"4\" numColonne=\"3\" "
      "dimColonne=\"350\" dimRighe=\"200\">\n"
      "\t<Cella x=\"1\" y=\"0\" Tipo=\"1\" Corsia=\"1\" Campata=\"1\">\n"
      "\t\t<LatoAccessibile>dw</LatoAccessibile>\n"
      "\t</Cella>\n"
      "\t<Cella x=\"2\" y=\"0\" Tipo=\"1\" Corsia=\"1\" Campata=\"2\">\n"
      "\t\t<LatoAccessibile>dw</LatoAccessibile>\n"
      "\t</Cella>\n"
      "\t<Cella x=\"1\" y=\"2\" Tipo=\"2\" Corsia=\"8\" Campata=\"24\">\n"
      "\t\t<LatoAccessibile>dw</LatoAccessibile>\n"
      "\t\t<LatoAccessibile>up</LatoAccessibile>\n"
      "\t\t<SpostamentoPermesso>dx</SpostamentoPermesso>\n"
      "\t\t<SpostamentoPermesso>dw</SpostamentoPermesso>\n"
      "\t</Cella>\n"
      "</Deposito>\n"
      "</Magazzino>\n";

  xroute_map_t *map = NULL;
  xroute_status_t st = xroute_map_load_xml_string(xml, &map);
  TEST_ASSERT(st == XROUTE_SUCCESS, "Load embedded XML failed");
  TEST_ASSERT(map != NULL, "Map is null");
  TEST_ASSERT(xroute_map_get_rows(map) == 4, "Rows should be 4");
  TEST_ASSERT(xroute_map_get_cols(map) == 3, "Cols should be 3");

  /* Check bays */
  const xroute_bay_t *b1 = xroute_map_find_bay(map, 1, 1);
  TEST_ASSERT(b1 != NULL, "Bay (1, 1) not found");
  TEST_ASSERT(b1->x == 1 && b1->y == 0, "Bay (1, 1) coordinate mismatch");
  TEST_ASSERT(b1->accessible_sides == XROUTE_DIR_DOWN,
              "Bay (1, 1) accessible side should be dw");

  const xroute_bay_t *b2 = xroute_map_find_bay(map, 8, 24);
  TEST_ASSERT(b2 != NULL, "Bay (8, 24) not found");
  TEST_ASSERT(b2->type == XROUTE_CELL_RACK_TRANSIT,
              "Bay (8, 24) should be type 2 (transit rack)");
  TEST_ASSERT((b2->accessible_sides & XROUTE_DIR_DOWN) &&
                  (b2->accessible_sides & XROUTE_DIR_UP),
              "Bay (8, 24) sides");

  xroute_map_free(map);
  return true;
}

static bool test_xml_parser_large(void) {
  /* Dynamically generate a large 32x32 warehouse XML in memory */
  char *large_xml = (char *)malloc(65536);
  TEST_ASSERT(large_xml != NULL, "Alloc large_xml");

  int offset = snprintf(large_xml, 65536,
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Magazzino numRighe=\"32\" numColonne=\"32\" "
                        "dimColonne=\"350\" dimRighe=\"200\">\n");

  /* Add 60 bays */
  for (int i = 0; i < 60; ++i) {
    int x = 2 + (i % 28);
    int y = 3 + (i / 28) * 4;
    offset += snprintf(
        large_xml + offset, 65536 - offset,
        "\t<Cella x=\"%d\" y=\"%d\" Tipo=\"1\" Corsia=\"8\" Campata=\"%d\">\n"
        "\t\t<LatoAccessibile>up</LatoAccessibile>\n"
        "\t</Cella>\n",
        x, y, i + 1);
  }
  snprintf(large_xml + offset, 65536 - offset, "</Magazzino>\n");

  xroute_map_t *map = NULL;
  xroute_status_t st = xroute_map_load_xml_string(large_xml, &map);
  free(large_xml);

  TEST_ASSERT(st == XROUTE_SUCCESS, "Load large XML failed");
  TEST_ASSERT(map != NULL, "Map is null");
  TEST_ASSERT(xroute_map_get_rows(map) == 32, "Rows should be 32");
  TEST_ASSERT(xroute_map_get_cols(map) == 32, "Cols should be 32");

  int bay_count = 0;
  const xroute_bay_t *bays = xroute_map_get_all_bays(map, &bay_count);
  TEST_ASSERT(bays != NULL && bay_count == 60, "Should have loaded 60 bays");

  const xroute_bay_t *b = xroute_map_find_bay(map, 8, 1);
  TEST_ASSERT(b != NULL, "Bay (8, 1) not found");
  TEST_ASSERT(b->x == 2 && b->y == 3, "Bay (8, 1) position");

  xroute_map_free(map);
  return true;
}

static bool test_csv_parser_and_conversion(void) {
  const char *tmp_csv = "test_temp_map.csv";
  FILE *fp = fopen(tmp_csv, "w");
  TEST_ASSERT(fp != NULL, "Create test CSV");
  fprintf(fp, "6,4,,\n");
  fprintf(fp, ",,,\n");
  fprintf(fp, "2,6,x,8\n");
  fprintf(fp, "2,,,\n");
  fprintf(fp, "2,5,3,2\n");
  fprintf(fp, "1,6,x,8\n");
  fprintf(fp, "1,,,\n");
  fprintf(fp, "1,5,3,2\n");
  fclose(fp);

  xroute_map_t *map = NULL;
  xroute_status_t st = xroute_map_load_csv(tmp_csv, 350, 200, &map);
  remove(tmp_csv);

  TEST_ASSERT(st == XROUTE_SUCCESS, "Load CSV failed");
  TEST_ASSERT(map != NULL, "Map is null");
  TEST_ASSERT(xroute_map_get_rows(map) == 6, "CSV rows should be 6");
  TEST_ASSERT(xroute_map_get_cols(map) == 4, "CSV cols should be 4");

  /* Check obstacle at row 0, col 2 -> (1, 0) */
  TEST_ASSERT(xroute_map_get_cell_type(map, 1, 0) == XROUTE_CELL_OBSTACLE,
              "(1,0) should be obstacle");

  /* Check bay (corsia 2, campata 6) at (0, 0) */
  const xroute_bay_t *b = xroute_map_find_bay(map, 2, 6);
  TEST_ASSERT(b != NULL, "Bay (2, 6) not found");
  TEST_ASSERT(b->x == 0 && b->y == 0, "Bay (2, 6) coordinates");
  TEST_ASSERT(b->accessible_sides & XROUTE_DIR_DOWN,
              "Bay (2, 6) should be accessible from bottom");

  /* Test roundtrip: save to XML and reload */
  const char *tmp_xml = "test_roundtrip.xml";
  st = xroute_map_save_xml(map, tmp_xml);
  TEST_ASSERT(st == XROUTE_SUCCESS, "Save XML failed");

  xroute_map_t *reloaded = NULL;
  st = xroute_map_load_xml(tmp_xml, &reloaded);
  TEST_ASSERT(st == XROUTE_SUCCESS, "Reload XML failed");
  TEST_ASSERT(xroute_map_get_rows(reloaded) == 6 &&
                  xroute_map_get_cols(reloaded) == 4,
              "Reloaded dims");

  remove(tmp_xml);
  xroute_map_free(reloaded);
  xroute_map_free(map);
  return true;
}

static bool test_astar_routing_optimality(void) {
  xroute_map_t *map = xroute_map_create(5, 5, 100, 100);
  /* Add obstacle in the middle at (2, 0), (2, 1), (2, 2) */
  xroute_map_set_cell(map, 2, 0, XROUTE_CELL_OBSTACLE, XROUTE_DIR_NONE);
  xroute_map_set_cell(map, 2, 1, XROUTE_CELL_OBSTACLE, XROUTE_DIR_NONE);
  xroute_map_set_cell(map, 2, 2, XROUTE_CELL_OBSTACLE, XROUTE_DIR_NONE);

  xroute_router_t *router = xroute_router_create(map);
  xroute_point_t start = {0, 0};
  xroute_point_t goal = {4, 0};

  xroute_route_t *route = NULL;
  xroute_status_t st =
      xroute_router_find_path(router, start, goal, NULL, &route);
  TEST_ASSERT(st == XROUTE_SUCCESS, "Pathfind failed");
  TEST_ASSERT(route != NULL, "Route is null");

  /* The path must go around the obstacle (y >= 3) */
  bool routed_around = false;
  for (int i = 0; i < route->length; ++i) {
    if (route->points[i].y >= 3)
      routed_around = true;
    TEST_ASSERT(
        xroute_map_is_traversable(map, route->points[i].x, route->points[i].y),
        "Route stepped on non-traversable cell");
  }
  TEST_ASSERT(routed_around, "Path should have routed around obstacle");
  TEST_ASSERT(route->total_cost == (route->length - 1) * 100,
              "Cost calculation");

  xroute_route_free(route);
  xroute_router_free(router);
  xroute_map_free(map);
  return true;
}

static bool test_dynamic_obstacles(void) {
  xroute_map_t *map = xroute_map_create(3, 3, 100, 100);
  xroute_router_t *router = xroute_router_create(map);

  xroute_point_t start = {0, 0};
  xroute_point_t goal = {2, 0};

  xroute_route_t *r1 = NULL;
  TEST_ASSERT(xroute_router_find_path(router, start, goal, NULL, &r1) ==
                  XROUTE_SUCCESS,
              "Initial path");
  TEST_ASSERT(r1->length == 3, "Straight path has length 3");
  xroute_route_free(r1);

  /* Dynamically block (1, 0) */
  xroute_map_set_dynamic_obstacle(map, 1, 0, true);
  TEST_ASSERT(!xroute_map_is_traversable(map, 1, 0),
              "Dynamic obstacle should be non-traversable");

  xroute_route_t *r2 = NULL;
  TEST_ASSERT(xroute_router_find_path(router, start, goal, NULL, &r2) ==
                  XROUTE_SUCCESS,
              "Path after obstacle");
  TEST_ASSERT(r2->length > 3, "New path must be longer because of diversion");
  for (int i = 0; i < r2->length; ++i) {
    TEST_ASSERT(!(r2->points[i].x == 1 && r2->points[i].y == 0),
                "Path must not contain dynamic obstacle");
  }
  xroute_route_free(r2);

  /* Unblock */
  xroute_map_set_dynamic_obstacle(map, 1, 0, false);
  TEST_ASSERT(xroute_map_is_traversable(map, 1, 0),
              "Should be traversable again");

  xroute_router_free(router);
  xroute_map_free(map);
  return true;
}

static bool test_fleet_nearest_operator(void) {
  xroute_map_t *map = xroute_map_create(10, 10, 100, 100);
  /* Add a bay at (5, 5) accessible only from right (6, 5) */
  xroute_map_add_bay(map, 5, 5, XROUTE_CELL_RACK_BLOCKED, 1, 10,
                     XROUTE_DIR_RIGHT, XROUTE_DIR_NONE);

  xroute_operator_t forklifts[3] = {{.id = 1, .x = 0, .y = 5, .name = "FL-1"},
                                    {.id = 2, .x = 9, .y = 5, .name = "FL-2"},
                                    {.id = 3, .x = 5, .y = 9, .name = "FL-3"}};

  xroute_router_t *router = xroute_router_create(map);
  xroute_fleet_result_t res;
  xroute_status_t st = xroute_router_find_nearest_operator(router, forklifts, 3,
                                                           1, 10, NULL, &res);
  TEST_ASSERT(st == XROUTE_SUCCESS, "Fleet search failed");
  /* Access cell is (6, 5). FL-2 at (9, 5) is 3 units away. FL-1 is at (0, 5) ->
   * 6 units away. FL-3 at (5, 9) is (1+4)=5 units away */
  TEST_ASSERT(res.best_operator_id == 2, "FL-2 should be closest operator");
  TEST_ASSERT(res.target_cell.x == 6 && res.target_cell.y == 5,
              "Target access cell");
  TEST_ASSERT(res.route != NULL && res.route->length == 4, "Route steps");

  xroute_fleet_result_free(&res);
  xroute_router_free(router);
  xroute_map_free(map);
  return true;
}

int main(void) {
  printf("====================================================\n");
  printf("xROUTE v%s Test Suite\n", XROUTE_VERSION_STRING);
  printf("====================================================\n");

  RUN_TEST(test_map_creation_and_bounds);
  RUN_TEST(test_cell_movements_and_types);
  RUN_TEST(test_xml_parser_basic);
  RUN_TEST(test_xml_parser_large);
  RUN_TEST(test_csv_parser_and_conversion);
  RUN_TEST(test_astar_routing_optimality);
  RUN_TEST(test_dynamic_obstacles);
  RUN_TEST(test_fleet_nearest_operator);

  printf("====================================================\n");
  printf("Tests Run: %d | Passed: %d | Failed: %d\n", g_tests_run,
         g_tests_passed, g_tests_run - g_tests_passed);
  printf("====================================================\n");

  return (g_tests_run == g_tests_passed) ? 0 : 1;
}
