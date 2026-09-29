"""Wrap an already deployed package without changing any application files."""
import argparse
import hashlib
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def version():
    return re.search(r'project\(SpeedMIDIEditor VERSION ([\d.]+)',
                     (ROOT / 'CMakeLists.txt').read_text()).group(1)


def manifest(folder):
    result = {}
    for path in folder.rglob('*'):
        name = path.relative_to(folder).as_posix()
        if path.is_symlink():
            result[name] = ('link', os.readlink(path))
        elif path.is_file():
            result[name] = ('file', hashlib.sha256(path.read_bytes()).hexdigest())
    return result


def build(platform, package, output):
    package, output = package.resolve(), output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # Preserve the original release's BUILD_INFO and binary provenance.
    info = (package / 'BUILD_INFO.txt').read_text(encoding='utf-8')
    if not info.startswith(f'Speed MIDI Editor {version()}\n'):
        raise SystemExit('Package version does not match installer version')
    if platform == 'windows':
        compiler = shutil.which('ISCC.exe') or str(
            Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) /
            'Inno Setup 6/ISCC.exe')
        subprocess.run([compiler, f'/DPackageDir={package}', f'/DOutputDir={output}',
                        f'/DAppVersion={version()}', str(ROOT / 'packaging/windows.iss')], check=True)
    else:
        dmg = output / f'Speed-MIDI-Editor-{version()}-macOS-Universal.dmg'
        with tempfile.TemporaryDirectory(prefix='speed-midi-dmg-') as temporary:
            stage = Path(temporary) / 'image'
            subprocess.run(['ditto', str(package), str(stage)], check=True)
            (stage / 'Applications').symlink_to('/Applications', target_is_directory=True)
            subprocess.run(['hdiutil', 'create', '-volname', 'Speed MIDI Editor',
                            '-srcfolder', str(stage), '-format', 'UDZO', str(dmg)], check=True)
            subprocess.run(['hdiutil', 'verify', str(dmg)], check=True)
            mounted = plistlib.loads(subprocess.check_output(
                ['hdiutil', 'attach', '-readonly', '-nobrowse', '-plist', str(dmg)]))
            volumes = [Path(e['mount-point']) for e in mounted['system-entities'] if 'mount-point' in e]
            if len(volumes) != 1:
                raise RuntimeError('Expected exactly one mounted DMG volume')
            try:
                actual = manifest(volumes[0])
                for name, digest in manifest(stage).items():
                    if actual.get(name) != digest:
                        raise RuntimeError(f'DMG payload mismatch: {name}')
                subprocess.run(['codesign', '--verify', '--deep', '--strict',
                                str(volumes[0] / 'Speed MIDI Editor.app')], check=True)
            finally:
                subprocess.run(['hdiutil', 'detach', str(volumes[0])], check=True)
        print(f'Created and mounted/verified {dmg.name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=('windows', 'macos'))
    parser.add_argument('package', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.platform, args.package, args.output)
