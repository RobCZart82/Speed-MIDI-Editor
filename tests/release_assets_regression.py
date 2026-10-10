import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location(
    'release_assets', Path(__file__).parents[1] / 'scripts/verify_release_assets.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseAssetsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.commit = 'a' * 40
        self.windows = self.folder / 'Speed-MIDI-Editor-Windows-x64.zip'
        for name, platform, root in [
            (self.windows.name, 'windows', ''),
            ('Speed-MIDI-Editor-macOS-Universal.zip', 'macos', 'Speed MIDI Editor/'),
        ]:
            with zipfile.ZipFile(self.folder / name, 'w') as archive:
                archive.writestr(root + 'BUILD_INFO.txt',
                    f'Speed MIDI Editor 0.1.5\nCommit: {self.commit}\n'
                    f'Platform: {platform}\nQt: 6.10.3 (shared libraries)\n')
        for name in ['Speed-MIDI-Editor-0.1.5-Windows-x64-Setup.exe',
                     'Speed-MIDI-Editor-0.1.5-macOS-Universal.pkg']:
            (self.folder / name).write_bytes(b'installer test fixture')
        self.manifest()

    def manifest(self):
        entries = [f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n'
                   for p in sorted(self.folder.iterdir()) if p.name != 'SHA256SUMS.txt']
        (self.folder / 'SHA256SUMS.txt').write_text(''.join(entries))

    def verify(self, commit=None):
        module.verify(self.folder, '0.1.5', commit or self.commit)

    def test_valid_release(self):
        self.verify()

    def test_modified_asset(self):
        self.windows.write_bytes(self.windows.read_bytes() + b'changed')
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            self.verify()

    def test_wrong_commit(self):
        with self.assertRaisesRegex(ValueError, 'Wrong build provenance'):
            self.verify('b' * 40)

    def test_incomplete_duplicate_and_unsafe_manifest(self):
        path = self.folder / 'SHA256SUMS.txt'
        original = path.read_text()
        for replacement in [original.splitlines()[0] + '\n',
                            original + original.splitlines()[0] + '\n',
                            original + '0' * 64 + '  ../outside.zip\n']:
            with self.subTest(replacement=replacement):
                path.write_text(replacement)
                with self.assertRaises(ValueError):
                    self.verify()

    def test_unsafe_zip_member(self):
        with zipfile.ZipFile(self.windows, 'a') as archive:
            archive.writestr('../outside', 'bad')
        self.manifest()
        with self.assertRaisesRegex(ValueError, 'Unsafe ZIP member'):
            self.verify()

    def test_crc_damage_with_matching_manifest(self):
        data = self.windows.read_bytes()
        self.windows.write_bytes(data.replace(b'Speed MIDI Editor 0.1.5',
                                             b'Speed MIDI Editor 0.1.6', 1))
        self.manifest()
        with self.assertRaisesRegex(ValueError, 'ZIP CRC failure'):
            self.verify()

    def test_invalid_inputs(self):
        for version, commit in [('v0.1.5', self.commit), ('0.1.5', 'short')]:
            with self.subTest(version=version, commit=commit), self.assertRaises(ValueError):
                module.verify(self.folder, version, commit)


if __name__ == '__main__':
    unittest.main()
