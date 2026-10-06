param([Parameter(Mandatory = $true)][string] $ArtifactDirectory)
$ErrorActionPreference = 'Stop'
$artifactPath = (Resolve-Path -LiteralPath $ArtifactDirectory).Path
$archives = @(Get-ChildItem -LiteralPath $artifactPath -Filter 'VYNX-QR-Portable-x64-*.zip' -File)
if ($archives.Count -ne 1) { throw 'Expected exactly one portable artifact' }
$archive = $archives[0]
$digest = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$line = "$digest  $($archive.Name)"
if ($line -notin (Get-Content -LiteralPath (Join-Path $artifactPath 'SHA256SUMS.txt'))) { throw 'Portable artifact hash mismatch' }
$payload = Join-Path ([System.IO.Path]::GetTempPath()) ('VYNX portable artifact ' + [guid]::NewGuid())
Expand-Archive -LiteralPath $archive.FullName -DestinationPath $payload
$rustRoot = Join-Path $payload 'licenses\rust'
python scripts/verify-rust-notices.py $rustRoot --target x86_64-pc-windows-msvc --lockfile bridge/Cargo.lock
if ($LASTEXITCODE -ne 0) { throw 'Portable Rust notice verification failed' }
$source = Join-Path $payload 'licenses\qt\qtbase-everywhere-src-6.8.3.tar.xz'
if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne '56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80') { throw 'Portable Qt source hash mismatch' }
foreach ($relative in @('LICENSE', 'THIRD_PARTY_LICENSES.md', 'licenses\qt\LGPL-3.0.txt', 'licenses\qt\GPL-3.0.txt', 'licenses\qt\README.md')) {
  if (-not (Test-Path -LiteralPath (Join-Path $payload $relative))) { throw "Missing portable notice: $relative" }
}
$exe = Join-Path $payload 'VYNX QR.exe'
& "$PSScriptRoot/windows-runtime-smoke.ps1" -Executable $exe -Report portable-runtime-smoke.json
if (Test-Path -LiteralPath (Join-Path $payload 'settings.json')) { throw 'Preferences leaked into portable payload' }
[ordered]@{ success = $true; artifact = $archive.Name; sha256 = $digest; payloadPathIncludesSpaces = $true } |
  ConvertTo-Json | Set-Content -LiteralPath portable-smoke-report.json -Encoding UTF8
