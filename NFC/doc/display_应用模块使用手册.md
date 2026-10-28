# display 显示状态机应用模块使用手册

> **分类：应用程序**（业务逻辑层，OLED 显示状态机）

## 1. 模块作用

统一在 guiTask 上下文驱动的 OLED 显示状态机：CLOCK / RESULT / TIME_SET / ADMIN_SET，
负责滚动渲染、天气缓存、温度缓存、时间偏差、看门狗恢复。

## 2. 函数清单

| 函数 | 做了什么 | 用了什么底层 | 为什么这么设计 |
|------|---------|-------------|---------------|
| `DISP_Init()` | 初始化状态机/缓冲 | — | 清空共享结构 |
| `DISP_ApplyI2CConfig()` | 覆盖 I2C 时钟为 400kHz | `HAL_I2C_DeInit/Init` | 与 `DISP_I2C_TARGET_HZ` 一致 |
| `DISP_ShowClock()` | 时钟页 + 滚动渲染 | GUI + bsp_rtc + 缓存 | 负 x 坐标像素级滚动 |
| `DISP_ShowCardResult()` | 刷卡结果页 | GUI | 倒计时/超时返回 |
| `DISP_ShowTimeSet/AdminSet()` | 时间/管理员设置页 | GUI + record | K5/K6 键操作 |
| `DISP_FeedWatchdog/CheckStuck/ResetOLED` | OLED 看门狗 | — | 卡死 3s 自动 DeInit+Init 复位 |
| `DISP_UpdateTempCache/GetCachedTemp` | 温度异步缓存 | ds18B20 | guiTask 零延迟读，避免 OneWire 阻塞 |
| `DISP_SetWeatherCache()` | 天气缓存 | 队列数据 | 中英混排宽度预计算 |
| `DISP_SetTimeOffset/IsRtcCalibrated` | NTP 偏差 | — | OFS 机制 |
| `DISP_EnterTimeSync/ExitTimeSync` | 时间同步锁 | — | 回写期间禁止刷卡/串口 |
| `DISP_SendNfcEvent()/DISP_CheckNfcEvent()` | NFC 队列收发 | 消息队列 myQueue04 | 生产者/消费者分离，消除共享内存竞态 |

## 3. 状态机

```
CLOCK ←→ RESULT ←→ TIME_SET ←→ ADMIN_SET
```

- CLOCK：默认，刷卡→RESULT，K5 长按→TIME_SET
- RESULT：3s 超时/按键→CLOCK，管理卡→ADMIN_SET
- 事件：nfcTask `osThreadFlagsSet`，guiTask `osThreadFlagsWait`

## 4. 与其它模块耦合

- 依赖：ssd1306 / GUI / bsp_rtc / ds18B20 / record。
- 被调用：freertos 的 guiTask / defaultTask。
- 与 nfcTask 共享 `DispCardInfo_t` / `DispCardBitmap_t`。

## 5. 注意事项

- 帧间隔 `DISP_FRAME_INTERVAL_MS` 过小会花屏（I2C 带宽不足）。
- 滚动参数集中在 display.h 顶部调参。
- 位图写保护：guiTask 显示中仅读头，nfcTask 写缓冲前需判断状态。
- 温度必须异步缓存，不能阻塞 GUI。
