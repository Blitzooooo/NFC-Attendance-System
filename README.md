# NFC 考勤系统 (NFC Attendance System)

> **目标芯片**: STM32F407VET6 (Cortex-M4, 512KB Flash, 128KB+64KB CCM RAM)  
> **RTOS**: FreeRTOS V10.3.1 (CMSIS-RTOS v2 API)  
> **工具链**: arm-none-eabi-gcc (Makefile)  
> **CubeMX**: v6 (NFCAttend.ioc)

---

## 1. 工程实现目标

基于 STM32F407 的 **NFC 考勤终端**，核心功能：

| 功能 | 说明 |
|------|------|
| **NFC 刷卡考勤** | RC522 读 Mifare Classic 1K 卡，三种考勤模式 |
| **OLED 显示** | SSD1306 128×64，显示时钟/刷卡结果/头像姓名/管理设置，像素级平滑滚动，内置 I2C 防卡死+看门狗自动恢复 |
| **管理员卡管理** | 刷管理卡 + 按键在 OLED 上配设备 ID 和考勤模式 |
| **SPI Flash 存储** | W25Qxx 存系统配置(双备份) + 刷卡记录(循环写) |
| **WiFi 联网** | ESP-01S (ESP8266) AT 指令，支持 NTP 授时 + 心知天气 API |
| **RTC 时钟** | 片内 RTC + 备份寄存器，NTP 自动校准，掉电保持 |
| **温度监测** | DS18B20，显示在时钟界面天气行末尾 (`本地温度：25.5℃`) |
| **蜂鸣器 MIDI** | 刷卡反馈音 |
| **PC 串口发卡协议** | USART1 接收上位机命令，支持 ISSUE/READ/IMGA/IMGN/IMGD 等指令 |

### 考勤模式

| 模式值 | 名称 | 规则 |
|--------|------|------|
| 0 | 入口模式 | 仅显示签到时刻(L4亮)；已入场→重复刷卡 |
| 1 | 出口模式 | 仅显示离开时长(L5亮)；无入场→提示 |
| 2 | 出入口模式(默认) | 在场→离场+时长(L7亮)；不在场→入场(L6亮) |

### LED 映射 (PE8~PE14, 共阳极, 低电平亮, 位掩码与 PCB 丝印对应)

| LED | 引脚 | 掩码 | 用途 |
|-----|------|:---:|------|
| L1 | PE8 | 0x01 | 卡检测指示 |
| L2 | PE9 | 0x02 | 管理员卡 |
| L3 | PE10 | 0x04 | (预留) |
| L4 | PE11 | 0x10 | 入口模式签到 |
| L5 | PE12 | 0x20 | 出口模式签退 |
| L6 | PE13 | 0x40 | 出入口模式签到 |
| L7 | PE14 | 0x80 | 出入口模式签退 |

---

## 2. 工程文件结构

