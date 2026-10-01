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
MACOS_PACKAGE_ID = 'org.speedymidieditor.SpeedMIDIEditor.installer'
MACOS_APP = Path('Applications/Speed MIDI Editor.app')
MACOS_DOCS = Path('Library/Application Support/Speed MIDI Editor')


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


def macos_payload_manifest(package):
    result = {}
    for name, digest in manifest(package).items():
        relative = Path(name)
        destination = (Path('Applications') if relative.parts[0] == 'Speed MIDI Editor.app'
                       else MACOS_DOCS) / relative
        result[destination.as_posix()] = digest
    return result


def verify_macos_payload(package, installed_root):
    actual = {}
    for relative in (MACOS_APP, MACOS_DOCS):
        for name, digest in manifest(installed_root / relative).items():
            actual[(relative / name).as_posix()] = digest
    expected = macos_payload_manifest(package)
    if actual != expected:
        different = sorted(name for name in actual.keys() | expected.keys()
                           if actual.get(name) != expected.get(name))
        raise RuntimeError(f'PKG payload mismatch: {different[:10]}')
    subprocess.run(['codesign', '--verify', '--deep', '--strict',
                    str(installed_root / MACOS_APP)], check=True)


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
        pkg = output / f'Speed-MIDI-Editor-{version()}-macOS-Universal.pkg'
        with tempfile.TemporaryDirectory(prefix='speed-midi-pkg-') as temporary:
            stage = Path(temporary) / 'root'
            (stage / MACOS_APP.parent).mkdir(parents=True)
            (stage / MACOS_DOCS).mkdir(parents=True)
            for entry in package.iterdir():
                target = (stage / MACOS_APP if entry.name == 'Speed MIDI Editor.app'
                          else stage / MACOS_DOCS / entry.name)
                subprocess.run(['ditto', str(entry), str(target)], check=True)
            if not (stage / MACOS_APP / 'Contents/Info.plist').is_file():
                raise RuntimeError('Missing application bundle')
            components = Path(temporary) / 'components.plist'
            subprocess.run(['pkgbuild', '--analyze', '--root', str(stage), str(components)], check=True)
            settings = plistlib.loads(components.read_bytes())
            for bundle in settings:
                # Always update /Applications, never a build/download copy found by Installer.
                bundle['BundleIsRelocatable'] = False
                if bundle['RootRelativeBundlePath'] == MACOS_APP.as_posix():
                    # Remove obsolete sealed resources on upgrades and same-version reinstalls.
                    # pkgbuild's documented "upgrade" atomically replaces the bundle;
                    # "update" merges it, while "replace" is not a supported action.
                    bundle['BundleOverwriteAction'] = 'upgrade'
                    bundle['BundleIsVersionChecked'] = True
            components.write_bytes(plistlib.dumps(settings))
            subprocess.run(['pkgbuild', '--root', str(stage), '--component-plist', str(components),
                            '--identifier', MACOS_PACKAGE_ID, '--version', version(),
                            '--install-location', '/', '--ownership', 'recommended', str(pkg)], check=True)
            expanded = Path(temporary) / 'expanded'
            subprocess.run(['pkgutil', '--expand-full', str(pkg), str(expanded)], check=True)
            verify_macos_payload(package, expanded / 'Payload')
        print(f'Created and payload-verified {pkg.name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=('windows', 'macos'))
    parser.add_argument('package', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.platform, args.package, args.output)
