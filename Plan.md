# 短期实现计划 (Plan)

> 详细文档见 [README.md](README.md)

## 待实现

### P0
- TCP 数据上报 (利用 ESP01S 透传 + uploadOffset 追踪)
- NFC 天线 GND 控制优化

### P1
- 考勤统计显示 (OLED 统计页)
- 掉电保护增强

### P2
- 管理员界面滚动菜单 (3+ 项)
- 清除 Flash 打卡记录

## 注意事项
- 结构体布局不可改 (Flash 持久化兼容)
- RC522 用 NFC_LockReader/UnlockReader 并发锁
- 队列 item size = sizeof(数据类型), 否则栈溢出
- I2C HAL 异常时 DeInit+Init, 不可每帧强制复位
- OLED 看门狗在 defaultTask, guiTask 仅喂狗
- 大缓冲区用 static 避免栈溢出