```
NFC_SyS/
├── NFC/                              # 主项目目录
│   ├── NFCAttend.ioc                 # CubeMX 项目配置
│   ├── Makefile                      # GCC Makefile (arm-none-eabi-gcc)
│   ├── STM32F407xx_FLASH.ld          # 链接脚本 (512K Flash, 128K+64K RAM)
│   ├── startup_stm32f407xx.s         # 启动汇编 (Reset_Handler, 中断向量表)
│   ├── build_log.txt                 # 最近编译日志
│   │
│   ├── BSP/                          # ★ 板级支持包 (所有业务逻辑)
│   │   ├── NFC.h / NFC.c             #   NFC 高层封装: 读卡解析/串口发卡协议
│   │   ├── rc522.h / rc522.c         #   RC522 芯片驱动 (硬件无关)
│   │   ├── rc522_platform_stm32.c    #   RC522 STM32 平台适配 (GPIO模拟SPI)
│   │   ├── display.h / display.c     #   显示状态机 + 天气缓存 + 时间偏差 + 同步锁
│   │   ├── record.h / record.c       #   ★ 记录存储模块: Flash配置/刷卡记录/LRU缓存/CRC16
│   │   ├── ssd1306.h / ssd1306.c     #   SSD1306 OLED 驱动
│   │   ├── ssd1306_i2c.h / ssd1306_i2c.c  # SSD1306 I2C 底层通信
│   │   ├── GUI.h / GUISlim.c         #   GUI 图形库接口/精简实现
│   │   ├── F08_ASCII.c               #   8×8 ASCII 字库
│   │   ├── SimSun_16.c               #   宋体 16px 中文字库
│   │   ├── SimSun_32.c               #   宋体 32px 中文字库
│   │   ├── HDU.c                     #   校徽 LOGO 位图 (64×64)
│   │   ├── uart_drv.h / uart_drv.c   #   通用串口驱动 (IDLE中断, 多实例)
│   │   ├── esp01s.h / esp01s.c       #   ESP-01S WiFi 驱动 (AT指令状态机)
│   │   ├── w25qxx.h / w25qxx.c       #   W25Qxx SPI Flash 驱动
│   │   ├── bsp_rtc.h / bsp_rtc.c     #   RTC 日历驱动 (含星期计算)
│   │   ├── key.h / key.c             #   按键驱动 (短按/长按/连按)
│   │   ├── led.h / led.c             #   LED 驱动 (位掩码控制)
│   │   ├── ds18B20.h / ds18B20.c     #   DS18B20 温度传感器 (OneWire)
│   │   ├── delay_us.h / delay_us.c   #   微秒延时 (DWT CYCCNT)
│   │   └── midi.h / midi.c           #   蜂鸣器 MIDI (TIM3 CH1 PWM)
│   │
│   ├── Core/
│   │   ├── Inc/                      # HAL 外设头文件 (13个)
│   │   │   ├── main.h, gpio.h, adc.h, dac.h, dma.h
│   │   │   ├── i2c.h, rtc.h, spi.h, tim.h, usart.h
│   │   │   ├── FreeRTOSConfig.h, stm32f4xx_hal_conf.h, stm32f4xx_it.h
│   │   └── Src/                      # HAL 外设源文件 + 用户代码
│   │       ├── main.c                #   main(): HAL初始化 → 启动FreeRTOS
│   │       ├── freertos.c            #   ★ FreeRTOS 任务: 6个任务 + 2个队列
│   │       ├── rtc.c                 #   MX_RTC_Init(): 备份寄存器防重复初始化
│   │       ├── stm32f4xx_it.c        #   中断服务 (SysTick→HAL, 串口IDLE等)
│   │       ├── stm32f4xx_hal_msp.c   #   HAL MSP 初始化 (外设引脚/时钟)
│   │       ├── stm32f4xx_hal_timebase_tim.c  # HAL时基用TIM
│   │       ├── syscalls.c / sysmem.c #   底层系统调用
│   │       └── system_stm32f4xx.c    #   系统初始化
│   │
│   ├── Drivers/
│   │   ├── CMSIS/                    # CMSIS 核心 + STM32F4 设备头文件
│   │   └── STM32F4xx_HAL_Driver/     # STM32 HAL 库 (外设驱动)
│   │
│   ├── Middlewares/
│   │   └── Third_Party/FreeRTOS/     # FreeRTOS V10.3.1 源码
│   │
│   ├── doc/                          # 驱动模块使用手册 (12份 .md)
│   └── build/                        # 编译中间文件 (.o, .d, .lst)
│
└── tools/                            # 开发工具
    ├── arm-none-eabi-gcc/            # GCC 交叉编译器
    ├── openocd/                      # OpenOCD 调试/烧录工具
    └── windows-build-tools/          # Windows 构建工具 (make等)
```

---

## 3. FreeRTOS 任务架构

