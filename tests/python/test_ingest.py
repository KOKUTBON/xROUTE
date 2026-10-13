import json
import unittest

from xroute import Warehouse
from xroute.ingest import import_map, ImportError, MAX_BYTES


class ImportTests(unittest.TestCase):
    def test_equivalent_grid_formats(self):
        sources = ['..#\n.1.\n...', '0,0,x\n0,1,0\n0,0,0',
                   '0;0;x\n0;1;0\n0;0;0', '0\t0\tx\n0\t1\t0\n0\t0\t0',
                   json.dumps([[0, 0, 'x'], [0, 1, 0], [0, 0, 0]])]
        expected = import_map(sources[0]).data
        for source in sources:
            with self.subTest(source=source):
                self.assertEqual(import_map(source).data, expected)

    def test_legacy_csv(self):
        result = import_map('2,3,,\n,,,\n0,0,0,x,\n7,0,21,0,\n')
        bay = result.data['cells'][1]
        self.assertEqual((bay['corsia'], bay['campata'], bay['accessible_sides']), (7, 21, 7))

    def test_coordinate_table(self):
        result = import_map('x,y,type,corsia,campata,movements\n2,1,rack,4,8,\n3,2,oneway,,,left\n')
        self.assertEqual((result.data['rows'], result.data['cols']), (3, 4))
        self.assertEqual(result.data['cells'][1]['movements'], 1)

    def test_xml_nested_dimensions_and_bom(self):
        result = import_map('\ufeff<Magazzino><Deposito numRighe="3" numColonne="4"><Cella x="1" y="1" Tipo="1" Corsia="1" Campata="1"><LatoAccessibile>up</LatoAccessibile></Cella></Deposito></Magazzino>')
        self.assertEqual(result.data['cells'][0]['accessible_sides'], 2)

    def test_roundtrip_keeps_scale_and_empty_masks(self):
        data = {'rows': 3, 'cols': 4, 'cell_width_cm': 123, 'cell_height_cm': 456,
                'cells': [{'x': 1, 'y': 1, 'type': 2, 'corsia': 1, 'campata': 1,
                           'movements': [], 'accessible_sides': []}]}
        original = import_map(json.dumps(data))
        self.assertEqual(import_map(original.to_xml()).data, original.data)
        self.assertEqual(import_map(json.dumps(original.data)).data, original.data)

    def test_invalid_input(self):
        sources = ['', 'hello world', '..\n...', '\x00abc', '{', '<svg/>',
                   '<!DOCTYPE Magazzino><Magazzino/>', '2,3\n,,,\n0,0,0,0',
                   'x,y,type\n1,1,wall,extra', '0,garbage\n0,0',
                   '{"rows": 2, "cols": 2, "grid": [2]}',
                   '{"rows": 999999999, "cols": 999999999}',
                   json.dumps({'rows': 2, 'cols': 2, 'cells': [{'x': 2, 'y': 0}]}),
                   json.dumps({'rows': 2, 'cols': 2, 'cells': [{'x': 0, 'y': 0}, {'x': 0, 'y': 0}]}),
                   json.dumps({'rows': 2, 'cols': 2, 'cells': [{'x': 0, 'y': 0, 'type': 99}]}),
                   '.' * (MAX_BYTES + 1)]
        for source in sources:
            with self.subTest(source=source[:80]):
                with self.assertRaises(ImportError):
                    import_map(source)

    def test_duplicate_bay_and_unknown_direction(self):
        for cells in [
            [{'x': x, 'y': 0, 'type': 1, 'corsia': 1, 'campata': 1} for x in (0, 1)],
            [{'x': 0, 'y': 0, 'movements': ['northwest']}],
            [{'x': 0, 'y': 0, 'movements': {'left': 'false'}}],
        ]:
            with self.assertRaises(ImportError):
                import_map(json.dumps({'rows': 2, 'cols': 2, 'cells': cells}))

    def test_inaccessible_bay_warning(self):
        result = import_map('###\n#1#\n###')
        self.assertTrue(any('no traversable access' in warning for warning in result.warnings))

    def test_native_routing_and_lifecycle(self):
        with Warehouse.load_string('...\n.#.\n...', cell_width_cm=100, cell_height_cm=200) as wh:
            self.assertEqual((wh.rows, wh.cols), (3, 3))
            self.assertEqual(wh.find_path((0, 0), (2, 2)).distance_meters, 6)
            wh.set_dynamic_obstacle(1, 0)
            self.assertNotIn((1, 0), wh.find_path((0, 0), (2, 2)).points)
            self.assertEqual(wh.to_dict()['cells'][1][1]['type'], 4)
        wh.close()

    def test_native_one_way_and_bay(self):
        with Warehouse.load_string('.>.') as wh:
            self.assertEqual(wh.find_path((0, 0), (2, 0)).steps, 3)
            with self.assertRaises(ValueError):
                wh.find_path((2, 0), (0, 0))
        with Warehouse.load_string('...\n.1.\n...') as wh:
            route = wh.route_to_bay((0, 0), 2, 1)
            self.assertEqual(route.steps, 2)
            self.assertNotIn((1, 1), route.points)



