"""Run HAL-independent C tests with GCC/Clang or MSVC (developer shell)."""
import argparse
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--cc", default="cc")
args = parser.parse_args()
sources = [root / "tests/test_gas.c", root / "stm32f103/app/monitor/gas_monitor.c",
           root / "stm32f103/app/parameters/gas_store.c"]
with tempfile.TemporaryDirectory(prefix="gas-tests-") as folder:
    exe = pathlib.Path(folder) / "test_gas.exe"
    if pathlib.Path(args.cc).stem.lower() == "cl":
        command = [args.cc, "/nologo", "/std:c11", "/W4", "/WX",
                   f"/I{root / 'stm32f103/app'}", *map(str, sources), f"/Fe:{exe}"]
    else:
        command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   f"-I{root / 'stm32f103/app'}", *map(str, sources), "-o", str(exe)]
    subprocess.run(command, cwd=folder, check=True)
    subprocess.run([str(exe)], cwd=folder, check=True)
