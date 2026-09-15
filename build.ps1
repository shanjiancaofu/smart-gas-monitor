param([Parameter(ValueFromRemainingArguments=$true)][string[]]$MakeArgs)
$repo = $PSScriptRoot
$make = Get-ChildItem 'E:/STM32CubeIDE' -Recurse -Filter make.exe | Select-Object -First 1
$gcc = Get-ChildItem 'E:/STM32CubeIDE' -Recurse -Filter arm-none-eabi-gcc.exe | Select-Object -First 1
$env:PATH = $gcc.DirectoryName + ';' + $env:PATH
& $make.FullName -C (Join-Path $repo 'stm32f103/cubemx') @MakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
New-Item -ItemType Directory -Force (Join-Path $repo 'build') | Out-Null
Copy-Item (Join-Path $repo 'stm32f103/cubemx/build/smart_gas_monitor.*') (Join-Path $repo 'build') -Force
