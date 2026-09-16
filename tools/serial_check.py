"""通过串口对固件做一次功能测试：逐条发真实命令，比对板子真实回来的应答。

协议定义见 docs/uart_protocol.md，实现见 app/protocol/protocol.c。覆盖命令
分发、大小写不敏感、参数解析、范围校验、空行不回话、多行应答和远程关阀拒绝。
默认只做无副作用的查询，以及写入后立刻还原；报警通路会锁存关阀、且没有
EEPROM 时无法从面板解除，需要显式加 --alarm，跑完要复位板子。

用法:
    python tools/serial_check.py COM11
    python tools/serial_check.py COM11 --alarm
"""
import argparse
import re
import sys
import time

import serial

R = re.compile
STATUS_LINES = [R(r"STATE=\w+ VALVE=(OPEN|CLOSED) ALARMS=\d+"),
                R(r"MQ4=\d+/\d+ MQ6=\d+/\d+ MQ7=\d+/\d+")]

# 只读：不改变任何状态。
READ_ONLY = [
    ("STATUS?", STATUS_LINES),
    ("status?", STATUS_LINES),                       # 命令不区分大小写
    ("CONFIG?", ["TH MQ4=2400 MQ6=2400 MQ7=2400 PERIOD=100 BUZZ=5S"]),
    ("HISTORY?", [R(r"HISTORY \d+/\d+")]),
    ("", []),                                        # 空行是行结束，不回话
    ("NONSENSE", ["ERR UNKNOWN"]),
    ("SET MQ4 2500 junk", ["ERR ARGS"]),             # 参数比任何命令能收的都多
    ("SET MQ4", ["ERR UNKNOWN"]),
    ("SET MQ9 1000", ["ERR NAME"]),
    ("SET BUZZZER 5", ["ERR NAME"]),
    ("SET MQ4 abc", ["ERR VALUE"]),
    ("SET BUZZER S", ["ERR VALUE"]),
    ("SET MQ4 100", ["ERR RANGE"]),                  # 低于下限 200
    ("SET MQ4 9999", ["ERR RANGE"]),                 # 高于上限 4000
    ("SET PERIOD 50", ["ERR RANGE"]),                # 低于 100
    ("SET PERIOD 255", ["ERR RANGE"]),               # 不是 10 的整数倍
    ("SET BUZZER 61", ["OK BUZZ=ALWAYS"]),           # 61 就是 ALWAYS 的哨兵值
    ("SET BUZZER 62", ["ERR RANGE"]),                # 比哨兵值还大 1
    ("SET BUZZER 256", ["ERR RANGE"]),
    ("VALVE OPEN", ["ERR ONLY CLOSE"]),              # 刻意没有远程开阀；这条不改状态
]

# 会置锁存的命令。gas_close_valve() 和报警走同一条锁存路径，跑过一次阀门就
# 关到复位为止：EEPROM 不在时锁存不持久化，复位即解除。所以单独放在最后，
# 且不进「只读」那一组。
LATCHING = [
    ("VALVE close", ["OK VALVE=CLOSED"]),
]

# 写入类：每条之后立刻还原，避免改掉台面上的配置。
WRITE_AND_RESTORE = [
    ("SET MQ4 2600", ["OK MQ4=2600"], "SET MQ4 2400", ["OK MQ4=2400"]),
    ("SET PERIOD 1000", ["OK PERIOD=1000"], "SET PERIOD 100", ["OK PERIOD=100"]),
    ("SET BUZZER ALWAYS", ["OK BUZZ=ALWAYS"], "SET BUZZER 5S", ["OK BUZZ=5S"]),
    ("SET BUZZER 0", ["OK BUZZ=OFF"], "SET BUZZER 5S", ["OK BUZZ=5S"]),
    ("SET BUZZER 60", ["OK BUZZ=60S"], "SET BUZZER 5S", ["OK BUZZ=5S"]),
]

QUIET_S = 0.5  # 多久没有新字节就认为一条应答发完了


