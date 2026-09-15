param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Debug',
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$make = Get-ChildItem 'E:/STM32CubeIDE' -Recurse -Filter make.exe | Select-Object -First 1
$gcc = Get-ChildItem 'E:/STM32CubeIDE' -Recurse -Filter arm-none-eabi-gcc.exe | Select-Object -First 1
$env:PATH = $gcc.DirectoryName + ';' + $env:PATH
$out = Join-Path $repo ('build/' + $Configuration.ToLowerInvariant())
New-Item -ItemType Directory -Force $out | Out-Null
$debug = if ($Configuration -eq 'Debug') { '1' } else { '0' }
$opt = if ($Configuration -eq 'Debug') { '-Og' } else { '-Os' }
& $make.FullName -C (Join-Path $repo 'stm32f103/cubemx') `
    "BUILD_DIR=../../build/$($Configuration.ToLowerInvariant())" `
    "DEBUG=$debug" "OPT=$opt" "-j$Jobs"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Built $Configuration firmware in $out"
