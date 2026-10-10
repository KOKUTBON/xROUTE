/**
 * @file types.h
 * @brief Core types, enumerations, and structures for xROUTE.
 * @author xROUTE Team
 */

#ifndef XROUTE_TYPES_H
#define XROUTE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Version information */
#define XROUTE_VERSION_MAJOR 2
#define XROUTE_VERSION_MINOR 0
#define XROUTE_VERSION_PATCH 0
#define XROUTE_VERSION_STRING "2.0.0"

/** Return status codes */
typedef enum {
  XROUTE_SUCCESS = 0,
  XROUTE_ERR_GENERIC = -1,
  XROUTE_ERR_INVALID_PARAM = -2,
  XROUTE_ERR_FILE_NOT_FOUND = -3,
  XROUTE_ERR_PARSE_FAILED = -4,
  XROUTE_ERR_OUT_OF_BOUNDS = -5,
  XROUTE_ERR_NO_PATH = -6,
  XROUTE_ERR_BAY_NOT_FOUND = -7,
  XROUTE_ERR_NO_MEMORY = -8,
  XROUTE_ERR_IO = -9
} xroute_status_t;

/** Warehouse cell types */
typedef enum {
  XROUTE_CELL_TRANSIT = 0, /**< Standard corridor cell: traversable in all 4
                              directions unless restricted */
  XROUTE_CELL_RACK_BLOCKED = 1, /**< Type 1: Rack/bay non-traversable. Accessed
                                   from designated neighbor sides */
  XROUTE_CELL_RACK_TRANSIT =
      2, /**< Type 2: Rack/bay traversable in specific directions and accessible
            from designated sides */
  XROUTE_CELL_RESTRICTED = 3, /**< Type 3: Transit cell with direction
                                 constraints (e.g. one-way aisle) */
  XROUTE_CELL_OBSTACLE = 4    /**< Type 4: Non-traversable obstacle */
} xroute_cell_type_t;

/** Direction and side flags (bitmask) */
typedef enum {
  XROUTE_DIR_NONE = 0,
  XROUTE_DIR_LEFT = (1 << 0),  /**< Left / sx (-1, 0) */
  XROUTE_DIR_UP = (1 << 1),    /**< Up / up (0, -1) */
  XROUTE_DIR_RIGHT = (1 << 2), /**< Right / dx (+1, 0) */
  XROUTE_DIR_DOWN = (1 << 3),  /**< Down / dw (0, +1) */
  XROUTE_DIR_ALL =
      (XROUTE_DIR_LEFT | XROUTE_DIR_UP | XROUTE_DIR_RIGHT | XROUTE_DIR_DOWN)
} xroute_direction_t;

/** Coordinate point (0-indexed) */
typedef struct {
  int x;
  int y;
} xroute_point_t;

/** Rack/Bay specification */
typedef struct {
  int x;
  int y;
  int corsia;  /**< Aisle identifier */
  int campata; /**< Bay identifier within the aisle */
  xroute_cell_type_t
      type; /**< XROUTE_CELL_RACK_BLOCKED or XROUTE_CELL_RACK_TRANSIT */
  uint8_t accessible_sides;  /**< Bitmask of XROUTE_DIR_* flags indicating which
                                neighbor cells can access this bay */
  uint8_t allowed_movements; /**< Bitmask of XROUTE_DIR_* flags for movement
                                through this cell (if traversable) */
} xroute_bay_t;

/** Routing path result */
typedef struct {
  xroute_point_t
      *points;    /**< Array of path points from start to arrival cell */
  int length;     /**< Number of points in the path */
  int total_cost; /**< Total weighted cost (metric centimeters by default) */
  double distance_meters;    /**< Physical distance in meters */
  double estimated_time_sec; /**< Estimated transit time in seconds (considering
                                speed and turns) */
  int turns_count;           /**< Number of 90-degree turns along the path */
} xroute_route_t;

/** Forklift operator in fleet */
typedef struct {
  int id;           /**< Operator / Forklift ID */
  int x;            /**< Current X coordinate */
  int y;            /**< Current Y coordinate */
  const char *name; /**< Optional label or name */
} xroute_operator_t;

/** Result of nearest operator fleet search */
typedef struct {
  int best_operator_index;    /**< Index in the input operators array */
  int best_operator_id;       /**< Operator ID */
  xroute_point_t start_point; /**< Starting point of the best operator */
  xroute_point_t target_cell; /**< Destination cell (neighbor cell providing
                                 access to the bay) */
  int distance_cost;          /**< Path cost in cm */
  double distance_meters;     /**< Distance in meters */
  xroute_route_t *route;      /**< Full calculated route (must be freed with
                                 xroute_route_free) */
} xroute_fleet_result_t;

/** Routing calculation options */
typedef struct {
  int turn_penalty_cost; /**< Additional cost added per 90-degree turn (in cm).
                            0 to disable */
  double speed_mps; /**< Vehicle travel speed in meters per second (default: 1.5
                       m/s) */
  double turn_time_sec; /**< Time lost per 90-degree turn in seconds
                           (default: 2.0 s) */
  bool allow_diagonal;  /**< Allow diagonal motion (default: false for warehouse
                           aisles) */
} xroute_options_t;

#ifdef __cplusplus
}
#endif

#endif /* XROUTE_TYPES_H */
