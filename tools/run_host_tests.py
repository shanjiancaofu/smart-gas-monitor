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
    "test_gas_monitor": ["tests/test_gas_monitor.c",
                         "stm32f103/app/gas/gas_monitor.c"],
    "test_settings": ["tests/test_settings.c",
                      "stm32f103/app/gas/gas_monitor.c",
                      "stm32f103/app/settings/settings.c"],
    "test_history": ["tests/test_history.c",
                     "stm32f103/app/history/history.c"],
    "test_protocol": ["tests/test_protocol.c",
                      "stm32f103/app/gas/gas_monitor.c",
                      "stm32f103/app/history/history.c",
                      "stm32f103/app/communication/protocol.c"],
}
with tempfile.TemporaryDirectory(prefix="gas-tests-") as folder:
    folder = pathlib.Path(folder)
    for name, sources in TESTS.items():
        paths = [root / source for source in sources]
        exe = folder / f"{name}.exe"
        if pathlib.Path(args.cc).stem.lower() == "cl":
            command = [args.cc, "/nologo", "/std:c11", "/W4", "/WX",
                       f"/I{root / 'stm32f103/app'}", *map(str, paths), f"/Fe:{exe}"]
        else:
            command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                       f"-I{root / 'stm32f103/app'}", *map(str, paths), "-o", str(exe)]
        subprocess.run(command, cwd=folder, check=True)
        subprocess.run([str(exe)], cwd=folder, check=True)
