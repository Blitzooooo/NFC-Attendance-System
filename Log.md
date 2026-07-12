# 代码改动日志 (Change Log)

> 详细架构文档见 [README.md](README.md)

## 2026-07-11

### 时钟界面滚动重构 + I2C 卡死修复 + 心跳保活
- 日期行/天气行像素级平滑滚动 (16px/s, 负 x 渲染)
- 天气格式优化 (城市前置, ℃, 去分隔符) + 本地温度标签
- 字符宽度 UTF-8 预计算 (中英混排正确)
- I2C HAL 抢占死锁修复 (按需 DeInit+Init)
- OLED 看门狗迁移至 defaultTask
- 位图写保护 (guiTask 显示中仅读头)
- 温度异步缓存 (消除 guiTask OneWire 阻塞)
- 心跳保活 (`HEART:DEV|MODE|TEMP|WIFI|PEND`)
- 上传追踪 (`Record_MarkAllUploaded` / `Record_GetUnuploadCount`)
- `WEATHER_UPDATE_INTERVAL_MS` / `HEARTBEAT_INTERVAL_MS` 可配

### 记录存储模块独立拆分
- 新增 `BSP/record.c/h`，从 display 分离 Flash 存储/记录/LRU/CRC16

### NTP 校准后回写历史记录
- `Record_FixAllTimeOffsets` + 时间同步锁

### 黑名单 (挂失卡) 功能
- `BSP/record.c/h`: 新增 `BlacklistData_t` 结构体, SPI Flash 扇区4持久化, CRC16校验
- `LOST:UID` / `UNLOST:UID` 串口指令 (无需RC522, 纯软件操作)
- nfcTask 刷卡时检查黑名单, 命中则拒绝打卡, OLED显示"已挂失"
- `Record_List` STATUS 输出增加 `LOST` (status=4)
- OLED 帧率调整为 75ms 统一刷新

## 2026-07-10

### ESP01S 联网校时 + 天气滚动显示
- 心知天气 API + NTP 校时 + weatherTask
- NTP 时间偏差记录 (OFS 字段)

## 2026-07-08

### NFC 考勤系统核心功能完成
- 三种考勤模式 + OLED 状态机 + 串口发卡协议 + 图像卡 + SPI Flash 存储