| 任务 | 优先级 | 栈 | 周期/触发 | 功能 |
|------|--------|-----|-----------|------|
| **defaultTask** | Normal | 2KB | 1s周期 | 系统初始化 → 温度缓存更新 + OLED 看门狗检测 + 心跳保活上报 |
| **guiTask** | BelowNormal | 6KB | 事件驱动, ~75ms刷新 | GUI 显示状态机: CLOCK(RTC+天气滚动)→RESULT(刷卡结果)→TIME_SET→ADMIN_SET, 循环顶部喂看门狗 |
| **keyTask** | Normal | 2KB | 10ms | 按键扫描: `Key_Scan()` |
| **uartTask** | Normal | 6KB | 1ms轮询 | 串口命令: 读取 USART1 行缓冲, 调用 `NFC_ProcessCommand()` |
| **nfcTask** | Normal | 3KB | 200ms周期 | NFC 核心: 寻卡→去重→读卡→考勤判定→写Flash→通知GUI, 同卡仅读头保护位图 |
| **networkTask** | BelowNormal | 4KB | 1s周期 | WiFi/NTP/天气: 联网校时 + 断网重连 + 天气查询(1h间隔) |

| 队列 | 容量 | 元素大小 | 用途 |
|------|------|---------|------|
| myQueue01 | 8 | 128B (DISP_WEATHER_LINE_LEN) | 天气数据: networkTask → guiTask |
| myQueue04 | 4 | `sizeof(NfcCardMsg_t)` | 预留: NFC 刷卡数据队列 |

### 任务间通信
- **nfcTask → guiTask**: `osThreadFlagsSet(guiTaskHandle, DISP_EVT_xxx)` + 共享 `DispCardInfo_t`/`DispCardBitmap_t`
- **networkTask → guiTask**: `myQueue01` 队列传递天气字符串, 消除跨任务竞态
- **keyTask → guiTask**: `DISP_EVT_KEY_PRESSED` 标志 + `Key_IsShortPressed()/Key_IsLongPressed()` 轮询
- **defaultTask → guiTask**: `g_cachedTemp` 异步温度缓存 (guiTask 零延迟读)
- **uartTask**: 直接读写 `g_uart1Drv.rxData`，调用 `NFC_ProcessCommand()` 同步处理

---

## 4. 核心数据流

### 4.1 刷卡考勤流程 (nfcTask)

```
NFC_ScanCard() → 检测到卡 → LED_On(0) → 去重检查(3s内同UID)
→ MIDI_Beep(6,50) → osDelay(300ms稳定) → NFC_ReadCard()
→ 分类:
  ├─ 管理卡 → DISP_EVT_CARD_ADMIN, LED_On(2)
  ├─ 无效卡 → 写未知卡记录(st=3), DISP_EVT_CARD_INVALID
  ├─ 有效卡 + 黑名单命中 → DISP_EVT_CARD_INVALID, 上报 REC:UID=...|LOST, 写记录(st=4)
  ├─ 有效卡 + 管理员模式 → 拒绝, 短鸣两声, 不写记录
  └─ 有效卡 → Cache_Find(cardId) → miss→Record_QueryLast(倒扫Flash)
      → 按模式判定:
        ├─ 入口(0): 在场→DUP(st=1) : 入场(L4,st=0)
        ├─ 出口(1): 在场→离场+时长(L5,st=0) : 无入场(st=2)
        └─ 出入口(2): 在场→离场+时长(L7,st=0) : 入场(L6,st=0)
      → Cache_Update → Record_Add → DISP_NotifyCardEvent
→ NFC_WaitCardOff() → LED全熄 → 继续寻卡
```

### 4.2 显示状态机 (guiTask)

```
CLOCK ←→ RESULT ←→ TIME_SET ←→ ADMIN_SET
  ↑        ↑          ↑            ↑
  │    刷卡/按键     K5长按     管理卡
  └──────────────────────────────────┘
  超时/按键返回
```

### 4.3 串口发卡协议 (USART1)

