# 逐页 dump 面板内容：靠改 display.page 和 last_draw_ms 逼 display_update 重画。
# 报警页用把 MQ6 阈值临时压到浮空电平之下来触发，dump 完 reset 就恢复默认。
set confirm off
set pagination off
set print pretty on

define snap
    set system_app.display.last_draw_ms = 0
    monitor resume
    shell sleep 1
    monitor halt
    set $fb = (char *)&system_app.oled.buffer
    dump binary memory $arg0 $fb $fb + 1024
end

target extended-remote localhost:3333
monitor reset halt
monitor resume
shell sleep 5
monitor halt

# 实时页
set system_app.display.page = 0
snap page_main.bin

# 参数页：选中项停在 MQ4
set system_app.display.page = 1
set system_app.monitor.item = 0
snap page_settings0.bin

# 参数页：光标移到蜂鸣器（第 5 项）上看还画不画得出来
set system_app.monitor.item = 4
snap page_settings4.bin

# 历史页（EEPROM 没接，应为空）
set system_app.display.page = 2
snap page_history.bin

# 报警页：把 MQ6 阈值压到浮空读数之下
set system_app.monitor.config.alarm[1] = 2000
monitor resume
shell sleep 1
monitor halt
snap page_alarm.bin

monitor reset run
disconnect
quit
