param(
  [Parameter(Mandatory = $true)]
  [string] $ArtifactDirectory
)

$ErrorActionPreference = 'Stop'
$report = [ordered]@{
  runner = $env:ImageOS
  os = (Get-CimInstance Win32_OperatingSystem).Caption
  version = (Get-CimInstance Win32_OperatingSystem).Version
  architecture = $env:PROCESSOR_ARCHITECTURE
  checks = @()
  success = $false
}

function Assert-Check {
  param([string] $Name, [bool] $Passed)
  $script:report.checks += [ordered]@{ name = $Name; passed = $Passed }
  Write-Host "$Name`: $(if ($Passed) { 'PASS' } else { 'FAIL' })"
  if (-not $Passed) { throw "Failed: $Name" }
}

$app = $null
$failure = $null
try {
  $artifactPath = (Resolve-Path -LiteralPath $ArtifactDirectory).Path
  $installDir = Join-Path $env:LOCALAPPDATA 'Programs\VYNX QR'
  $uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\VYNX QR'
  $settingsKey = 'HKCU:\Software\VYNX QR'
  $startMenu = Join-Path $env:APPDATA 'Microsoft\Windows\Start Menu\Programs\VYNX QR'

  Assert-Check 'Application absent before install' (-not (Test-Path -LiteralPath $installDir))
  Assert-Check 'Uninstall registration absent before install' (-not (Test-Path -LiteralPath $uninstallKey))

  $installers = @(Get-ChildItem -LiteralPath $artifactPath -Filter 'VYNX-QR-Setup-x64-*.exe' -File)
  $checksums = Join-Path $artifactPath 'SHA256SUMS.txt'
  Assert-Check 'Exactly one installer and checksum manifest supplied by Actions' ($installers.Count -eq 1 -and (Test-Path -LiteralPath $checksums))
  $installer = $installers[0]
  $actualHash = (Get-FileHash -LiteralPath $installer.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  $expectedLine = Get-Content -LiteralPath $checksums | Where-Object { $_.EndsWith($installer.Name) }
  Assert-Check 'Installer SHA256 matches the artifact manifest' ($expectedLine -eq "$actualHash  $($installer.Name)")
  $report.installer = $installer.Name
  $report.sha256 = $actualHash

  $install = Start-Process -FilePath $installer.FullName -ArgumentList '/S' -WindowStyle Hidden -PassThru -Wait
  Assert-Check 'Per-user silent installer exits successfully' ($install.ExitCode -eq 0)

  $exe = Join-Path $installDir 'VYNX QR.exe'
  Assert-Check 'Application installed in the current user profile' (Test-Path -LiteralPath $exe)
  foreach ($relativePath in @(
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll',
    'platforms\qwindows.dll', 'styles\qmodernwindowsstyle.dll', 'Uninstall.exe',
    'LICENSE', 'THIRD_PARTY_LICENSES.md', 'licenses\qt\LGPL-3.0.txt',
    'licenses\qt\GPL-3.0.txt', 'licenses\qt\README.md',
    'licenses\qt\qtbase-everywhere-src-6.8.3.tar.xz'
  )) {
    Assert-Check "Installed payload: $relativePath" (Test-Path -LiteralPath (Join-Path $installDir $relativePath))
  }

  Assert-Check 'Apps and Features registration created for current user' (Test-Path -LiteralPath $uninstallKey)
  $registration = Get-ItemProperty -LiteralPath $uninstallKey
  Assert-Check 'Registration points to per-user install path' ($registration.InstallLocation -eq $installDir)
  Assert-Check 'Settings registration created for current user' (Test-Path -LiteralPath $settingsKey)

  $shortcut = Join-Path $startMenu 'VYNX QR.lnk'
  Assert-Check 'Start Menu shortcut created' (Test-Path -LiteralPath $shortcut)
  $shell = New-Object -ComObject WScript.Shell
  $shortcutTarget = $shell.CreateShortcut($shortcut).TargetPath
  Assert-Check 'Start Menu shortcut targets the installed executable' ($shortcutTarget -eq $exe)

  $applicationStart = Get-Date
  $app = Start-Process -FilePath $exe -WorkingDirectory $installDir -PassThru
  $startupWait = [System.Diagnostics.Stopwatch]::StartNew()
  $deadline = (Get-Date).AddSeconds(20)
  do {
    Start-Sleep -Milliseconds 250
    $app.Refresh()
  } while (-not $app.HasExited -and ($app.MainWindowHandle -eq [IntPtr]::Zero) -and (Get-Date) -lt $deadline)
  $startupWait.Stop()
  $app.Refresh()
  $report.application = [ordered]@{
    exited = $app.HasExited
    exitCode = if ($app.HasExited) { $app.ExitCode } else { $null }
    mainWindowHandle = $app.MainWindowHandle
    mainWindowTitle = $app.MainWindowTitle
    sessionId = $app.SessionId
    elapsedSeconds = $startupWait.Elapsed.TotalSeconds
    responding = $app.Responding
    qtModules = @($app.Modules | Where-Object { $_.ModuleName -like 'Qt6*.dll' } | ForEach-Object { $_.FileName })
  }
  $report.startupEvents = @(
    Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $applicationStart } -ErrorAction SilentlyContinue |
      Where-Object { $_.Level -le 2 -and ($_.Message -match 'VYNX QR|VYNX_QR|Qt6|SideBySide|Application Error') } |
      Select-Object -First 10 TimeCreated, ProviderName, Id, Message
  )
  Assert-Check 'Installed application creates a window' (-not $app.HasExited -and $app.MainWindowHandle -ne 0)
  Start-Sleep -Seconds 3
  $app.Refresh()
  Assert-Check 'Application stays running after startup' (-not $app.HasExited)
  Assert-Check 'Application has no child processes' (@(Get-CimInstance Win32_Process | Where-Object { $_.ParentProcessId -eq $app.Id }).Count -eq 0)

  $qtModules = @($app.Modules | Where-Object { $_.ModuleName -in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll') })
  $report.qtModules = @($qtModules | ForEach-Object { $_.FileName })
  Assert-Check 'Qt runtime modules load from installed directory' (
    $qtModules.Count -eq 3 -and @($qtModules | Where-Object {
      -not $_.FileName.StartsWith($installDir, [StringComparison]::OrdinalIgnoreCase)
    }).Count -eq 0
  )

  $app.CloseMainWindow() | Out-Null
  Assert-Check 'Application closes normally' ($app.WaitForExit(10000))
  $app = $null

  $uninstall = Start-Process -FilePath (Join-Path $installDir 'Uninstall.exe') -ArgumentList '/S' -WindowStyle Hidden -PassThru -Wait
  Assert-Check 'Per-user uninstaller exits successfully' ($uninstall.ExitCode -eq 0)
  $deadline = (Get-Date).AddSeconds(20)
  while ((Test-Path -LiteralPath $installDir) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
  Assert-Check 'Installation directory removed' (-not (Test-Path -LiteralPath $installDir))
  Assert-Check 'Start Menu shortcut removed' (-not (Test-Path -LiteralPath $startMenu))
  Assert-Check 'Apps and Features registration removed' (-not (Test-Path -LiteralPath $uninstallKey))
  Assert-Check 'Install settings registration removed' (-not (Test-Path -LiteralPath $settingsKey))
  $report.success = $true
} catch {
  $failure = $_.Exception.Message
  $report.error = $failure
} finally {
  if ($app -and -not $app.HasExited) { Stop-Process -Id $app.Id -Force }
  $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath 'installer-smoke-report.json' -Encoding UTF8
}

if (-not $report.success) { throw "Installer smoke failed: $failure" }