| 命令 | 格式 | 说明 |
|------|------|------|
| `ISSUE` | `ISSUE:CID,SID,0,CTYPE\n` | 写 Block1 (卡号/工号/积分/类型) |
| `READ` | `READ\n` | 返回 UID/SID/TYPE |
| `IMGAxx` | `IMGAxx:HEX32\n` (xx=00~23) | 缓存头像块 |
| `IMGNxx` | `IMGNxx:HEX32\n` (xx=00~09) | 缓存姓名块 |
| `IMGDxx` | `IMGDxx:HEX32\n` (xx=00~09) | 缓存部门块 |
| `UPDATEIMG` | `UPDATEIMG\n` | 写全部44块图像数据到卡 |
| `CLEAR` | `CLEAR\n` | 清零 Block1 + 清除图像缓存 |
| `LIST` | `LIST[:N]\n` | 返回记录列表, N=条数(默认全部), STATUS: IN/OUT/DUP/NOIN/INV/LOST |
| `LOST` | `LOST:UID\n` | 挂失: UID加入黑名单 → `OK` / `ERR:BL_FULL` / `ERR:PARSE` |
| `UNLOST` | `UNLOST:UID\n` | 解除挂失: UID移出黑名单 → `OK` / `ERR:NOT_FOUND` / `ERR:PARSE` |

---

## 5. SPI Flash 存储布局 (W25Qxx)

| 区域 | 地址 | 扇区 | 大小 | 内容 |
|------|------|------|------|------|
| 主配置 | `0x000000` | S0 | 16B | `SysConfig_t` (设备ID/考勤模式/magic/CRC16) |
| 备配置 | `0x001000` | S1 | 16B | 配置备份 |
| 记录头 | `0x002000` | S2 | 32B | `RecordHeader_t` (写指针/总数/上传偏移) |
| 记录区 | `0x003000` | S3 | 4096B | `SwipeRecord_t` 循环写入, 128条/扇区 |
| 黑名单 | `0x004000` | S4 | 208B | `BlacklistData_t` (magic/count/uid[50]/CRC16) |

---

## 6. Mifare Classic 1K 卡数据布局

| 扇区 | 块 | 内容 |
|------|-----|------|
| 0, 块1 | Block1 | 卡号(4B) + 工号(4B) + 积分(4B) + 卡类型(1B) + 状态标志(1B) + 校验和(2B) |
| 1~8 | 块0~2 | 头像位图 (24块 = 48×64 @ 1bpp) |
| 9~12 | 块0 | 姓名位图 (10块 = 80×16 @ 1bpp) |
| 12~15 | 块1 | 部门位图 (10块 = 80×16 @ 1bpp) |

---

## 7. 关键硬件引脚

| 功能 | 引脚 | 说明 |
|------|------|------|
| K1~K4 | PE1~PE4 | 上拉输入, 低电平有效 |
| K5~K6 | PE5~PE6 | 下拉输入, 高电平有效 |
| L1~L7 | PE8~PE14 | 共阳极, 低电平亮 |
| NFC_NSS | PE15 | RC522 片选 |
| NFC_RST | PB15 | RC522 复位 |
| NFC_MOSI | PA0 | RC522 MOSI |
| NFC_MISO | PB13 | RC522 MISO |
| NFC_SCK | PD9 | RC522 SCK |
| NFC_GND | PB10 | NFC天线GND控制 |
| SPI1_CS | PC4 | W25Qxx Flash 片选 |
| TEMP | PE0 | DS18B20 OneWire |
| BEEP | PB4 | 蜂鸣器 PWM (TIM3 CH1) |
| I2C1 SCL/SDA | PB6/PB7 | OLED SSD1306 |
| USART1 TX/RX | PA9/PA10 | PC 串口发卡 |
| USART6 TX/RX | PC6/PC7 | ESP-01S WiFi |

---

## 8. NTP 时间偏差 (OFS) 机制

每条刷卡记录携带 `timeOffset` 字段 (REC 协议 `OFS=%d`)：

| OFS 值 | 含义 | 上位机处理 |
|:---:|------|------|
| `-1` (0xFFFF) | RTC 尚未经 NTP 校准, 时间不可信 | **丢弃** |
| `0` | RTC 已校准, 记录时间 = 标准时间 | 直接使用 |
| `±N` | NTP 校准后回写的历史记录修正量 | `标准时间 = 记录时间 + OFS` |

**校准流程**:
1. NTP 成功后 → `Record_FixAllTimeOffsets(delta)` 遍历 Flash 全部记录
2. 对 `OFS=-1` 的记录写入 `delta` 并重算 CRC
3. 擦除记录扇区 → 回写 → OFS 归零 (新记录无需修正)
4. 同步期间禁止刷卡和串口命令, OLED 显示 "Time Sync..."

