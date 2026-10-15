"""
GeoJSON export utilities for xROUTE paths and warehouse layouts.
Converts local grid coordinates into WGS84 geographic features (RFC 7946).
"""

from __future__ import annotations

import math
from typing import Any, Dict, List, Sequence, Tuple, Union

EARTH_RADIUS_METERS = 6378137.0


def grid_to_latlon(
    x: float,
    y: float,
    origin_lat: float,
    origin_lon: float,
    resolution_meters: float = 1.0,
) -> Tuple[float, float]:
    """Convert grid (x, y) coordinates to WGS84 (latitude, longitude) in degrees.

    Parameters:
        x: Horizontal grid coordinate (eastward offset).
        y: Vertical grid coordinate (northward offset).
        origin_lat: Base latitude of the origin (0, 0) in degrees [-90, 90].
        origin_lon: Base longitude of the origin (0, 0) in degrees [-180, 180].
        resolution_meters: Physical scale of each grid cell in meters (default 1.0m).

    Returns:
        (latitude, longitude) tuple in degrees.
    """
    if not (-90.0 <= origin_lat <= 90.0):
        raise ValueError(f"Origin latitude out of range [-90, 90]: {origin_lat}")
    if not (-180.0 <= origin_lon <= 180.0):
        raise ValueError(f"Origin longitude out of range [-180, 180]: {origin_lon}")
    if resolution_meters <= 0:
        raise ValueError(f"Resolution must be positive, got: {resolution_meters}")

    # Northward offset (y) changes latitude
    delta_lat = (y * resolution_meters / EARTH_RADIUS_METERS) * (180.0 / math.pi)
    lat = origin_lat + delta_lat

    # Eastward offset (x) changes longitude, scaled by cos(latitude)
    lat_rad = math.radians(origin_lat)
    cos_lat = math.cos(lat_rad)
    if abs(cos_lat) < 1e-9:
        cos_lat = 1e-9
    delta_lon = (x * resolution_meters / (EARTH_RADIUS_METERS * cos_lat)) * (180.0 / math.pi)
    lon = origin_lon + delta_lon

    return (lat, lon)


def route_to_geojson(
    route: Any,
    origin_lat: float,
    origin_lon: float,
    resolution_meters: float = 1.0,
    geometry_type: str = "LineString",
    as_feature_collection: bool = False,
) -> Dict[str, Any]:
    """Convert an xROUTE computed route to a GeoJSON Feature or FeatureCollection.

    Coordinates follow the RFC 7946 standard: [longitude, latitude].

    Parameters:
        route: An xroute.Route instance or a sequence of (x, y) coordinate tuples.
        origin_lat: Latitude of grid origin (0, 0) in WGS84 degrees.
        origin_lon: Longitude of grid origin (0, 0) in WGS84 degrees.
        resolution_meters: Physical size per grid cell in meters (default: 1.0).
        geometry_type: Geometry to produce: 'LineString' (default) or 'MultiPoint'.
        as_feature_collection: If True, wraps the feature in a FeatureCollection.

    Returns:
        GeoJSON dictionary representation.
    """
    if route is None:
        points: Sequence[Any] = []
    elif hasattr(route, "points"):
        points = route.points
    elif isinstance(route, (list, tuple)):
        points = route
    else:
        raise TypeError(f"Expected Route or coordinate sequence, got {type(route).__name__}")

    coordinates: List[List[float]] = []
    for pt in points:
        if isinstance(pt, (list, tuple)) and len(pt) >= 2:
            x, y = pt[0], pt[1]
        elif hasattr(pt, "x") and hasattr(pt, "y"):
            x, y = pt.x, pt.y
        else:
            raise ValueError(f"Invalid coordinate element in route: {pt}")
        lat, lon = grid_to_latlon(float(x), float(y), origin_lat, origin_lon, resolution_meters)
        coordinates.append([lon, lat])

    norm_type = geometry_type.strip().lower()
    if norm_type in ("linestring", "line_string"):
        actual_geom_type = "LineString"
    elif norm_type in ("multipoint", "multi_point"):
        actual_geom_type = "MultiPoint"
    else:
        raise ValueError(
            f"Unsupported geometry type: '{geometry_type}'. Must be 'LineString' or 'MultiPoint'."
        )

    properties: Dict[str, Any] = {
        "steps": len(points),
        "origin_lat": origin_lat,
        "origin_lon": origin_lon,
        "resolution_meters": resolution_meters,
    }
    if hasattr(route, "distance_meters"):
        properties["distance_meters"] = float(route.distance_meters)
    if hasattr(route, "estimated_time_sec"):
        properties["estimated_time_sec"] = float(route.estimated_time_sec)
    if hasattr(route, "turns"):
        properties["turns"] = int(route.turns)
    if hasattr(route, "total_cost"):
        properties["total_cost"] = int(route.total_cost)

    feature: Dict[str, Any] = {
        "type": "Feature",
        "geometry": {
            "type": actual_geom_type,
            "coordinates": coordinates,
        },
        "properties": properties,
    }

    if as_feature_collection:
        return {
            "type": "FeatureCollection",
            "features": [feature],
        }
    return feature


