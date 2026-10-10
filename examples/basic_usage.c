/**
 * @file basic_usage.c
 * @brief Example demonstrating basic usage of the xROUTE C API.
 */

#include "xroute/xroute.h"
#include <stdio.h>

int main(void) {
    printf("xROUTE v%s Basic API Example\n\n", XROUTE_VERSION_STRING);

    /* 1. Load warehouse map directly from XML string (or XML/CSV file) */
    const char *sample_xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Magazzino numRighe=\"4\" numColonne=\"3\" dimColonne=\"350\" dimRighe=\"200\">\n"
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
        "</Magazzino>\n";

    xroute_map_t *map = NULL;
    xroute_status_t st = xroute_map_load_xml_string(sample_xml, &map);
    if (st != XROUTE_SUCCESS) {
        printf("Error loading map: %d\n", st);
        return 1;
    }

    printf("Map dimensions: %dx%d cells\n", xroute_map_get_cols(map), xroute_map_get_rows(map));

    /* 2. Create router instance */
    xroute_router_t *router = xroute_router_create(map);
    if (!router) {
        xroute_map_free(map);
        return 1;
    }

    /* 3. Find optimal route to a specific warehouse rack (corsia 1, campata 1) */
    xroute_point_t start = { 0, 3 };
    xroute_route_t *route = NULL;
    xroute_options_t opts = xroute_default_options();
    opts.turn_penalty_cost = 50; /* 50 cm turn penalty */

    st = xroute_router_route_to_bay(router, start, 1, 1, &opts, &route);
    if (st == XROUTE_SUCCESS && route) {
        printf("\nRoute to Bay (Corsia 1, Campata 1):\n");
        printf("  Steps:          %d\n", route->length);
        printf("  Distance:       %.2f meters\n", route->distance_meters);
        printf("  Estimated time: %.2f seconds\n", route->estimated_time_sec);
        printf("  Turns:          %d\n\n", route->turns_count);

        xroute_visualizer_print_route(map, route, stdout);
        xroute_route_free(route);
    } else {
        printf("Failed to find path to bay: code %d\n", st);
    }

    /* 4. Multi-forklift fleet nearest-operator query */
    xroute_operator_t fleet[3] = {
        { .id = 101, .x = 0, .y = 3, .name = "Forklift-Alpha" },
        { .id = 102, .x = 2, .y = 3, .name = "Forklift-Bravo" },
        { .id = 103, .x = 0, .y = 1, .name = "Forklift-Charlie" }
    };

    xroute_fleet_result_t fleet_res;
    st = xroute_router_find_nearest_operator(router, fleet, 3, 1, 1, &opts, &fleet_res);
    if (st == XROUTE_SUCCESS) {
        printf("\nNearest Forklift in Fleet: Operator #%d\n", fleet_res.best_operator_id);
        printf("  Start position: (%d, %d)\n", fleet_res.start_point.x, fleet_res.start_point.y);
        printf("  Access point:   (%d, %d)\n", fleet_res.target_cell.x, fleet_res.target_cell.y);
        printf("  Distance:       %.2f meters\n", fleet_res.distance_meters);
        xroute_fleet_result_free(&fleet_res);
    }

    /* 5. Clean up */
    xroute_router_free(router);
    xroute_map_free(map);

    printf("\nExample finished successfully!\n");
    return 0;
}
