# RTC 驱动模块使用手册

## 1. 驱动概述

RTC 驱动是对 STM32 HAL RTC 的上层封装，提供易用的日期时间读写接口。通过备份寄存器（Backup Register）判断是否首次上电，避免每次复位时重置时间。断电后时间保持需要 VBAT 引脚接电池或并接 VDD。

| 项目 | 说明 |
|------|------|
| 文件 | `bsp_rtc.c`、`bsp_rtc.h` |
| 依赖 | STM32 HAL RTC 库、CubeMX 生成的 `MX_RTC_Init()` / `hrtc` |
| 时间格式 | BCD/BIN 二进制格式（本驱动统一使用 BIN 格式） |
| 断电保持 | 需要 VBAT 供电 |

---

## 2. 模块架构

```
┌───────────────────────────────────────────┐
│               用户应用层                  │
│   BSP_RTC_GetDateTime() / BSP_RTC_Set*()  │
└─────────────────┬─────────────────────────┘
                  │
┌─────────────────▼─────────────────────────┐
│             RTC 驱动层                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │ 读写封装  │ │ 备份寄存器│ │ 日历辅助  │  │
│  │ Get/Set  │ │ 上电判断  │ │ 闰年/星期 │  │
│  └──────────┘ └──────────┘ └──────────┘  │
│         g_rtc_datetime 全局变量           │
└─────────────────┬─────────────────────────┘
                  │ HAL_RTC_GetTime/GetDate/SetTime/SetDate
┌─────────────────▼─────────────────────────┐
│         STM32 HAL RTC / 硬件              │
└───────────────────────────────────────────┘
```

驱动将读取到的时间同时写入全局变量 `g_rtc_datetime`，方便其他任务直接访问，无需重复调用 HAL 接口。

---

## 3. 核心数据结构

### BSP_RTC_DateTime_t — 日期时间结构体

```c
typedef struct {
    uint16_t year;    // 年，完整 4 位，如 2026
    uint8_t  month;   // 月，1-12
    uint8_t  day;     // 日，1-31
    uint8_t  weekday; // 星期，0=周日 1=周一 ... 6=周六
    uint8_t  hour;    // 时，0-23
    uint8_t  minute;  // 分，0-59
    uint8_t  second;  // 秒，0-59
} BSP_RTC_DateTime_t;
```

### 备份寄存器配置

```c
#define BSP_RTC_BKUP_DR     RTC_BKP_DR0      // 使用的备份寄存器
#define BSP_RTC_BKUP_MAGIC  0x5050U          // 魔数，标记已初始化
```

### 全局变量

```c
extern BSP_RTC_DateTime_t g_rtc_datetime;  // 每次 GetDateTime 后自动更新
```

---

## 4. API 参考

### 上电判断

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `uint8_t BSP_RTC_IsFirstPowerOn(void)` | 判断是否首次上电（备份寄存器无魔数） | 1=首次, 0=非首次 |
| `void BSP_RTC_MarkInitialized(void)` | 标记 RTC 已初始化（写入魔数） | 无 |

### 日期时间读写

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `HAL_StatusTypeDef BSP_RTC_GetDateTime(BSP_RTC_DateTime_t *dt)` | 读取日期时间，传 NULL 仅更新全局变量 | HAL_OK 成功 |
| `HAL_StatusTypeDef BSP_RTC_SetDate(uint16_t year, uint8_t month, uint8_t day)` | 设置日期 | HAL_OK 成功 |
| `HAL_StatusTypeDef BSP_RTC_SetTime(uint8_t hour, uint8_t minute, uint8_t second)` | 设置时间 | HAL_OK 成功 |
| `HAL_StatusTypeDef BSP_RTC_SetDateTime(const BSP_RTC_DateTime_t *dt)` | 同时设置日期和时间 | HAL_OK 成功 |

### 日历辅助函数

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `uint8_t BSP_RTC_CalcWeekday(uint16_t year, uint8_t month, uint8_t day)` | 根据日期计算星期 | 0=周日~6=周六 |
| `uint8_t BSP_RTC_IsLeapYear(uint16_t year)` | 判断闰年 | 1=闰年, 0=平年 |
| `uint8_t BSP_RTC_DaysInMonth(uint16_t year, uint8_t month)` | 获取某月最大天数 | 28-31 |

---

## 5. 使用示例

### 基本读写