def route_to_feature_collection(
    route: Any,
    origin_lat: float,
    origin_lon: float,
    resolution_meters: float = 1.0,
    include_waypoints: bool = True,
) -> Dict[str, Any]:
    """Export route as a GeoJSON FeatureCollection including path LineString and start/goal points.

    Parameters:
        route: An xroute.Route instance or sequence of (x, y) coordinates.
        origin_lat: Latitude of grid origin (0, 0) in WGS84 degrees.
        origin_lon: Longitude of grid origin (0, 0) in WGS84 degrees.
        resolution_meters: Scale of grid cell in meters (default: 1.0).
        include_waypoints: If True, adds separate Point features for start and arrival.

    Returns:
        GeoJSON FeatureCollection dictionary.
    """
    line_feature = route_to_geojson(
        route,
        origin_lat,
        origin_lon,
        resolution_meters,
        geometry_type="LineString",
        as_feature_collection=False,
    )
    features = [line_feature]

    coords = line_feature["geometry"]["coordinates"]
    if include_waypoints and len(coords) >= 1:
        start_feat = {
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": coords[0],
            },
            "properties": {
                "role": "start",
                "step_index": 0,
            },
        }
        features.append(start_feat)
        if len(coords) >= 2:
            goal_feat = {
                "type": "Feature",
                "geometry": {
                    "type": "Point",
                    "coordinates": coords[-1],
                },
                "properties": {
                    "role": "goal",
                    "step_index": len(coords) - 1,
                },
            }
            features.append(goal_feat)

    return {
        "type": "FeatureCollection",
        "features": features,
    }


def aisles_to_geojson(
    warehouse_or_bays: Any,
    origin_lat: float,
    origin_lon: float,
    resolution_meters: float = 1.0,
) -> Dict[str, Any]:
    """Export warehouse rack bays as a GeoJSON FeatureCollection of Point features.

    Parameters:
        warehouse_or_bays: A Warehouse instance, warehouse dict, or list of bay items.
        origin_lat: Latitude of grid origin (0, 0) in WGS84 degrees.
        origin_lon: Longitude of grid origin (0, 0) in WGS84 degrees.
        resolution_meters: Metric scale per grid cell (default: 1.0).

    Returns:
        GeoJSON FeatureCollection dictionary with rack bays.
    """
    if hasattr(warehouse_or_bays, "to_dict"):
        data = warehouse_or_bays.to_dict()
        bays = data.get("bays", [])
    elif isinstance(warehouse_or_bays, dict):
        bays = warehouse_or_bays.get("bays", [])
    elif isinstance(warehouse_or_bays, (list, tuple)):
        bays = warehouse_or_bays
    else:
        raise TypeError(f"Unsupported warehouse or bays type: {type(warehouse_or_bays).__name__}")

    features: List[Dict[str, Any]] = []
    for bay in bays:
        x = bay.get("x") if isinstance(bay, dict) else getattr(bay, "x", None)
        y = bay.get("y") if isinstance(bay, dict) else getattr(bay, "y", None)
        if x is None or y is None:
            continue
        lat, lon = grid_to_latlon(float(x), float(y), origin_lat, origin_lon, resolution_meters)
        props: Dict[str, Any] = {
            "x": x,
            "y": y,
        }
        if isinstance(bay, dict):
            for k in ("corsia", "campata", "type", "accessible_sides", "allowed_movements"):
                if k in bay:
                    props[k] = bay[k]
        features.append({
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [lon, lat],
            },
            "properties": props,
        })

    return {
        "type": "FeatureCollection",
        "features": features,
    }
