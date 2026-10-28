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

## 2026-10-28

### 文档整理 + 问题排查结论
- 新增 README §18「临界区与延时函数选择规范」：临界区概念、`osDelay` vs `delay_us`、OLED I2C 轮询结论、读卡刷新冲突修正版
- 新增三篇应用模块手册：NFC / record / display + 驱动模块总览与分类索引
- 纠正：OLED 走硬件 I2C1 阻塞式轮询 (`HAL_I2C_Mem_Write`，超时 100ms)，非中断式；临界区屏蔽中断并不直接干扰 OLED 传输
- 发现：`myQueue04` (NFC 队列，4×1024B) 为死队列，只创建未使用，浪费约 4KB FreeRTOS heap (总堆 48KB)
- 发现：共享数据 `s_cardInfo` / `s_cardBitmap` 无互斥锁，当前靠"先写数据后置事件标志"的顺序 + `DISP_STATE_RESULT` 状态守卫隐式防竞态

### 分支 feature/nfc-queue: 接入 NFC 队列
- 目的：把死队列 `myQueue04` 接线，让 guiTask 成为共享缓存的唯一写者，彻底消除 `s_cardInfo` / `s_cardBitmap` 的共享内存竞态
- 实现：nfcTask 5 处 `DISP_NotifyCardEvent` → `DISP_SendNfcEvent(myQueue04Handle, ...)`；guiTask CLOCK 态改为 `DISP_CheckNfcEvent(myQueue04Handle)` 非阻塞轮询队列（无事件时 `osDelay(DISP_FRAME_INTERVAL_MS)` 保持帧节奏）；删除无用的 `uint32_t flags`
- 收益：回收约 4KB FreeRTOS heap；消除无锁共享数据隐患；已编译通过 (NFCAttend.elf)
