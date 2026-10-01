"""Install and reinstall a PKG on a disposable GitHub macOS runner only."""
import os
from pathlib import Path
import plistlib
import subprocess
import sys

from build_installer import MACOS_APP, MACOS_DOCS, MACOS_PACKAGE_ID, verify_macos_payload, version


def test(package, installer):
    if os.environ.get('GITHUB_ACTIONS') != 'true' or os.environ.get('RUNNER_OS') != 'macOS':
        raise SystemExit('This installation test requires a disposable GitHub macOS runner')
    if any((Path('/') / item).exists() for item in (MACOS_APP, MACOS_DOCS)):
        raise SystemExit('Refusing to overwrite an existing installation')
    for _ in range(2):
        subprocess.run(['sudo', '-n', 'installer', '-pkg', str(installer.resolve()),
                        '-target', '/'], check=True)
        verify_macos_payload(package.resolve(), Path('/'))
        receipt = plistlib.loads(subprocess.check_output(
            ['pkgutil', '--pkg-info-plist', MACOS_PACKAGE_ID]))
        if receipt['pkg-version'] != version():
            raise RuntimeError('Unexpected installer receipt version')
    print('PKG installation, reinstallation, payload and receipt checks passed')


if __name__ == '__main__':
    test(Path(sys.argv[1]), Path(sys.argv[2]))
