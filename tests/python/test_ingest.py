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


if __name__ == '__main__':
    unittest.main()