```c
#include "bsp_rtc.h"

/* 读取当前时间 */
BSP_RTC_DateTime_t dt;
if (BSP_RTC_GetDateTime(&dt) == HAL_OK) {
    printf("当前时间: %04d-%02d-%02d %02d:%02d:%02d 星期%d\r\n",
           dt.year, dt.month, dt.day,
           dt.hour, dt.minute, dt.second, dt.weekday);
}

/* 也可直接访问全局变量 */
printf("全局时间: %02d:%02d:%02d\r\n",
       g_rtc_datetime.hour, g_rtc_datetime.minute, g_rtc_datetime.second);
```

### 设置时间

```c
/* 设置日期和时间 */
BSP_RTC_SetDate(2026, 6, 20);
BSP_RTC_SetTime(14, 30, 0);

/* 或一次性设置 */
BSP_RTC_DateTime_t dt = {
    .year = 2026, .month = 6, .day = 20,
    .hour = 14, .minute = 30, .second = 0
};
BSP_RTC_SetDateTime(&dt);
```

### 首次上电初始化

```c
/* 在 main 中，MX_RTC_Init() 之后调用 */
if (BSP_RTC_IsFirstPowerOn()) {
    /* 首次上电，设置默认时间 */
    BSP_RTC_SetDate(2026, 1, 1);
    BSP_RTC_SetTime(0, 0, 0);
    BSP_RTC_MarkInitialized();  // 标记已初始化
}
```

### 配合 NTP 校时

```c
/* 从 ESP01S NTP 获取时间后写入 RTC */
ESP01S_SetRtcFromNtp(&hrtc);

/* 之后直接使用 RTC 读取 */
BSP_RTC_GetDateTime(&dt);
```

---

## 7. NFCAttend 工程实际应用

NFCAttend 考勤系统使用 RTC 驱动记录考勤时间和显示实时时钟。

### 初始化（freertos.c → StartDefaultTask）

```c
#include "bsp_rtc.h"

void StartDefaultTask(void *argument)
{
    /* 读取 RTC 时间，确认 RTC 正常运行 */
    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);
}
```

### 考勤记录生成时间（nfc_attend.c）

每次刷卡考勤时从 RTC 读取时间戳：

```c
AttendResult_t result;
BSP_RTC_DateTime_t now;
BSP_RTC_GetDateTime(&now);

/* 用 RTC 时间生成考勤记录时间戳 */
result.hour = now.hour;
result.minute = now.minute;
result.second = now.second;
result.day = now.day;
```

### NTP 校时（freertos.c → StartDefaultTask）

NFCAttend 通过 ESP01S 获取 NTP 时间后写入 RTC：

```c
/* ESP01S_Start() 成功后调用 */
if (ESP01S_IsNtpSynced()) {
    ESP01S_SetRtcFromNtp(&hrtc);
    printf("[ESP01S] RTC synced from NTP\r\n");
}
```

### 时间按键设置（freertos.c → StartTaskKey）

按住 K6 进入时间设置模式，K2/K3 加减，K4 切换光标，K6 保存：

```c
if (Key_IsLongPressed(5)) { /* K6长按: 进入时间设置 */
    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);
    s_set_year = dt.year;
    s_set_month = dt.month;
    s_set_day = dt.day;
    s_set_hour = dt.hour;
    s_set_minute = dt.minute;
    s_set_second = dt.second;
    DISP_SetState(DISP_TIME_SETTING);
}

/* 保存时间 */
BSP_RTC_SetDate(s_set_year, s_set_month, s_set_day);
BSP_RTC_SetTime(s_set_hour, s_set_minute, s_set_second);
```

---

## 6. 注意事项

- **读取顺序**：STM32 RTC 要求必须先读 `Time` 再读 `Date`，否则影子寄存器不会更新。驱动内部已正确处理此顺序。
- **首次上电**：备份寄存器在 VBAT 供电下断电不丢失，因此 `IsFirstPowerOn()` 仅在真正首次上电（VBAT 也断电）时返回 1。
- **VBAT 供电**：断电保持时间依赖 VBAT 引脚。若未接电池，每次断电后时间会丢失，需重新设置。
- **星期计算**：`CalcWeekday()` 使用 Tomohiko Sakamoto 算法，返回 0=周日~6=周六，与 HAL RTC 的 WeekDay 字段一致。
- **全局变量**：`g_rtc_datetime` 仅在调用 `BSP_RTC_GetDateTime()` 时更新，不会自动刷新。如需实时时间请周期性调用。
- **年份范围**：HAL RTC 的 Year 字段为 2 位（0-99），驱动内部自动加 2000 转换为 4 位年份。
