/**
 * @file parser.h
 * @brief XML and CSV map loaders, savers, and format converters.
 */

#ifndef XROUTE_PARSER_H
#define XROUTE_PARSER_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load a warehouse map from an XML file.
 * Conforms to the specification in Docs/MAP_FORMATS.md.
 * @param filepath Path to XML file.
 * @param out_map Pointer to receive allocated map. Free with xroute_map_free().
 * @return XROUTE_SUCCESS on success, or error code.
 */
xroute_status_t xroute_map_load_xml(const char *filepath,
                                    xroute_map_t **out_map);

/**
 * @brief Load a warehouse map from an XML string in memory.
 */
xroute_status_t xroute_map_load_xml_string(const char *xml_content,
                                           xroute_map_t **out_map);

/**
 * @brief Load a warehouse map from a CSV file.
 * Conforms to the specification in Docs/MAP_FORMATS.md.
 * @param filepath Path to CSV file.
 * @param cell_width_cm Physical width in cm (default 350 if 0).
 * @param cell_height_cm Physical height in cm (default 200 if 0).
 * @param out_map Pointer to receive allocated map.
 * @return XROUTE_SUCCESS on success, or error code.
 */
xroute_status_t xroute_map_load_csv(const char *filepath, int cell_width_cm,
                                    int cell_height_cm, xroute_map_t **out_map);

/**
 * @brief Save a warehouse map to an XML file.
 */
xroute_status_t xroute_map_save_xml(const xroute_map_t *map,
                                    const char *filepath);

/**
 * @brief Convert a CSV map directly into an XML warehouse map file.
 * Converts a CSV map directly into the xROUTE XML map format.
 * @param csv_path Path to input CSV.
 * @param xml_path Path to output XML.
 * @param cell_width_cm Width in cm (e.g. 350).
 * @param cell_height_cm Height in cm (e.g. 200).
 * @return XROUTE_SUCCESS on success, or error code.
 */
xroute_status_t xroute_csv_to_xml(const char *csv_path, const char *xml_path,
                                  int cell_width_cm, int cell_height_cm);

#ifdef __cplusplus
}
#endif

#endif /* XROUTE_PARSER_H */
