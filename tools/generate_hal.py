"""Regenerate the checked-in HAL project with standalone STM32CubeMX."""
import argparse
import pathlib
import re
import subprocess

from configure_keil_targets import configure as configure_keil_targets

root = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cubemx', type=pathlib.Path, required=True, help='CubeMX installation directory')
parser.add_argument('--cube', type=pathlib.Path, required=True, help='Installed STM32Cube FW_F1 directory')
parser.add_argument('--toolchain', default='MDK-ARM V5.32', help='CubeMX toolchain, for example MDK-ARM V5.32 or Makefile')
args = parser.parse_args()
ioc = root / 'stm32f103/smart_gas_monitor.ioc'
main = root / 'stm32f103/Core/Src/main.c'
project = root / 'stm32f103/MDK-ARM/smart_gas_monitor.uvprojx'
before = project.stat().st_mtime_ns if project.exists() else 0
build = root / 'build'
logs = build / 'logs'
logs.mkdir(parents=True, exist_ok=True)
script = logs / 'cubemx-generate.txt'
log = logs / 'cubemx-generate.log'
# Use CubeMX's unquoted command-file path syntax.
script.write_text(
    f'popupwrapper set\nconfig load {ioc.as_posix()}\n'
    f'project setCustomFWPath {args.cube.resolve().as_posix()}\n'
    f'project toolchain {args.toolchain}\nproject generate\nconfig save\nexit\n', encoding='utf-8')
with log.open('w', encoding='utf-8') as output:
    subprocess.run([str(args.cubemx / 'jre/bin/java.exe'), '-jar',
                    str(args.cubemx / 'STM32CubeMX.exe'), '-q', str(script)],
                   cwd=root, stdout=output, stderr=subprocess.STDOUT, check=True)
generated_log = log.read_text(encoding='utf-8', errors='replace')
if (not main.exists() or project.stat().st_mtime_ns == before or
        'Time for Generating toolchain IDE Files:' not in generated_log):
    raise SystemExit(f'CubeMX generation was not confirmed; inspect {log}')
text = ioc.read_text(encoding='utf-8')
text = re.sub(r'(?m)^ProjectManager.CustomerFirmwarePackage=.*$',
              'ProjectManager.CustomerFirmwarePackage=', text)
text = re.sub(r'(?m)^ProjectManager.DefaultFWLocation=.*$',
              'ProjectManager.DefaultFWLocation=true', text)
ioc.write_text(text, encoding='utf-8')
configure_keil_targets(project)
print(f'PASS: HAL regenerated. Log: {log}')