class PythonValidationTests(unittest.TestCase):
    def test_rejects_unsafe_native_arguments(self):
        with Warehouse.load_string('..') as wh:
            for start in [(2**40, 0), (False, 0), (0.5, 0)]:
                with self.assertRaises(ValueError):
                    wh.find_path(start, (1, 0))
            for options in [{'speed_mps': float('inf')}, {'speed_mps': -1}, {'turn_penalty_cost': -1}]:
                with self.assertRaises(ValueError):
                    wh.find_path((0, 0), (1, 0), **options)
        with self.assertRaises(ValueError):
            wh.find_path((0, 0), (1, 0))


    def test_arrow_grid_not_mistaken_for_xml(self):
        self.assertEqual(import_map('<v>').format, 'ascii')


class GeoJSONExportTests(unittest.TestCase):
    def test_route_to_geojson_linestring(self):
        from xroute.geojson import route_to_geojson, grid_to_latlon

        with Warehouse.load_string('...\n...\n...') as wh:
            route = wh.find_path((0, 0), (2, 2))
            geo = route_to_geojson(route, origin_lat=45.4642, origin_lon=9.1900, resolution_meters=1.5)

            self.assertEqual(geo["type"], "Feature")
            self.assertEqual(geo["geometry"]["type"], "LineString")
            coords = geo["geometry"]["coordinates"]
            self.assertEqual(len(coords), route.steps)
            # Verify coordinates are [lon, lat] pairs
            for lon, lat in coords:
                self.assertAlmostEqual(lat, 45.4642, delta=0.01)
                self.assertAlmostEqual(lon, 9.1900, delta=0.01)

            props = geo["properties"]
            self.assertEqual(props["steps"], route.steps)
            self.assertEqual(props["origin_lat"], 45.4642)
            self.assertEqual(props["origin_lon"], 9.1900)
            self.assertEqual(props["resolution_meters"], 1.5)
            self.assertEqual(props["distance_meters"], route.distance_meters)
            self.assertEqual(props["turns"], route.turns)

    def test_route_to_geojson_multipoint_and_collection(self):
        from xroute.geojson import route_to_geojson, route_to_feature_collection

        points = [(0, 0), (1, 0), (2, 0)]
        # Test MultiPoint geometry
        geo_mp = route_to_geojson(points, origin_lat=45.0, origin_lon=9.0, geometry_type="MultiPoint")
        self.assertEqual(geo_mp["type"], "Feature")
        self.assertEqual(geo_mp["geometry"]["type"], "MultiPoint")
        self.assertEqual(len(geo_mp["geometry"]["coordinates"]), 3)

        # Test as_feature_collection=True
        geo_fc = route_to_geojson(points, origin_lat=45.0, origin_lon=9.0, as_feature_collection=True)
        self.assertEqual(geo_fc["type"], "FeatureCollection")
        self.assertEqual(len(geo_fc["features"]), 1)
        self.assertEqual(geo_fc["features"][0]["geometry"]["type"], "LineString")

        # Test route_to_feature_collection with waypoints
        fc = route_to_feature_collection(points, origin_lat=45.0, origin_lon=9.0, include_waypoints=True)
        self.assertEqual(fc["type"], "FeatureCollection")
        self.assertEqual(len(fc["features"]), 3)  # LineString, start point, goal point
        self.assertEqual(fc["features"][0]["geometry"]["type"], "LineString")
        self.assertEqual(fc["features"][1]["properties"]["role"], "start")
        self.assertEqual(fc["features"][2]["properties"]["role"], "goal")

    def test_route_instance_and_warehouse_to_geojson(self):
        with Warehouse.load_string('...\n.1.\n...') as wh:
            route = wh.route_to_bay((0, 0), 2, 1)
            # Route.to_geojson helper method
            geo_route = route.to_geojson(origin_lat=45.0, origin_lon=9.0)
            self.assertEqual(geo_route["type"], "Feature")
            self.assertEqual(geo_route["geometry"]["type"], "LineString")

            # Warehouse.to_geojson helper method
            geo_wh = wh.to_geojson(origin_lat=45.0, origin_lon=9.0)
            self.assertEqual(geo_wh["type"], "FeatureCollection")
            self.assertEqual(len(geo_wh["features"]), 1)
            bay_feat = geo_wh["features"][0]
            self.assertEqual(bay_feat["geometry"]["type"], "Point")
            self.assertEqual(bay_feat["properties"]["corsia"], 2)
            self.assertEqual(bay_feat["properties"]["campata"], 1)

    def test_geojson_validation_errors(self):
        from xroute.geojson import route_to_geojson, grid_to_latlon

        # Out-of-range latitude
        with self.assertRaises(ValueError):
            grid_to_latlon(0, 0, origin_lat=95.0, origin_lon=0.0)
        with self.assertRaises(ValueError):
            grid_to_latlon(0, 0, origin_lat=-95.0, origin_lon=0.0)

        # Out-of-range longitude
        with self.assertRaises(ValueError):
            grid_to_latlon(0, 0, origin_lat=0.0, origin_lon=190.0)

        # Non-positive resolution
        with self.assertRaises(ValueError):
            grid_to_latlon(0, 0, origin_lat=0.0, origin_lon=0.0, resolution_meters=0.0)

        # Invalid geometry type
        with self.assertRaises(ValueError):
            route_to_geojson([(0, 0)], origin_lat=0.0, origin_lon=0.0, geometry_type="Polygon")

        # Invalid coordinate element
        with self.assertRaises(ValueError):
            route_to_geojson(["not-a-point"], origin_lat=0.0, origin_lon=0.0)


if __name__ == '__main__':
    unittest.main()
