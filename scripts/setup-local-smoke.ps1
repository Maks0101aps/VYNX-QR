param([Parameter(Mandatory = $true)][string] $Installer)
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$evidence = Join-Path $workspace ('build\setup-smoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
$destination = Join-Path $evidence ('QR ' + [char]0x442 + [char]0x435 + [char]0x441 + [char]0x442 + ' folder')
$registration = 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\VYNX QR'
$settings = 'HKCU\Software\VYNX QR'
$startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\VYNX QR'
$backups = @{}
$shortcuts = @{}
$report = [ordered]@{ success = $false; installer = (Resolve-Path $Installer).Path; checks = @() }
function Check([string] $Name, [bool] $Passed) {
  $script:report.checks += [ordered]@{ name = $Name; passed = $Passed }
  if (-not $Passed) { throw $Name }
  Write-Host "PASS: $Name"
}
foreach ($key in @($registration, $settings)) {
  $file = Join-Path $evidence (($backups.Count).ToString() + '.reg')
  if (Test-Path ($key.Replace('HKCU\', 'HKCU:\'))) {
    & reg.exe export $key $file /y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Registry backup failed' }
    $backups[$key] = $file
  }
}
foreach ($name in @('VYNX QR.lnk', 'Uninstall.lnk')) {
  $path = Join-Path $startMenu $name
  if (Test-Path -LiteralPath $path) { $shortcuts[$path] = [IO.File]::ReadAllBytes($path) }
}
$process = $null
try {
  $foreignDirectory = Join-Path $evidence 'foreign'
  New-Item -ItemType Directory -Path $foreignDirectory | Out-Null
  $foreignFile = Join-Path $foreignDirectory 'keep.txt'
  [IO.File]::WriteAllText($foreignFile, 'keep')
  $result = Start-Process -FilePath $report.installer -ArgumentList "/S /D=$foreignDirectory" -WindowStyle Hidden -PassThru -Wait
  Check 'Silent installer rejects an unrelated nonempty directory' ($result.ExitCode -ne 0 -and [IO.File]::ReadAllText($foreignFile) -eq 'keep')
  New-Item -ItemType Directory -Path $destination | Out-Null
  [IO.File]::WriteAllText((Join-Path $destination '.vynx-qr-install'), 'VYNX QR Windows setup')
  [IO.File]::WriteAllText((Join-Path $destination 'LICENSE'), 'partial extraction')
  $result = Start-Process -FilePath $report.installer -ArgumentList "/S /D=$destination" -WindowStyle Hidden -PassThru -Wait
  Check 'Bootstrap forwards silent /D with spaces and Unicode' ($result.ExitCode -eq 0 -and (Test-Path -LiteralPath (Join-Path $destination 'VYNX QR.exe')))
  Check 'Owned partial installation can be resumed' ((Get-Item -LiteralPath (Join-Path $destination 'LICENSE')).Length -gt 1000)
  $entry = Get-ItemProperty ($registration.Replace('HKCU\', 'HKCU:\'))
  Check 'Registration points to selected destination' ($entry.InstallLocation -eq $destination)
  $shell = New-Object -ComObject WScript.Shell
  Check 'Start Menu targets installed application' ($shell.CreateShortcut((Join-Path $startMenu 'VYNX QR.lnk')).TargetPath -eq (Join-Path $destination 'VYNX QR.exe'))
  foreach ($name in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll', 'Uninstall.exe', 'LICENSE', 'licenses\rust\manifest.json')) {
    Check "Installed payload $name" (Test-Path -LiteralPath (Join-Path $destination $name))
  }
  $process = Start-Process -FilePath (Join-Path $destination 'VYNX QR.exe') -WindowStyle Minimized -PassThru
  $deadline = [DateTime]::UtcNow.AddSeconds(20)
  do { Start-Sleep -Milliseconds 100; $process.Refresh() } while (-not $process.HasExited -and $process.MainWindowHandle -eq 0 -and [DateTime]::UtcNow -lt $deadline)
  Check 'Installed application opens a real window' (-not $process.HasExited -and $process.MainWindowHandle -ne 0)
  $process.CloseMainWindow() | Out-Null
  Check 'Installed application closes normally' ($process.WaitForExit(10000) -and $process.ExitCode -eq 0)
  $process = $null
  $kept = Join-Path $destination 'my-file.txt'
  [IO.File]::WriteAllText($kept, 'preserve unrelated content')
  $uninstall = Join-Path $destination 'Uninstall.exe'
  $result = Start-Process -FilePath $uninstall -ArgumentList '/S' -WindowStyle Hidden -PassThru -Wait
  $deadline = [DateTime]::UtcNow.AddSeconds(20)
  while ((Test-Path -LiteralPath $uninstall) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
  Check 'Uninstall removes application files' (-not (Test-Path -LiteralPath (Join-Path $destination 'VYNX QR.exe')) -and -not (Test-Path -LiteralPath $uninstall))
  Check 'Uninstall preserves unrelated files' ([IO.File]::ReadAllText($kept) -eq 'preserve unrelated content')
  Check 'Uninstall removes test registration' (-not (Test-Path ($registration.Replace('HKCU\', 'HKCU:\'))))
  $report.success = $true
} finally {
  try {
  if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
  $remainingUninstaller = Join-Path $destination 'Uninstall.exe'
  if (Test-Path -LiteralPath $remainingUninstaller) {
    Start-Process -FilePath $remainingUninstaller -ArgumentList '/S' -WindowStyle Hidden -Wait
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ((Test-Path -LiteralPath $remainingUninstaller) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
  }
  } finally {
  foreach ($key in $backups.Keys) {
    & reg.exe import $backups[$key] | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Failed to restore $key; backup is $($backups[$key])" }
  }
  foreach ($path in $shortcuts.Keys) {
    [IO.Directory]::CreateDirectory($startMenu) | Out-Null
    [IO.File]::WriteAllBytes($path, $shortcuts[$path])
  }
  $report | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $evidence 'report.json') -Encoding UTF8
  Write-Host "Evidence: $evidence"
  }
}
