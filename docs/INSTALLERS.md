# Installer packaging

The platform workflows first build, test, deploy and verify the application.
They then produce both the portable ZIP and the installer from that same deployed
directory. Release drafts contain all four downloads and SHA-256 checksums.

## Windows

`python scripts/build_installer.py windows dist/windows dist` uses Inno Setup 6
(preinstalled on the GitHub Windows 2022 runner). Locally, install Inno Setup 6.3+
and put ISCC.exe on PATH or use its standard installation directory.

The installer targets x64 Windows 10 1809+ and Windows 11, installs under the
current user's LocalAppData/Programs, and includes all app-local Qt/MSVC libraries.
It provides a Start menu shortcut, optional desktop shortcut, and Windows uninstall
registration. Its stable AppId supports upgrades. The application's existing mutex
prevents installing or uninstalling while the editor is running. No file associations,
drivers, system runtimes, MIDI device settings or user registry preferences are modified.
Uninstall removes installed files, but does not recursively erase user-created songs.

CI silently installs into a temporary path containing spaces, compares every payload
file byte-for-byte, reinstalls, then uninstalls and checks that a user-created file
survives. Run `scripts/test_windows_installer.ps1` only on disposable CI runners;
it also creates normal per-user uninstall registration and shortcuts.

## macOS

`python3 scripts/build_installer.py macos "dist/Speed MIDI Editor" dist` uses Apple's
`pkgbuild`, `pkgutil` and `ditto`. It creates a PKG that installs the Universal 2 app
into `/Applications/Speed MIDI Editor.app`, with documentation and BUILD_INFO under
`/Library/Application Support/Speed MIDI Editor`. The installer needs administrator
approval. Bundle relocation is disabled so an existing build or download copy is
never selected as the installation destination.
The app bundle is replaced on upgrade or same-version reinstall to remove obsolete
resources and preserve its sealed code signature. Songs and preferences outside
the application bundle are preserved. The script expands the package,
compares every payload file and symlink target with the deployed source, and verifies
the app's code signature. Apple Silicon and Intel are included; macOS 13+ remains
the deployment target. Future builds and releases produce PKG only, with no DMG.

CI uses `scripts/test_macos_installer.py` on a disposable GitHub runner to install
and reinstall the PKG, check removal of an obsolete app resource, compare the
installed payload, verify its signature and check the version in the installer
receipt. It refuses to overwrite a pre-existing app.
No application files, MIDI preferences, or user songs are changed by packaging.

## Supplementing an existing release

Run **Add installers to published release** from main. It derives the release tag
from CMake, downloads that release's original ZIPs and validates SHA256SUMS.txt.
It wraps those binaries without rebuilding them or changing BUILD_INFO.txt.
Only after both installer checks pass does it append the EXE, PKG, a separate
INSTALLER-SHA256SUMS.txt and INSTALLER-BUILD-INFO.txt to the published release,
then append installation instructions. The original tag, ZIPs and checksums stay
unchanged. Upload deliberately does not use `--clobber`; a rerun with existing
installer assets fails instead of silently replacing a public download. Inspect
any partial upload before recovery; do not delete already published assets blindly.

Installer packaging source is identified in INSTALLER-BUILD-INFO.txt, separately
from the original tagged application source. Neither platform has a publisher
certificate configured; the Mac app is ad-hoc signed and not notarized. Creating
an installer does not remove operating-system unknown-publisher warnings.


## Verifying downloaded release assets

Download the four packages and `SHA256SUMS.txt` into one folder, then run:

```sh
python3 scripts/verify_release_assets.py downloads --version 0.1.5 \
  --commit 905c024f5bf5258c1772861751df9b5cf4f3f7f4
```

Use the full commit targeted by that release's immutable tag, not the latest
`main` commit. The checker validates the complete checksum manifest, hashes all
four assets, tests ZIP CRCs, and checks both archives' BUILD_INFO version, commit,
platform and Qt metadata without extracting or launching the application. It
rejects duplicate or unsafe ZIP member names. It does not authenticate the
checksum publisher or replace executable/signature, installer or manual MIDI QA.
The release workflow runs this gate before creating a tag or uploading a draft.
