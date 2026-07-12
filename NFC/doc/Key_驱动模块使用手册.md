# Key 按键驱动模块使用手册

## 1. 驱动概述

Key 驱动是一个通用按键检测模块，支持多按键独立配置、计数式消抖，以及短按、长按、连按三种事件检测。每个按键可独立配置 GPIO 端口、引脚和有效电平（高电平有效或低电平有效），适配上拉/下拉两种输入方式。

| 项目 | 说明 |
|------|------|
| 文件 | `key.c`、`key.h` |
| 依赖 | STM32 HAL 库（`HAL_GPIO_ReadPin`） |
| 最大按键数 | `KEY_MAX_COUNT`（6） |
| 扫描周期 | `KEY_SCAN_INTERVAL_MS`（10ms） |

### 功能特性

- **计数消抖**：连续 `KEY_DEBOUNCE_COUNT`（2）次检测到同一状态才确认，有效滤除抖动
- **短按**：按下并释放，且未触发长按时，在释放瞬间产生短按事件
- **长按**：按下保持超过 `KEY_LONG_PRESS_MS`（800ms）时触发长按事件
- **连按**：长按后每隔 `KEY_REPEAT_INTERVAL_MS`（100ms）连续触发，适用于数值加减调节
- **组合键**：`Key_AnyPressed()` 返回所有按键的组合状态位掩码

---

## 2. 模块架构

```
┌────────────────────────────────────────────┐
│                用户应用层                  │
│  Key_IsShortPressed() / Key_IsLongPressed()│
└──────────────────┬─────────────────────────┘
                   │ 查询事件（读后自动清除）
┌──────────────────▼─────────────────────────┐
│              Key 驱动层                    │
│  ┌────────────┐  ┌────────────────────┐    │
│  │ 配置表      │  │ 状态表              │    │
│  │ s_configs[]│  │ s_states[]          │    │
│  └────────────┘  │  ├─ 消抖计数         │    │
│                  │  ├─ 稳定状态         │    │
│                  │  ├─ 按下时长         │    │
│                  │  └─ 事件位掩码       │    │
│  Key_Scan(): 读GPIO→消抖→事件判定          │
└──────────────────┬─────────────────────────┘
                   │ HAL_GPIO_ReadPin
┌──────────────────▼─────────────────────────┐
│           STM32 HAL / 硬件                 │
└────────────────────────────────────────────┘
```

### 消抖与事件判定流程

```
读取GPIO → 转换为逻辑状态(按下/释放)
    │
    ▼
计数消抖：状态一致则计数+1，变化则开始计数
    │ 计数 >= DEBOUNCE_COUNT → 确认状态翻转
    ▼
事件判定：
  按下边沿 → 产生 PRESSED 事件
  持续按下 → 累计时长 → 超过800ms → LONG_PRESS 事件
                            │ 继续每100ms → REPEAT 事件
  释放边沿 → 若未长按 → SHORT_PRESS 事件
```

---

## 3. 核心数据结构

### Key_ActiveLevel_t — 有效电平

```c
typedef enum {
    KEY_ACTIVE_LOW = 0,   // 上拉输入，低电平有效
    KEY_ACTIVE_HIGH = 1,  // 下拉输入，高电平有效
} Key_ActiveLevel_t;
```

### Key_Event_t — 事件类型（位掩码）

```c
typedef enum {
    KEY_EVENT_NONE        = 0x00,  // 无事件
    KEY_EVENT_PRESSED     = 0x01,  // 按下（消抖后）
    KEY_EVENT_SHORT_PRESS = 0x02,  // 短按（按下并释放）
    KEY_EVENT_LONG_PRESS  = 0x04,  // 长按（首次触发）
    KEY_EVENT_REPEAT      = 0x08,  // 长按连发
} Key_Event_t;
```

### Key_Config_t — 按键配置结构

```c
typedef struct {
    GPIO_TypeDef    *port;         // GPIO 端口
    uint16_t         pin;          // GPIO 引脚
    Key_ActiveLevel_t activeLevel; // 有效电平
} Key_Config_t;
```

### Key_State_t — 按键内部状态

```c
typedef struct {
    uint8_t  debounceCnt;   // 消抖计数器
    uint8_t  stableState;   // 消抖后稳定状态（0=释放，1=按下）
    uint8_t  prevState;     // 上一次稳定状态
    uint16_t pressDuration; // 按下持续时间（ms）
    uint8_t  longPressed;   // 是否已触发长按
    uint8_t  events;        // 待处理事件（位掩码）
} Key_State_t;
```