---

## 9. 延迟函数选择规范

> **核心原则**: OLED (SSD1306 I2C) 与 RC522 (GPIO 模拟 SPI) 共享 MCU, RC522 的 `taskENTER_CRITICAL` 会屏蔽所有中断(含 I2C)。若在 RC522 操作间隙使用 `osDelay()`(触发 FreeRTOS 任务切换), GUI 任务被唤醒做 I2C 通信时又被后续 RC522 操作打断, 形成反复"启动-打断"循环, 导致 OLED 卡顿/冻屏。

### 必须用 `delay_us()` 的场景

| 场景 | 用法 | 说明 |
|------|------|------|
| RC522 扇区读之间 | `delay_us(2000)` | 2ms 忙等, I2C 中断可正常触发, **不切任务** |
| NFC_ReadCard 后 | `delay_us(50000)` | 50ms 让 I2C 完成 OLED 刷新 |
| CmdDone 重读后 | `delay_us(200000)` | 200ms 给 GUI 渲染 + I2C 恢复 |

### 可以用 `osDelay()` 的场景

| 场景 | 说明 |
|------|------|
| 锁重试 `while(!NFC_LockReader())` | 需让出 CPU 给持锁任务 |
| `CARD_SCAN_INTERVAL_MS` (200ms) | 周期性寻卡间隔, 需任务调度 |
| `CARD_STABLE_DELAY_MS` (300ms) | 卡片 RF 稳定, 时长过长不宜忙等 |
| 按键扫描/温度读取等非 NFC 任务 | 正常任务休眠 |

### `delay_us()` 实现 (`BSP/delay_us.c`)

基于 Cortex-M4 **DWT CYCCNT** 硬件周期计数器, 忙等但不关中断:
- `delay_us_init()` — 系统启动时调用一次
- `delay_us(uint32_t us)` — 微秒级延迟, 可重入, 中断/多任务安全

---

## 10. 串口心跳协议 (Heartbeat)

设备每 `HEARTBEAT_INTERVAL_MS`（默认 5000ms）主动上报：

```
HEART:DEV=1|MODE=2|TEMP=25.5|WIFI=1|PEND=3\r\n
```

| 字段 | 含义 |
|------|------|
| `DEV` | 设备 ID (1~999) |
| `MODE` | 考勤模式 (0=入口 1=出口 2=出入口) |
| `TEMP` | 本地 DS18B20 温度 (℃) |
| `WIFI` | 联网状态 (0=离线 1=已连接) |
| `PEND` | 未上传记录数 |

通过 `LIST` 命令查询的记录自动标记为已上传（`uploadOffset = totalCount`），`PEND` 归零。

---

## 11. 黑名单协议 (Blacklist)

### 11.1 概述

黑名单存储在 SPI Flash 扇区 4 (`0x004000`)，最大 50 条卡 UID，CRC16 校验保护，断电不丢失。

### 11.2 串口指令

```
上位机 → 下位机    LOST:A1B2C3D4      挂失: 将卡 UID 加入黑名单
上位机 → 下位机    UNLOST:A1B2C3D4    解除挂失: 从黑名单移除
下位机 → 上位机    OK                 操作成功
下位机 → 上位机    ERR:BL_FULL        黑名单已满 (50条)
下位机 → 上位机    ERR:NOT_FOUND      解除挂失时 UID 不在黑名单
下位机 → 上位机    ERR:PARSE          UID 格式错误
```

> **注意**: `LOST`/`UNLOST` 不依赖 RC522（无需放卡），上位机可在任意时刻发送。

### 11.3 刷卡时黑名单检查

挂失卡刷卡时：
- **OLED 显示**：× 无效卡 / 已挂失
- **蜂鸣器**：短鸣两声
- **串口上报**：`REC:UID=A1B2C3D4|SID=23041036|LOST\r\n`
- **Flash 记录**：写入一条 status=4 (LOST) 的记录
- **考勤判定**：跳过，不产生正常打卡

### 11.4 恢复出厂 / 清空黑名单

发送 `CLEAR` 仅清除卡数据，黑名单独立存储。如需清空黑名单，需逐条 `UNLOST`。

