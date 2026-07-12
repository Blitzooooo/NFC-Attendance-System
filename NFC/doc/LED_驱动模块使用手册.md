# LED 驱动模块使用手册

## 1. 驱动概述

LED 驱动是一个通用的 GPIO LED 控制模块，支持多路 LED 独立配置与统一控制。每个 LED 可单独配置 GPIO 端口、引脚和亮灯电平（高电平点亮或低电平点亮），适配共阳极/共阴极两种接法。驱动提供位掩码批量控制与单灯开/关/翻转操作，使用简便。

| 项目 | 说明 |
|------|------|
| 文件 | `led.c`、`led.h` |
| 依赖 | STM32 HAL 库（`HAL_GPIO_WritePin`） |
| 最大 LED 数 | `LED_MAX_COUNT`（7） |

---

## 2. 模块架构

```
┌───────────────────────────────────────┐
│             用户应用层                │
│   LED_On() / LED_Off() / LED_SetLeds()│
└─────────────────┬─────────────────────┘
                  │
┌─────────────────▼─────────────────────┐
│           LED 驱动层                  │
│  ┌──────────────┐  ┌──────────────┐   │
│  │ 配置表        │  │ 状态位掩码    │   │
│  │ s_configs[]  │  │ s_currentMask│   │
│  └──────────────┘  └──────────────┘   │
│         位掩码 → 逐灯写 GPIO           │
└─────────────────┬─────────────────────┘
                  │ HAL_GPIO_WritePin
┌─────────────────▼─────────────────────┐
│           STM32 HAL / 硬件            │
└───────────────────────────────────────┘
```

驱动内部用静态数组 `s_configs[]` 保存各 LED 的硬件配置，用 `s_currentMask` 记录当前亮灭状态。所有写操作最终通过 `LED_SetLeds()` 统一刷新到 GPIO。

---

## 3. 核心数据结构

### Led_OnLevel_t — 亮灯电平

```c
typedef enum {
    LED_ON_LOW = 0,   // 低电平点亮（灌电流，共阳极）
    LED_ON_HIGH = 1,  // 高电平点亮（拉电流，共阴极）
} Led_OnLevel_t;
```

### Led_Config_t — LED 配置结构

```c
typedef struct {
    GPIO_TypeDef  *port;     // GPIO 端口
    uint16_t       pin;      // GPIO 引脚
    Led_OnLevel_t  onLevel;  // 亮灯电平
} Led_Config_t;
```

---

## 4. API 参考

| 函数 | 说明 |
|------|------|
| `void LED_Init(const Led_Config_t *configs, uint8_t count)` | 初始化 LED 驱动，传入配置数组，初始化后全部熄灭 |
| `void LED_SetLeds(uint8_t mask)` | 位掩码统一设置 LED 状态（bit0=LED0，1=亮） |
| `uint8_t LED_GetLeds(void)` | 获取当前 LED 状态位掩码 |
| `void LED_On(uint8_t index)` | 点亮单个 LED |
| `void LED_Off(uint8_t index)` | 熄灭单个 LED |
| `void LED_Toggle(uint8_t index)` | 翻转单个 LED |

---

## 5. 使用示例

### 基本使用

```c
#include "led.h"

/* 1. 定义 LED 配置数组 */
const Led_Config_t ledConfigs[] = {
    {LED1_GPIO_Port, LED1_Pin, LED_ON_LOW},   // LED0: 低电平点亮
    {LED2_GPIO_Port, LED2_Pin, LED_ON_LOW},   // LED1
    {LED3_GPIO_Port, LED3_Pin, LED_ON_LOW},   // LED2
};

/* 2. 初始化 */
LED_Init(ledConfigs, ARRAY_SIZE(ledConfigs));

/* 3. 批量控制 */
LED_SetLeds(0x05);   // 点亮 LED0 和 LED2

/* 4. 单灯控制 */
LED_On(1);           // 点亮 LED1
LED_Off(0);          // 熄灭 LED0
LED_Toggle(2);       // 翻转 LED2
```

### 流水灯示例

```c
/* 在周期任务中实现流水灯 */
uint8_t pos = 0;
void LedFlowTask(void)
{
    LED_SetLeds(1 << pos);
    pos = (pos + 1) % 3;  // 3 个 LED 循环
}
```

---

## 6. NFCAttend 工程实际应用

NFCAttend 考勤系统使用 7 个 LED（L1~L7），低电平点亮（灌电流）。

| LED | 引脚 | 电平 | 功能 |
|-----|------|------|------|
| L1 | PE8 | 低亮 | 刷卡成功指示灯（亮 150ms） |
| L2 | PE9 | 低亮 | 刷卡失败指示灯（亮 100ms） |
| L3 | PE10 | 低亮 | 重复刷卡指示灯（亮 50ms） |
| L4~L7 | PE11~PE14 | 低亮 | 预留 |

### NFCAttend 中的 LED 配置（freertos.c）

```c
static const Led_Config_t s_led_configs[] = {
    {GPIOE, GPIO_PIN_8,  LED_ON_LOW},   /* L1 */
    {GPIOE, GPIO_PIN_9,  LED_ON_LOW},   /* L2 */
    {GPIOE, GPIO_PIN_10, LED_ON_LOW},   /* L3 */
    {GPIOE, GPIO_PIN_11, LED_ON_LOW},   /* L4 */
    {GPIOE, GPIO_PIN_12, LED_ON_LOW},   /* L5 */
    {GPIOE, GPIO_PIN_13, LED_ON_LOW},   /* L6 */
    {GPIOE, GPIO_PIN_14, LED_ON_LOW},   /* L7 */
};

/* 初始化 */
LED_Init(s_led_configs, 7);
LED_SetLeds(0);  /* 全部熄灭 */
```

### NFCAttend 中的声光反馈（nfc_app.c → NFC_Feedback）

```c
void NFC_Feedback(NFC_Event_t evt)
{
    switch (evt) {
    case NFC_EVT_CARD_VALID:
        /* 刷卡成功: L1亮150ms */
        LED_SetLeds(0);
        LED_On(0);  /* L1 */
        Beep_Start(BEEP_C5, 100);
        break;

    case NFC_EVT_CARD_INVALID:
        /* 刷卡失败: L2亮100ms */
        LED_SetLeds(0);
        LED_On(1);  /* L2 */
        Beep_Start(BEEP_A4, 200);
        break;

    case NFC_EVT_CARD_DUPLICATE:
        /* 重复刷卡: L3亮50ms */
        LED_SetLeds(0);
        LED_On(2);  /* L3 */
        Beep_Start(BEEP_C4, 50);
        break;

    case NFC_EVT_CARD_ADMIN:
        /* 管理员卡: L1+L2亮300ms */
        LED_SetLeds(0);
        LED_On(0);
        LED_On(1);
        Beep_Start(BEEP_C5, 150);
        break;

    default:
        break;
    }
}
```

> **设计要点**：NFCAttend 中每次反馈前先调用 `LED_SetLeds(0)` 熄灭所有 LED，再单独点亮对应指示灯，避免不同事件的 LED 状态叠加干扰。

---

## 7. 注意事项

- **索引范围**：`index` 参数范围为 `0 ~ count-1`，超出范围函数内部会忽略，不会崩溃。
- **最大数量**：受 `LED_MAX_COUNT`（默认 7）限制，如需更多 LED 请修改宏定义。
- **电平配置**：务必根据实际硬件电路正确设置 `onLevel`，共阳极接法用 `LED_ON_LOW`，共阴极用 `LED_ON_HIGH`。
- **初始化状态**：`LED_Init()` 后所有 LED 默认熄灭。