### 编译配置宏

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `KEY_SCAN_INTERVAL_MS` | 10 | 扫描间隔（ms），需与调用周期一致 |
| `KEY_DEBOUNCE_COUNT` | 2 | 消抖有效计数 |
| `KEY_LONG_PRESS_MS` | 800 | 长按判定时长（ms） |
| `KEY_REPEAT_INTERVAL_MS` | 100 | 长按连发间隔（ms） |
| `KEY_MAX_COUNT` | 6 | 最大按键数量 |

---

## 4. API 参考

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `void Key_Init(const Key_Config_t *configs, uint8_t count)` | 初始化按键驱动 | 无 |
| `void Key_Scan(void)` | 按键扫描，需周期调用（间隔10ms） | 无 |
| `uint8_t Key_IsPressed(uint8_t index)` | 查询按键是否按下 | 1=按下，0=释放 |
| `uint8_t Key_IsShortPressed(uint8_t index)` | 查询并清除短按事件 | 1=短按发生，0=无 |
| `uint8_t Key_IsLongPressed(uint8_t index)` | 查询并清除长按事件 | 1=长按发生，0=无 |
| `uint8_t Key_IsRepeat(uint8_t index)` | 查询并清除连发事件 | 1=连发触发，0=无 |
| `uint16_t Key_AnyPressed(void)` | 获取所有按键组合状态 | 位掩码，bit0=按键0... |
| `void Key_ClearAllEvents(void)` | 清除所有待处理事件 | 无 |

> **事件清除机制**：`Key_IsShortPressed/IsLongPressed/IsRepeat` 为"读后即清"语义，查询到事件后自动清除，不会重复触发。

---

## 5. 使用示例

### 基本使用

```c
#include "key.h"

/* 1. 定义按键配置 */
const Key_Config_t keyConfigs[] = {
    {KEY1_GPIO_Port, KEY1_Pin, KEY_ACTIVE_LOW},   // K1: 低电平有效
    {KEY2_GPIO_Port, KEY2_Pin, KEY_ACTIVE_LOW},   // K2
    {KEY3_GPIO_Port, KEY3_Pin, KEY_ACTIVE_LOW},   // K3
    {KEY4_GPIO_Port, KEY4_Pin, KEY_ACTIVE_LOW},   // K4
};

/* 2. 初始化 */
Key_Init(keyConfigs, 4);

/* 3. 在周期任务中扫描（10ms周期） */
void KeyTask(void *arg)
{
    for (;;) {
        Key_Scan();
        osDelay(10);  // FreeRTOS 10ms 周期
    }
}
```

### 事件处理

```c
void HandleKeys(void)
{
    /* 短按 */
    if (Key_IsShortPressed(0)) {
        printf("K1 短按\r\n");
    }

    /* 长按首次触发 */
    if (Key_IsLongPressed(1)) {
        printf("K2 长按\r\n");
    }

    /* 长按连发（适合数值调节） */
    if (Key_IsRepeat(1)) {
        volume++;  // 长按K2持续增加音量
    }
}
```

### 组合键检测

```c
uint16_t mask = Key_AnyPressed();
if (mask == 0x03) {
    /* 按键0和按键1同时按下 */
    printf("组合键 K1+K2\r\n");
}
```

---

## 6. NFCAttend 工程实际应用

NFCAttend 考勤系统使用 6 个按键（K1~K6），配置如下：

| 按键 | 引脚 | 有效电平 | 功能 |
|------|------|---------|------|
| K1 | PE1 | 低有效（上拉） | 正常模式：管理员光标切换；时间设置：切光标 |
| K2 | PE2 | 低有效（上拉） | 正常模式：无效；时间设置/管理员设置：数值+ |
| K3 | PE3 | 低有效（上拉） | 正常模式：无效；时间设置/管理员设置：数值- |
| K4 | PE4 | 低有效（上拉） | 正常模式：无效；时间设置：切光标 |
| K5 | PE5 | 高有效（下拉） | 正常模式：无效；管理员模式：备用 |
| K6 | PE6 | 高有效（下拉） | 短按：返回时钟/保存退出；长按：进入时间设置 |

### NFCAttend 中的按键配置和初始化（freertos.c）

