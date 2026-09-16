param(
    [string]$AxF = (Join-Path $PSScriptRoot '..\build\keil5\Artifacts\smart_gas_monitor.axf'),
    [string]$Output = (Join-Path $PSScriptRoot '..\build\keil5\Artifacts\smart_gas_monitor.bin')
)

$ErrorActionPreference = 'Stop'
$axfPath = [IO.Path]::GetFullPath($AxF)
$outputPath = [IO.Path]::GetFullPath($Output)
if (-not (Test-Path -LiteralPath $axfPath)) {
    throw "AXF file not found: $axfPath. Build the Keil project first."
}
$fromelf = Get-Command fromelf.exe -ErrorAction SilentlyContinue
if (-not $fromelf) {
    throw 'fromelf.exe was not found. Add Keil\ARM\ARMCC\bin to PATH.'
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $outputPath) | Out-Null
& $fromelf.Source --bin --output $outputPath $axfPath
if ($LASTEXITCODE -ne 0) { throw "fromelf failed with exit code $LASTEXITCODE." }
Write-Host "Generated: $outputPath"
