import http.client
import http.server
import json
import subprocess
import sys
import threading
import unittest

from xroute.server import StudioRequestHandler


class QuietHandler(StudioRequestHandler):
    def log_message(self, *args):
        pass


class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.HTTPServer(('127.0.0.1', 0), QuietHandler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def post(self, path, data, raw=False):
        conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=5)
        try:
            conn.request('POST', path, data if raw else json.dumps(data), {'Content-Type': 'application/json'})
            response = conn.getresponse()
            return response.status, json.loads(response.read())
        finally:
            conn.close()

    def test_parse_route_convert_pipeline(self):
        status, result = self.post('/api/parse', {'content': '..\n..', 'cell_width_cm': 125})
        self.assertEqual(status, 200)
        status, route = self.post('/api/route', {'map_content': result['normalized_content'],
            'map_format': result['normalized_format'], 'start': [0, 0], 'goal': [1, 0]})
        self.assertEqual(status, 200)
        self.assertEqual(route['route']['distance_meters'], 1.25)
        status, converted = self.post('/api/convert', {'content': result['normalized_content']})
        self.assertEqual(status, 200)
        status, again = self.post('/api/parse', {'content': converted['xml']})
        self.assertEqual(status, 200)
        self.assertEqual(again['map'], result['map'])

    def test_fleet_respects_hazards(self):
        req = {'map_content': '...\n.1.\n...', 'corsia': 2, 'campata': 1,
               'forklifts': [{'id': 1, 'x': 0, 'y': 0}],
               'blocked_cells': [[1, 0], [0, 1], [2, 1], [1, 2]]}
        self.assertEqual(self.post('/api/fleet', req)[0], 400)
        req['blocked_cells'] = []
        status, response = self.post('/api/fleet', req)
        self.assertEqual(status, 200)
        self.assertEqual(response['best_operator_id'], 1)

    def test_invalid_request_and_recovery(self):
        for body in ['{', '[]', 'null', '[' * 1500 + '0' + ']' * 1500]:
            self.assertEqual(self.post('/api/parse', body, raw=True)[0], 400)
        for data in [{'content': ''}, {'content': 42}, {'content': '..', 'format': 1}]:
            self.assertEqual(self.post('/api/parse', data)[0], 400)
        for override in [{'speed': 0}, {'speed': float('nan')}, {'turn_penalty': -1},
                         {'start': [2**40, 0]}, {'blocked_cells': [[99, 0]]}, {'start': [0.5, 0]}]:
            self.assertEqual(self.post('/api/route', {'map_content': '..', 'start': [0, 0],
                                                    'goal': [1, 0], **override})[0], 400)
        self.assertEqual(self.post('/api/parse', {'content': '..'})[0], 200)

    def test_rejects_invalid_and_oversized_request_lengths(self):
        for length, expected in [('invalid', 400), ('-1', 413), (str(9 * 1024 * 1024), 413)]:
            conn = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=5)
            try:
                conn.request('POST', '/api/parse', '', {'Content-Length': length})
                response = conn.getresponse()
                self.assertEqual(response.status, expected)
                self.assertEqual(json.loads(response.read())['status'], 'error')
            finally:
                conn.close()

    def test_cli_json_output_and_exit_codes(self):
        process = subprocess.run([sys.executable, '-m', 'xroute', 'validate', '-'],
                                 input='..\n..', text=True, capture_output=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(json.loads(process.stdout)['format'], 'ascii')
        process = subprocess.run([sys.executable, '-m', 'xroute', 'validate', '-'],
                                 input='garbage', text=True, capture_output=True)
        self.assertEqual(process.returncode, 1)
        self.assertEqual(json.loads(process.stderr)['status'], 'error')


if __name__ == '__main__':
    unittest.main()
