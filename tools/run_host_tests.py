"""Run HAL-independent C tests with GCC/Clang or MSVC (developer shell)."""
import argparse
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()

TESTS = {
    "test_alarm": ["tests/test_alarm.c", "stm32f103/app/alarm/alarm.c",
                   "stm32f103/app/gas/gas.c", "stm32f103/app/config/config.c"],
    "test_gas": ["tests/test_gas.c",
                 "stm32f103/app/gas/gas.c", "stm32f103/app/config/config.c"],
    "test_config": ["tests/test_config.c",
                    "stm32f103/app/gas/gas.c",
                    "stm32f103/app/config/config.c"],
    "test_history": ["tests/test_history.c",
                     "stm32f103/app/history/history.c"],
    "test_protocol": ["tests/test_protocol.c",
                      "stm32f103/app/gas/gas.c",
                      "stm32f103/app/history/history.c",
                      "stm32f103/app/protocol/protocol.c", "stm32f103/app/config/config.c"],
}
with tempfile.TemporaryDirectory(prefix="gas-tests-") as folder:
    folder = pathlib.Path(folder)
    for name, sources in TESTS.items():
        paths = [root / source for source in sources]
        exe = folder / f"{name}.exe"
        if pathlib.Path(args.cc).stem.lower() == "cl":
            # /utf-8 because the sources carry Chinese comments and are UTF-8
            # without a BOM, as the rest of the repository is. Without it MSVC
            # decodes them as the local code page and C4819 becomes an error
            # under /WX. The cross build needs nothing: gcc assumes UTF-8.
            command = [args.cc, "/nologo", "/std:c11", "/W4", "/WX", "/utf-8",
                       f"/I{root / 'stm32f103/app'}", f"/I{root / 'stm32f103/bsp'}", *map(str, paths), f"/Fe:{exe}"]
        else:
            command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                       f"-I{root / 'stm32f103/app'}", f"-I{root / 'stm32f103/bsp'}", *map(str, paths), "-o", str(exe)]
        subprocess.run(command, cwd=folder, check=True)
        subprocess.run([str(exe)], cwd=folder, check=True)

