import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('release_tag', Path(__file__).parents[1] / 'scripts/ensure_release_tag.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseTagTests(unittest.TestCase):
    def test_existing_tags(self):
        commit = 'a' * 40
        for depth in range(4):
            with self.subTest(depth=depth):
                calls = []
                objects = [{'type': 'tag', 'sha': str(i) * 40} for i in range(depth)]
                objects.append({'type': 'commit', 'sha': commit})
                def request(method, path, payload=None):
                    calls.append((method, path))
                    return {'object': objects.pop(0)}
                module.ensure_release_tag('owner/repo', 'v0.1.4', commit, request)
                self.assertEqual(len(calls), depth + 1)
                self.assertTrue(all(method == 'GET' for method, _ in calls))

    def test_missing_and_api_errors(self):
        for status in [404, 401, 403, 429, 500]:
            with self.subTest(status=status):
                created = []
                def request(method, path, payload=None):
                    if method == 'GET':
                        raise module.ApiError(status)
                    created.append(payload)
                if status == 404:
                    module.ensure_release_tag('owner/repo', 'v0.1.4', 'a' * 40, request)
                    self.assertEqual(created, [{'ref': 'refs/tags/v0.1.4', 'sha': 'a' * 40}])
                else:
                    with self.assertRaises(module.ApiError):
                        module.ensure_release_tag('owner/repo', 'v0.1.4', 'a' * 40, request)
                    self.assertEqual(created, [])

    def test_wrong_target_and_cycle(self):
        for obj in [{'type': 'commit', 'sha': 'b' * 40},
                    {'type': 'tree', 'sha': 'a' * 40},
                    {'type': 'tag', 'sha': 'c' * 40}]:
            with self.subTest(obj=obj), self.assertRaises(ValueError):
                module.ensure_release_tag('owner/repo', 'v0.1.4', 'a' * 40,
                                          lambda *args: {'object': obj})


if __name__ == '__main__':
    unittest.main()
