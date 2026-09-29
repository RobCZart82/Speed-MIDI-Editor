param([Parameter(Mandatory)][string]$Installer, [Parameter(Mandatory)][string]$Package)
$ErrorActionPreference = 'Stop'
# Run only on disposable CI runners: Inno also registers the uninstall entry.
if ($env:CI -ne 'true') { throw 'Installer smoke test requires a disposable CI runner' }
$Installer = (Resolve-Path -LiteralPath $Installer).Path
$Package = (Resolve-Path -LiteralPath $Package).Path
$target = Join-Path $env:RUNNER_TEMP ('Speed MIDI install test ' + [guid]::NewGuid())
$common = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-')
foreach ($pass in 1..2) {
    # The second pass verifies an in-place reinstall using the stable AppId.
    $process = Start-Process -FilePath $Installer -ArgumentList ($common + ('/DIR="' + $target + '"')) -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
    foreach ($file in Get-ChildItem -LiteralPath $Package -File -Recurse) {
        $relative = $file.FullName.Substring($Package.Length).TrimStart('\')
        $installed = Join-Path $target $relative
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $installed).Hash) {
            throw "Installed file differs: $relative"
        }
    }
}
$userFile = Join-Path $target 'user-song.mid'
Set-Content -LiteralPath $userFile -Value 'User content must survive uninstall'
$process = Start-Process -FilePath (Join-Path $target 'unins000.exe') -ArgumentList $common -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw "Uninstaller failed: $($process.ExitCode)" }
if (Test-Path -LiteralPath (Join-Path $target 'SpeedMIDIEditor.exe')) { throw 'Application was not removed' }
if (-not (Test-Path -LiteralPath $userFile)) { throw 'Uninstaller removed user content' }
Write-Host 'Install, byte-for-byte payload, reinstall and safe uninstall passed.'