def read_reply(port):
    """读到总线安静为止，给出非空的行。"""
    buf = b""
    deadline = time.time() + QUIET_S
    while time.time() < deadline:
        chunk = port.read(256)
        if chunk:
            buf += chunk
            deadline = time.time() + QUIET_S
    text = buf.decode("utf-8", "replace").replace("\r\n", "\n")
    return [line for line in text.split("\n") if line.strip()]


def matches(line, want):
    return want.match(line) is not None if hasattr(want, "match") else line == want


def check(port, send, expected, failures):
    port.reset_input_buffer()
    if send:
        port.write((send + "\r\n").encode())
        port.flush()
    got = read_reply(port)
    ok = len(got) == len(expected) and all(matches(l, w) for l, w in zip(got, expected))
    label = '"%s"' % send if send else "(空行)"
    if ok:
        print("  PASS  %-20s -> %s" % (label, " | ".join(got) if got else "(无应答)"))
    else:
        failures.append(send)
        print("  FAIL  %-20s" % label)
        print("        期望: %s" % " | ".join(str(e.pattern if hasattr(e, "pattern") else e)
                                              for e in expected))
        print("        实收: %s" % (" | ".join(got) if got else "(无应答)"))
    return got


def alarm_phase(port, failures):
    """把阈值压到当前读数之下，验证报警通路。"""
    status = check(port, "STATUS?", STATUS_LINES, failures)
    if len(status) < 2:
        return
    mq4 = int(re.search(r"MQ4=(\d+)/", status[1]).group(1))
    print("  (MQ4 当前读数 %d；把阈值压到下限 200 触发报警)" % mq4)
    port.reset_input_buffer()
    port.write(b"SET MQ4 200\r\n")
    port.flush()
    got = read_reply(port)
    # 应答和主动推送可能落在同一次读取里，所以一起看。
    if "OK MQ4=200" not in got:
        failures.append("SET MQ4 200")
        print("  FAIL  阈值未接受: %s" % " | ".join(got))
    push = [line for line in got if line.startswith("ALARM ")]
    if push:
        print("  PASS  主动推送        -> %s" % push[0])
    else:
        failures.append("ALARM push")
        print("  FAIL  没收到主动推送的 ALARM 行，实收: %s" % " | ".join(got))
    check(port, "STATUS?", [R(r"STATE=ALARM VALVE=CLOSED ALARMS=\d+"),
                            R(r"MQ4=\d+/200 MQ6=\d+/\d+ MQ7=\d+/\d+")], failures)
    # EEPROM 没接，history_add() 写失败，计数不会涨——这不是协议的问题。
    check(port, "HISTORY?", [R(r"HISTORY \d+/\d+")], failures)
    check(port, "SET MQ4 2400", ["OK MQ4=2400"], failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="串口号，例如 COM11")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--alarm", action="store_true", help="额外测报警通路")
    args = parser.parse_args()

    failures = []
    with serial.Serial(args.port, args.baud, timeout=0.05) as port:
        time.sleep(0.3)
        port.reset_input_buffer()

        print("== 查询与解析 ==")
        for send, expected in READ_ONLY:
            check(port, send, expected, failures)

        print("== 写入与范围校验（每条都还原） ==")
        for send, expected, back, back_expected in WRITE_AND_RESTORE:
            check(port, send, expected, failures)
            check(port, back, back_expected, failures)

        latched = False
        if args.alarm:
            print("== 报警通路 ==")
            alarm_phase(port, failures)
            latched = True

        print("== 远程关阀（会置锁存，放最后） ==")
        for send, expected in LATCHING:
            check(port, send, expected, failures)
        latched = True

    checks = (len(READ_ONLY) + 2 * len(WRITE_AND_RESTORE) + len(LATCHING)
              + (5 if args.alarm else 0))
    print("\n%d 项检查，%d 项失败" % (checks, len(failures)))
    if latched:
        print("锁存已置位，阀门保持关闭。串口只能关不能开，KEY4 要环境回到安全才生效——")
        print("MQ6/MQ7 没接时读数浮在安全线以上，KEY4 永远不成立，只能复位板子。")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
