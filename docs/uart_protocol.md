# 串口协议

USART1 和 USART2 共用同一套文本协议。每条命令以 `CRLF` 结束，命令名不区分大小写。USART1 连接 USB-TTL，波特率 115200；USART2 连接 HC-05，波特率 9600；数据格式均为 8-N-1。

查询命令：

```text
STATUS?
CONFIG?
HISTORY?
```

配置命令：

```text
SET MQ4 2500
SET MQ7 2000
SET MQ8 2500
SET PERIOD 500
SET BUZZER OFF
SET BUZZER 5
SET BUZZER ALWAYS
```

执行命令：

```text
VALVE CLOSE
```

协议不提供远程开阀命令。`VALVE OPEN` 始终返回 `ERR ONLY CLOSE`，报警或锁存解除必须满足安全条件后由现场 KEY4 确认。

常见应答示例：

```text
STATUS?
STATE=NORMAL VALVE=OPEN ALARMS=NONE
MQ4=0840/2400 MQ7=0710/2000 MQ8=0900/2400

CONFIG?
TH MQ4=2400 MQ7=2000 MQ8=2400 PERIOD=100 BUZZ=5S

SET MQ7 2200
OK MQ7=2200

SET MQ7 5000
ERR RANGE
```

报警首次进入时两路串口都会主动收到一行 `ALARM`。`HISTORY?` 返回最新记录在前的报警历史，记录内容是序号、上电秒数、三路 ADC 值和报警通道掩码；没有 RTC，因此不返回日期时间。
