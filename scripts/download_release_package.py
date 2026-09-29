"""Download a published ZIP and verify its published SHA-256 before repackaging."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from build_installer import version

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('platform', choices=('windows', 'macos'))
args = parser.parse_args()
tag = 'v' + version()
name = {'windows': 'Speed-MIDI-Editor-Windows-x64.zip',
        'macos': 'Speed-MIDI-Editor-macOS-Universal.zip'}[args.platform]
release = json.loads(subprocess.check_output(
    ['gh', 'release', 'view', tag, '--json', 'isDraft,assets'], text=True))
if release['isDraft']:
    raise SystemExit('Supplement workflow requires a published release')
subprocess.run(['gh', 'release', 'download', tag, '--pattern', name,
                '--pattern', 'SHA256SUMS.txt', '--dir', 'download'], check=True)
checksums = dict(line.split(maxsplit=1)[::-1] for line in
                 Path('download/SHA256SUMS.txt').read_text().splitlines() if line.strip())
actual = hashlib.sha256((Path('download') / name).read_bytes()).hexdigest()
if checksums.get(name) != actual:
    raise SystemExit('Published ZIP checksum mismatch')
print(f'Verified original {tag} asset {name}: {actual}')
