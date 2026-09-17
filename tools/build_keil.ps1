param(
    [ValidateSet('hw', 'soft', 'all')]
    [string]$Mode = 'all',
    [string]$Uv4 = 'C:\Keil514\UV4\UV4.exe'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$project = Join-Path $root 'stm32f103\MDK-ARM\smart_gas_monitor.uvprojx'
$modes = if ($Mode -eq 'all') { @('hw', 'soft') } else { @($Mode) }
foreach ($item in $modes) {
    $target = "smart_gas_monitor_$item"
    $args = @('-r', $project, '-j0', '-t', $target)
    $process = Start-Process -FilePath $Uv4 -ArgumentList $args -WorkingDirectory $root -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw "$target failed: $($process.ExitCode)" }
    & (Join-Path $PSScriptRoot 'keil_collect_artifacts.ps1') -Mode $item
}