```c
/* 按键配置数组: K1~K4 上拉低有效, K5~K6 下拉高有效 */
static const Key_Config_t s_key_configs[] = {
    {GPIOE, GPIO_PIN_1,  KEY_ACTIVE_LOW},   /* K1 */
    {GPIOE, GPIO_PIN_2,  KEY_ACTIVE_LOW},   /* K2 */
    {GPIOE, GPIO_PIN_3,  KEY_ACTIVE_LOW},   /* K3 */
    {GPIOE, GPIO_PIN_4,  KEY_ACTIVE_LOW},   /* K4 */
    {GPIOE, GPIO_PIN_5,  KEY_ACTIVE_HIGH},  /* K5 */
    {GPIOE, GPIO_PIN_6,  KEY_ACTIVE_HIGH},  /* K6 */
};

void StartDefaultTask(void *argument)
{
    /* 初始化按键驱动 */
    Key_Init(s_key_configs, 6);
}
```

### NFCAttend 中的按键处理任务（freertos.c → StartTaskKey）

```c
void StartTaskKey(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    for(;;)
    {
        Key_Scan();

        DisplayState_t state = DISP_GetState();

        if (state == DISP_TIME_SETTING) {
            /* 时间设置模式 */
            if (Key_IsShortPressed(5)) { /* K6: 保存并退出 */
                BSP_RTC_SetDate(s_set_year, s_set_month, s_set_day);
                BSP_RTC_SetTime(s_set_hour, s_set_minute, s_set_second);
                s_time_setting_active = 0;
                DISP_SetState(DISP_CLOCK);
            }
            if (Key_IsShortPressed(1)) { /* K2: 数值+ */
                /* 根据光标位置增加对应字段 */
            }
            if (Key_IsShortPressed(2)) { /* K3: 数值- */
                /* 根据光标位置减少对应字段 */
            }
            if (Key_IsShortPressed(3)) { /* K4: 切换光标 */
                s_set_cursor = (s_set_cursor + 1) % 6;
            }
        } else if (state == DISP_ADMIN_SETTING) {
            /* 管理员设置模式 */
            if (Key_IsShortPressed(5)) { /* K6: 保存并退出 */
                NFC_ATTEND_SetAdminConfig(s_admin_edit_dev_id, s_admin_edit_mode);
                s_admin_active = 0;
                DISP_SetState(DISP_CLOCK);
            }
            if (Key_IsShortPressed(0)) { /* K1: 切换光标 */
                s_admin_cursor = !s_admin_cursor;
            }
            if (Key_IsShortPressed(1)) { /* K2: 数值+ */
                /* 增加设备编号或考勤模式 */
            }
            if (Key_IsShortPressed(2)) { /* K3: 数值- */
                /* 减少设备编号或考勤模式 */
            }
        } else {
            /* 正常模式 */
            if (Key_IsLongPressed(5)) { /* K6长按: 进入时间设置 */
                DISP_SetState(DISP_TIME_SETTING);
            }
            if (Key_IsShortPressed(5)) { /* K6短按: 返回时钟 */
                DISP_SetState(DISP_CLOCK);
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(KEY_SCAN_INTERVAL_MS));
    }
}
```

> **设计要点**：NFCAttend 的按键事件处理分散在 `StartTaskKey` 中，通过 `DISP_GetState()` 获取当前界面状态，根据不同状态执行不同功能。K6 兼具短按（返回/保存）和长按（进入设置）两个功能。

---

## 7. 注意事项

- **扫描周期**：`Key_Scan()` 必须以 `KEY_SCAN_INTERVAL_MS`（10ms）的固定周期调用，否则消抖和时长判定会不准确。建议放在 FreeRTOS 定时任务或硬件定时器回调中。
- **事件清除**：短按/长按/连发事件为"读后即清"，每个事件只能被查询到一次。若不查询，事件会保留在内部状态中直到被读取或调用 `Key_ClearAllEvents()`。
- **长按与短按互斥**：一旦触发长按事件，释放时不会再产生短按事件。
- **连发机制**：连发通过重置 `pressDuration` 为 `KEY_LONG_PRESS_MS` 实现，每 `KEY_REPEAT_INTERVAL_MS` 触发一次。
- **电平配置**：务必根据硬件电路设置正确的 `activeLevel`，上拉输入用 `KEY_ACTIVE_LOW`，下拉输入用 `KEY_ACTIVE_HIGH`。
- **GPIO 模式**：按键引脚需在 CubeMX 中配置为输入模式（上拉/下拉），与 `activeLevel` 对应。
