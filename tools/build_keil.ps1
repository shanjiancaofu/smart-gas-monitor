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
    # UV4 的退出码：0 无警告无错误，1 有警告但构建成功，2 有错误，3 致命。
    # 1 不能当失败处理：固件是好的，而一旦在这里抛异常，下面的产物收集就不跑，
    # Artifacts 里留着上一版 HEX，Proteus 会静默地继续跑旧固件——「编译过了但
    # 仿真里还是旧行为」很难查，所以这里只警告不中断。
    if ($process.ExitCode -ge 2) { throw "$target failed: $($process.ExitCode)" }
    if ($process.ExitCode -eq 1) { Write-Warning "${target}: built with warnings (UV4 exit 1)" }
    & (Join-Path $PSScriptRoot 'keil_collect_artifacts.ps1') -Mode $item
}
