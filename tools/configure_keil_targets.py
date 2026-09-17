"""Normalize the CubeMX Keil project into independent HW/Soft I2C targets."""
import copy
import pathlib
import xml.etree.ElementTree as ET


def configure(project: pathlib.Path) -> None:
    tree = ET.parse(project)
    root = tree.getroot()
    targets = root.find("Targets")
    if targets is None or targets.find("Target") is None:
        raise RuntimeError(f"No Keil target found in {project}")
    base = targets.find("Target")
    for node in list(targets):
        targets.remove(node)
    for mode, value in (("hw", "0"), ("soft", "1")):
        target = copy.deepcopy(base)
        target.find("TargetName").text = f"smart_gas_monitor_{mode}"
        common = target.find("./TargetOption/TargetCommonOption")
        common.find("OutputDirectory").text = f"..\\..\\build\\keil5\\{mode}\\Artifacts\\"
        common.find("OutputName").text = f"smart_gas_monitor_keil_{mode}"
        common.find("ListingPath").text = f"..\\..\\build\\keil5\\{mode}\\Listings\\"
        target.find(".//Cads/VariousControls/Define").text = (
            f"USE_HAL_DRIVER,STM32F103xB,USE_SOFT_I2C={value}"
        )
        targets.append(target)
    ET.indent(tree, space="  ")
    tree.write(project, encoding="utf-8", xml_declaration=True)


if __name__ == "__main__":
    configure(pathlib.Path(__file__).resolve().parents[1] /
              "stm32f103/MDK-ARM/smart_gas_monitor.uvprojx")
