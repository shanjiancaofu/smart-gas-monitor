# 固件说明

工程按 `main → app → bsp → HAL` 分层。CubeMX 负责 `stm32f103/` 中的初始化与 HAL 文件，仓库根目录的 CMake 配置负责统一编译 CubeMX、app 和 bsp。

## 构建目录

```text
build/
├── arm-debug/
└── arm-release/
```

CMake 和 Ninja 的缓存、目标文件及固件都放在对应配置目录内。仓库根目录不再使用 `build.ps1`，`cubemx/` 下也不再维护手写 GNUmakefile。

## 构建命令

Debug：

```sh
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel 4
```

Release：

```sh
cmake --preset arm-release
cmake --build --preset arm-release --parallel 4
```

Debug 使用 `-Og -g3`，适合断点调试；Release 使用 `-Os -g3`，适合最终烧录。产物文件名包含本地配置时间，重新配置时会移除该目录中的上一组固件文件。

若工具链不在 PATH，可设置：

```sh
ARM_GNU_TOOLCHAIN_ROOT=/path/to/gnu-arm-tools
```

该目录必须包含 `bin/arm-none-eabi-gcc`。Windows PowerShell 对应写法为 `$env:ARM_GNU_TOOLCHAIN_ROOT = "..."`，但实际构建命令仍然是相同的 `cmake --preset`。