---

## 12. 时钟界面布局

```
┌──────────────────────────┐
│ 2026年7月11日 周六 设备:001 模式:出入口 │ ← 16px 平滑滚动
│         14:30:25         │ ← 32px 大字居中
│ 杭州 晴 5~12℃  本地温度：25.5℃ │ ← 16px 平滑滚动
└──────────────────────────┘
```

- 滚动速度: `DISP_SCROLL_MS_PER_PX` 控制 (默认 62ms/px ≈ 16px/s)
- 像素级平滑: `GUI_DispStringAt(buf, -subOff, y)` 负 x 渲染
- 天气数据通过 `myQueue01` 队列传递（networkTask → guiTask）
- 天气刷新间隔: `WEATHER_UPDATE_INTERVAL_MS`（默认 1h）
- 温度异步缓存: defaultTask 每秒更新，guiTask 零延迟读取

---

## 13. 显示帧率调参 (`display.h`)

所有显示性能参数集中在 `BSP/display.h` 顶部，修改后重新编译即生效：

```c
#define DISP_FRAME_INTERVAL_MS   60   // GUI 刷新间隔 (≈16.7fps)
#define DISP_SCROLL_MS_PER_PX    62   // 滚动速度 (ms/px)
#define DISP_I2C_TARGET_HZ       400000 // I2C 时钟 (STM32F4 Fast Mode 上限)
```

| 参数 | 作用 | 范围 | 说明 |
|------|------|:---:|------|
| `DISP_FRAME_INTERVAL_MS` | 帧间隔 | 60~100ms | <60ms 花屏（I2C 带宽不足）, >100ms 卡顿感明显 |
| `DISP_SCROLL_MS_PER_PX` | 滚动速率 | 30~120 | 越小越快; 独立于帧率, 不影响稳定性 |
| `DISP_I2C_TARGET_HZ` | I2C 时钟 | 100k/400k | 运行时通过 `DISP_ApplyI2CConfig()` 自动覆盖 CubeMX 默认值 |

### 帧率瓶颈分析

```
每帧 I2C 传输: 8页 × 131B = 1048B @400kHz ≈ 32ms
每帧 CPU 渲染: 文本布局 + 滚动计算 + 字库查找 ≈ 10ms
中断开销余量:                        ≈ 18ms
─────────────────────────────────────────
理论安全下限:                        ≈ 60ms (16.7fps)
```

> **花屏原因**: 帧间隔过小 → 上一帧 I2C 未完成即触发下一帧 → HAL 检测到 I2C 异常 → DeInit+Init 复位 → OLED 进入未初始化状态 → 显示乱码。增大 `DISP_FRAME_INTERVAL_MS` 即可解决。

---

## 14. 构建与烧录

```bash
# 构建 (在 NFC/ 目录下)
make -j4

# 清理
make clean

# 烧录 (VS Code Task: Flash = Build + OpenOCD)
```

---

## 15. 关键数据结构速查

- `SysConfig_t` → 系统配置 (deviceId, attendMode, magic, CRC16)
- `RecordHeader_t` → 记录区头部 (writeOffset, totalCount, uploadOffset)
- `SwipeRecord_t` (32B) → 单条刷卡记录
- `CardCache_t` → LRU 缓存条目 (cardId, eventType, enterTick, accessTick)
- `NFC_CardData_t` → 读卡解析结果 (cardId, empId, points, cardType, statusFlag)
- `DispCardInfo_t` → 刷卡显示信息 (含 eventType, recordStatus, durationSec)
- `DispCardBitmap_t` → 卡面位图缓存 (头像/姓名/部门)
- `DispWeatherCache_t` → 天气滚动缓存 (含字符宽度预计算)
- `NfcCardMsg_t` → NFC 刷卡队列消息 (info + bitmap)

## 16. 已知限制

- 中文字库仅 102 字，天气/日期部分汉字显示为 `?`，需补字模
- TCP 实时数据上报尚未实现（当前为上位机 LIST 拉取模式）
- FPU 未启用 (`configENABLE_FPU = 0`)，浮点运算为软件模拟
