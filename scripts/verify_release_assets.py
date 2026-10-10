"""Check downloaded release hashes, ZIP integrity and exact build provenance."""
import argparse
import hashlib
from pathlib import Path
import re
import zipfile


def verify(folder, version, commit):
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid release version')
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('Expected a full release commit SHA')
    archives = {
        'Speed-MIDI-Editor-Windows-x64.zip': ('windows', 'BUILD_INFO.txt'),
        'Speed-MIDI-Editor-macOS-Universal.zip':
            ('macos', 'Speed MIDI Editor/BUILD_INFO.txt'),
    }
    expected = set(archives) | {
        f'Speed-MIDI-Editor-{version}-Windows-x64-Setup.exe',
        f'Speed-MIDI-Editor-{version}-macOS-Universal.pkg',
    }
    checksums = {}
    for line in (folder / 'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
        match = re.fullmatch(r'([0-9a-fA-F]{64}) [ *](.+)', line)
        if not match:
            raise ValueError('Malformed SHA256SUMS entry')
        digest, name = match.groups()
        if name not in expected or name in checksums:
            raise ValueError(f'Unexpected or duplicate checksum: {name}')
        checksums[name] = digest.lower()
    if set(checksums) != expected:
        raise ValueError('Checksum manifest must list all four release packages')
    for name in sorted(expected):
        path = folder / name
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f'Missing or empty release package: {name}')
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        if digest.hexdigest() != checksums[name]:
            raise ValueError(f'Checksum mismatch: {name}')
    for name, (platform, metadata) in archives.items():
        with zipfile.ZipFile(folder / name) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise ValueError(f'Duplicate ZIP member: {name}')
            for member in names:
                parts = member.replace('\\', '/').split('/')
                if member.startswith(('/', '\\')) or '..' in parts or ':' in parts[0]:
                    raise ValueError(f'Unsafe ZIP member: {member}')
            damaged = archive.testzip()
            if damaged:
                raise ValueError(f'ZIP CRC failure: {damaged}')
            info = archive.read(metadata).decode('utf-8').splitlines()
            prefix = [f'Speed MIDI Editor {version}', f'Commit: {commit}',
                      f'Platform: {platform}']
            if info[:3] != prefix or len(info) < 4 or not re.fullmatch(
                    r'Qt: \d+\.\d+\.\d+(?:[.-][\w.-]+)? \(shared libraries\)', info[3]):
                raise ValueError(f'Wrong build provenance: {name}')
    print(f'Verified all four {version} packages from {commit}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    try:
        verify(args.folder, args.version, args.commit)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Release verification failed: {error}\n')
