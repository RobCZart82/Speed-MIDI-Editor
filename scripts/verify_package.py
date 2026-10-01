"""Validate deployed release contents; uses only the Python standard library."""
import argparse
import os
from pathlib import Path
import plistlib
import re
import struct
import subprocess


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def verify(platform, package, qt_version=None):
    source = Path(__file__).resolve().parents[1]
    version = re.search(r'project\(SpeedMIDIEditor VERSION ([\d.]+)',
                        (source / 'CMakeLists.txt').read_text()).group(1)
    for name in ('README.md', 'LICENSE', 'LICENSE.PortMidi', 'THIRD_PARTY_NOTICES.md',
                 'USER_GUIDE_EN.md', 'USER_GUIDE_HU.md', 'README.images'):
        require((package / name).is_file(), f'Missing package document: {name}')
    if platform == 'windows':
        executable = package / 'SpeedMIDIEditor.exe'
        data = executable.read_bytes()
        require(data[:2] == b'MZ', 'Not a Windows executable')
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        require(data[pe:pe+4] == b'PE\0\0', 'Invalid PE header')
        require(struct.unpack_from('<H', data, pe+4)[0] == 0x8664, 'Not an x64 executable')
        require(struct.unpack_from('<H', data, pe+24+68)[0] == 2,
                'Windows application must use GUI subsystem, not a console')
        for name in ('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Xml.dll',
                     'Qt6Network.dll', 'Qt6Svg.dll', 'platforms/qwindows.dll',
                     'msvcp140.dll', 'msvcp140_1.dll', 'vcruntime140.dll', 'vcruntime140_1.dll'):
            require((package / name).is_file(), f'Missing runtime: {name}')
        translations = package / 'translations'
    else:
        app = package / 'Speed MIDI Editor.app'
        contents = app / 'Contents'
        with (contents / 'Info.plist').open('rb') as f:
            info = plistlib.load(f)
        require(info.get('CFBundleShortVersionString') == version, 'Wrong macOS app version')
        require(info.get('CFBundleVersion') == version, 'Wrong macOS bundle version')
        require((contents / 'PlugIns/platforms/libqcocoa.dylib').is_file(), 'Missing Cocoa plugin')
        # Check every deployed Mach-O, not just the executable and QtCore.
        magics = (b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
                  b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
                  b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca')
        count = 0
        for path in contents.rglob('*'):
            if not path.is_file() or path.is_symlink():
                continue
            with path.open('rb') as f:
                magic = f.read(4)
            if magic in magics:
                subprocess.run(['lipo', str(path), '-verify_arch', 'x86_64', 'arm64'], check=True)
                count += 1
        require(count > 1, 'No deployed Universal 2 frameworks found')
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
        require(not (contents / 'MacOS/translations').exists(), 'Translations must not be in the code directory')
        translations = contents / 'Resources/translations'
    for name in ('msg_en.qm', 'msg_de.qm', 'music_en.qm', 'music_de.qm'):
        require((translations / name).is_file(), f'Missing translation: {name}')
    sha = os.environ.get('GITHUB_SHA') or subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    if qt_version is None:
        qt_version = subprocess.check_output(['qmake', '-query', 'QT_VERSION'], text=True).strip()
    require(re.fullmatch(r'\d+\.\d+\.\d+(?:[.-][\w.-]+)?', qt_version), 'Invalid Qt version')
    (package / 'BUILD_INFO.txt').write_text(
        f'Speed MIDI Editor {version}\nCommit: {sha}\nPlatform: {platform}\n'
        f'Qt: {qt_version} (shared libraries)\n'
        'No publisher certificate; macOS is ad-hoc signed and not notarized.\n', encoding='utf-8')
    print(f'Package verified: {platform}, version {version}, commit {sha}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=('windows', 'macos'))
    parser.add_argument('package', type=Path)
    parser.add_argument('--qt-version', help='Qt version used to build the package (defaults to qmake query)')
    args = parser.parse_args()
    verify(args.platform, args.package, args.qt_version)
