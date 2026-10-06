param(
  [Parameter(Mandatory = $true)][string] $Executable,
  [string] $Report = 'windows-runtime-smoke.json',
  [int] $Runs = 5
)
$ErrorActionPreference = 'Stop'
$exePath = (Resolve-Path -LiteralPath $Executable).Path
$payload = Split-Path -Parent $exePath
$originalAppData = $env:APPDATA
$isolated = Join-Path ([System.IO.Path]::GetTempPath()) ('VYNX runtime smoke ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $isolated | Out-Null
$env:APPDATA = $isolated
$result = [ordered]@{ success = $false; samples = @(); executable = $exePath; os = [Environment]::OSVersion.VersionString }
$app = $null
try {
  for ($run = 0; $run -lt $Runs; $run++) {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    # The application under test needs a visible interactive window: Windows
    # excludes hidden windows from Process.MainWindowHandle.
    $app = Start-Process -FilePath $exePath -WorkingDirectory $payload -PassThru
    do {
      Start-Sleep -Milliseconds 50
      $app.Refresh()
    } while (-not $app.HasExited -and $app.MainWindowHandle -eq [IntPtr]::Zero -and $timer.Elapsed.TotalSeconds -lt 20)
    if ($app.HasExited -or $app.MainWindowHandle -eq [IntPtr]::Zero) { throw 'No application window' }
    $startup = $timer.Elapsed.TotalMilliseconds
    $beforeCpu = $app.TotalProcessorTime.TotalSeconds
    Start-Sleep -Seconds 2
    $app.Refresh()
    if ($app.HasExited) { throw 'Application exited during idle sample' }
    $children = @(Get-CimInstance Win32_Process | Where-Object ParentProcessId -eq $app.Id)
    if ($children.Count -ne 0) { throw 'Application created child processes' }
    $modules = @($app.Modules | Where-Object ModuleName -in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll'))
    if ($modules.Count -ne 3 -or @($modules | Where-Object { -not $_.FileName.StartsWith($payload + '\', [StringComparison]::OrdinalIgnoreCase) }).Count) {
      throw 'Qt runtime did not load from the tested payload'
    }
    $result.samples += [ordered]@{
      startupMs = $startup
      privateBytes = $app.PrivateMemorySize64
      workingSetBytes = $app.WorkingSet64
      idleCpuSecondsOver2Seconds = $app.TotalProcessorTime.TotalSeconds - $beforeCpu
      childProcesses = $children.Count
    }
    $app.CloseMainWindow() | Out-Null
    if (-not $app.WaitForExit(10000) -or $app.ExitCode -ne 0) { throw 'Application did not close normally' }
    $app = $null
  }
  $result.success = $true
} catch {
  $result.error = $_.Exception.Message
  throw
} finally {
  if ($app -and -not $app.HasExited) { Stop-Process -Id $app.Id -Force }
  $env:APPDATA = $originalAppData
  $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Report -Encoding UTF8
}
