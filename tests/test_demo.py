import contextlib
import io
import json
import threading
import unittest
import urllib.error
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import patch
from observatory.__main__ import Q_MEV, main, ready, request_when_ready, run


class DemoTest(unittest.TestCase):
    def test_demo_pairs_sweep_and_survival(self):
        def fake_request(base, path, payload=None):
            if path == '/health':
                return {'status': 'ok'}
            if path == '/v1/barrier':
                return {'log10_half_life_s': -payload['q_mev'],
                        'log10_penetrability': -15, 'outer_turning_fm': 26}
            self.assertEqual(payload['log10_half_life_s'], -Q_MEV)
            return {'expected_survivors': [10000], 'sampled_survivors': [10000]}
        with patch('observatory.__main__.ready'), patch('observatory.__main__.request', side_effect=fake_request):
            result = run('http://example', seed=42)
        self.assertEqual(len(result['energy_sweep']), 3)
        self.assertEqual(result['energy_sweep'][1]['q_mev'], Q_MEV)
        self.assertEqual(result['seed'], 42)

    def test_text_health_and_delayed_backends(self):
        calls = {'/v1/barrier': 0, '/v1/ensemble': 0}

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200)
                self.end_headers()
                self.wfile.write(b'ok')

            def do_POST(self):
                payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                calls[self.path] += 1
                if calls[self.path] == 1:
                    self.send_response(502 if self.path == '/v1/barrier' else 503)
                    self.end_headers()
                    return
                data = ({'log10_half_life_s': -payload['q_mev']} if self.path == '/v1/barrier'
                        else {'expected_survivors': [10000], 'sampled_survivors': [10000]})
                self.send_response(200)
                self.end_headers()
                self.wfile.write(json.dumps(data).encode())

            def log_message(self, *_args):
                pass

        with ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                base = f'http://127.0.0.1:{server.server_port}'
                ready(base, timeout=1)
                with patch('observatory.__main__.time.sleep'):
                    result = run(base)
                self.assertEqual(result['seed'], 42)
                self.assertEqual(calls, {'/v1/barrier': 4, '/v1/ensemble': 2})
            finally:
                server.shutdown()
                thread.join()

    def test_retry_is_bounded_and_does_not_retry_validation(self):
        for code in (400, 502):
            error = urllib.error.HTTPError('http://example', code, 'failure', {}, None)
            with patch('observatory.__main__.request', side_effect=error) as mocked:
                with self.assertRaises(urllib.error.HTTPError if code == 400 else RuntimeError):
                    request_when_ready('http://example', '/v1/barrier', {}, timeout=0)
                mocked.assert_called_once()

    def test_cli_rejects_service_bound_violations(self):
        for flag, value in (('--population', '0'), ('--population', '100001'),
                            ('--seed', str(-(2**63) - 1)), ('--seed', str(2**63))):
            with self.subTest(flag=flag, value=value), patch('sys.argv', ['observatory', flag, value]):
                with patch('observatory.__main__.run') as mocked, contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as caught:
                        main()
                self.assertEqual(caught.exception.code, 2)
                mocked.assert_not_called()


if __name__ == '__main__':
    unittest.main()
