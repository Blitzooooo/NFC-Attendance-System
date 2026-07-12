# OLED (SSD1306/SH1106) 显示驱动使用手册

## 1. 驱动概述

OLED 显示驱动基于 SSD1306/SH1106 控制芯片，提供 128×64 像素的单色显示能力。驱动支持硬件 I2C 和软件模拟 I2C 两种通信方式，并自动检测芯片类型（SSD1306 或 SH1106）。上层封装了一套精简 GUI 库（GUISlim），提供文本显示、图形绘制（点/线/矩形/圆/三角形）、颜色控制等功能，并支持 UTF-8/GBK 中文显示。

| 项目 | 说明 |
|------|------|
| 文件 | `ssd1306.c/h`、`ssd1306_i2c.c/h`、`GUISlim.c`、`GUI.h`、`F08_ASCII.c` |
| 通信方式 | 硬件 I2C（默认）或软件模拟 I2C（定义 `OLED_I2C_HARD` 控制） |
| 分辨率 | 128 × 64 |
| I2C 地址 | `0x78`（7位地址 0x3C 左移1位） |
| 显存机制 | 帧缓冲区（`SSD1306_Buffer`），修改后需调用刷新函数 |

---

## 2. 模块架构

```
┌───────────────────────────────────────────────┐
│                 用户应用层                     │
│   GUI_DispStringAt() / GUI_DrawCircle() ...   │
├───────────────────────────────────────────────┤
│            GUI 精简库 (GUISlim.c)              │
│   文本绘制 / 图形绘制 / 颜色管理 / 字体管理     │
│   UTF-8→GBK 转码 / 帧缓冲操作                  │
├───────────────────────────────────────────────┤
│         SSD1306 驱动层 (ssd1306.c)             │
│   帧缓冲区 SSD1306_Buffer[1024]                │
│   底层绘图: DrawPixel / Fill / UpdateScreen    │
│   芯片自检测: SSD1306 / SH1106                 │
├───────────────────────────────────────────────┤
│         I2C 通信层 (ssd1306_i2c.c)             │
│   硬件I2C (HAL_I2C_Mem_Write) 或 软件模拟I2C   │
├───────────────────────────────────────────────┤
│              STM32 HAL / 硬件                  │
└───────────────────────────────────────────────┘
```

**核心设计要点**：
- **帧缓冲架构**：所有绘制操作先写入内存缓冲区 `SSD1306_Buffer`（128×64/8 = 1024 字节），需调用 `SSD1306_UpdateScreen()` 或 `GUI_Update()` 才会刷新到屏幕。
- **芯片自适应**：`SSD1306_init()` 中自动检测芯片类型，SH1106 与 SSD1306 的显存列偏移不同，驱动内部自动处理。
- **双 I2C 模式**：通过编译宏 `OLED_I2C_HARD` 切换硬件 I2C 和软件模拟 I2C。

---

## 3. 核心数据结构

### GUI_COLOR_t — 颜色枚举

```c
typedef enum {
    GUI_COLOR_BLACK = 0x00,  // 黑色（不点亮像素）
    GUI_COLOR_WHITE = 0x01   // 白色（点亮像素）
} GUI_COLOR_t;
```

### SSD1306 配置常量

```c
#define SSD1306_WIDTH       128    // 显示宽度（像素）
#define SSD1306_HEIGHT      64     // 显示高度（像素）
#define SSD1306_I2C_ADDR    0x78   // I2C 设备地址
#define OLED_CMD            0x00   // 命令控制字节
#define OLED_DATA           0x40   // 数据控制字节
```

### GUI_CONTEXT — GUI 上下文

```c
typedef struct {
    LCD_RECT  ClipRect;    // 裁剪区域
    U8        TextStyle;   // 文本样式
    U8        PenSize;     // 画笔大小
    GUI_COLOR DrawColor;   // 当前绘制颜色
    I16P      DispPosX;    // 文本光标 X
    I16P      DispPosY;    // 文本光标 Y
    // ... 其他字段
} GUI_CONTEXT;
```

---

## 4. API 参考

### 4.1 初始化与刷新

| 函数 | 说明 |
|------|------|
| `int GUI_Init(void)` | 初始化 GUI 上下文和 OLED 硬件（内部调用 `SSD1306_init()`） |
| `void GUI_Update(void)` | 将帧缓冲区刷新到屏幕（等同 `SSD1306_UpdateScreen()`） |
| `void SSD1306_init(void)` | 初始化 OLED 显示屏（自动检测芯片类型） |
| `void SSD1306_UpdateScreen(void)` | 刷新显存到屏幕 |
| `void SSD1306_ON(void)` | 开启 OLED 显示 |
| `void SSD1306_OFF(void)` | 关闭 OLED 显示 |

### 4.2 颜色与清屏

| 函数 | 说明 |
|------|------|
| `void GUI_SetColor(GUI_COLOR color)` | 设置绘制颜色（`GUI_COLOR_WHITE`/`GUI_COLOR_BLACK`） |
| `void GUI_Clear(void)` | 清屏并重置文本光标 |
| `void GUI_ClearRect(int x0, int y0, int x1, int y1)` | 清除指定矩形区域 |
| `void SSD1306_Fill(GUI_COLOR color)` | 用指定颜色填充整个屏幕 |

### 4.3 文本显示

| 函数 | 说明 |
|------|------|
| `void GUI_DispString(const char *s)` | 在当前光标位置显示字符串 |
| `void GUI_DispStringAt(const char *s, int x, int y)` | 在指定坐标显示字符串 |
| `void GUI_DispStringHCenterAt(const char *s, int x, int y)` | 水平居中显示字符串 |
| `void GUI_DispStringInRect(const char *s, GUI_RECT *pRect, int Flags)` | 在矩形区域内显示字符串 |
| `char GUI_GotoXY(int x, int y)` | 设置文本光标位置 |
| `const GUI_FONT *GUI_SetFont(const GUI_FONT *pNewFont)` | 设置当前字体 |

### 4.4 图形绘制

| 函数 | 说明 |
|------|------|
| `void GUI_DrawPixel(int x, int y)` | 绘制单个像素点 |
| `void GUI_DrawPoint(int x, int y, GUI_COLOR c)` | 绘制指定颜色像素点 |
| `void GUI_DrawLine(int x0, int y0, int x1, int y1)` | 绘制直线 |
| `void GUI_DrawRect(int x0, int y0, int x1, int y1)` | 绘制矩形边框 |
| `void GUI_FillRect(int x0, int y0, int x1, int y1)` | 绘制填充矩形 |
| `void GUI_DrawCircle(int x0, int y0, int r)` | 绘制圆形边框 |
| `void GUI_FillCircle(int x0, int y0, int r)` | 绘制填充圆形 |
| `void SSD1306_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, GUI_COLOR c)` | 底层直线绘制 |
| `void SSD1306_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, GUI_COLOR c)` | 底层矩形绘制 |
| `void SSD1306_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, GUI_COLOR c)` | 底层填充矩形 |
| `void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r, GUI_COLOR c)` | 底层圆形绘制 |
| `void SSD1306_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, GUI_COLOR c)` | 底层填充圆形 |

---

## 5. 使用示例

### 基本初始化与文本显示

```c
#include "gui.h"

void OledDemo(void)
{
    /* 1. 初始化 GUI 和 OLED */
    GUI_Init();

    /* 2. 清屏 */
    GUI_Clear();

    /* 3. 显示文本 */
    GUI_DispStringAt("Hello OLED!", 0, 0);       // 左上角
    GUI_DispStringHCenterAt("Center", 64, 16);   // 水平居中

    /* 4. 刷新到屏幕 */
    GUI_Update();
}
```

### 图形绘制

```c
void OledDrawDemo(void)
{
    GUI_Init();
    GUI_Clear();

    GUI_SetColor(GUI_COLOR_WHITE);

    /* 绘制矩形边框 */
    GUI_DrawRect(0, 0, 127, 63);

    /* 绘制填充圆形 */
    GUI_FillCircle(64, 32, 15);

    /* 绘制直线 */
    GUI_DrawLine(0, 0, 127, 63);

    /* 刷新 */
    GUI_Update();
}
```

### 动态更新显示

```c
/* 在周期任务中更新显示内容 */
void DisplayTask(void)
{
    char buf[24];

    GUI_Clear();
    GUI_DispStringAt("System Running", 0, 0);
    sprintf(buf, "Tick: %lu", HAL_GetTick());
    GUI_DispStringAt(buf, 0, 16);
    GUI_Update();
}
```

---

## 7. NFCAttend 工程实际应用

NFCAttend 考勤系统使用 OLED 显示实时时钟、刷卡结果、管理员设置界面等。上层封装了 `display.c/h` 模块管理所有显示场景。

### 显示初始化（display.c → DISP_Init）

```c
void DISP_Init(void)
{
    GUI_Init();
    GUI_SetFont(&GUI_Font8_ASCII);

    /* 开机启动画面 */
    DISP_SetState(DISP_STARTUP);
    DISP_ShowStartup();  // LOGO 2秒 + 信息页2秒
}
```

### 时钟界面（display.c → DISP_ShowClock）

```c
void DISP_ShowClock(void)
{
    char buf[24];
    BSP_RTC_DateTime_t dt;
    BSP_RTC_GetDateTime(&dt);

    GUI_Clear();

    /* 显示日期 */
    sprintf(buf, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
    GUI_DispStringAt(buf, 0, 0);

    /* 显示大字体时间 */
    sprintf(buf, "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
    GUI_DispStringAt(buf, 0, 24);

    /* 刷新 OLED */
    SSD1306_UpdateScreen();
}
```

### 刷卡结果界面

```c
void DISP_ShowCardInfo(CardInfo_t *pInfo, uint8_t isEntry,
                        uint8_t duration, uint8_t verifyOk)
{
    GUI_Clear();
    GUI_DispStringAt("Card Info", 0, 0);
    /* 显示工号、卡号、考勤模式等 */
    GUI_DispStringAt(buf, 0, 16);
    SSD1306_UpdateScreen();
    DISP_SetState(DISP_CARD_INFO);  // 3秒后自动返回时钟
}
```

### GUI 任务管理（freertos.c → StartTaskGui）

```c
void StartTaskGui(void *argument)
{
    uint32_t last_clock_tick = 0;

    for(;;)
    {
        /* 检查 NFC 事件通知（通过任务通知） */
        if (osThreadFlagsWait(0x01, osFlagsWaitAny, 0) == 0x01) {
            /* 根据 GUI 事件类型更新显示 */
            DISP_ShowCardInfo(&card_info, ...);
        }

        /* 时钟界面每秒刷新 */
        if (HAL_GetTick() - last_clock_tick >= 1000) {
            DISP_ShowClock();
            last_clock_tick = HAL_GetTick();
        }

        osDelay(100);
    }
}
```

> **设计要点**：NFCAttend 将 GUI 操作统一放在 `guiTask` 中处理，NFC 事件通过任务通知（`osThreadFlagsSet`）从 `nfcTask` 传递到 `guiTask`，确保所有显示更新在同一任务上下文中执行，避免并发冲突。

---

## 6. 注意事项

- **必须刷新**：所有绘制操作只修改内存帧缓冲区，必须调用 `GUI_Update()` 或 `SSD1306_UpdateScreen()` 才会显示到屏幕。
- **I2C 模式选择**：通过 `ssd1306.h` 中的 `#define OLED_I2C_HARD` 宏选择硬件 I2C（取消注释）或软件 I2C（注释掉）。硬件 I2C 模式下使用 `hi2c1`，需在 CubeMX 中配置 I2C1。
- **显示方向**：通过 `ssd1306.c` 中的 `OLED_FLIP_X` 和 `OLED_FLIP_Y` 宏控制 X/Y 轴翻转（默认均翻转）。
- **中文显示**：驱动内置 UTF-8 到 GBK 的转码表，支持中文字符显示，需确保字体文件包含对应中文字模。
- **初始化延时**：`SSD1306_init()` 内部有上电等待延时，确保模块稳定后再初始化。
- **FreeRTOS**：驱动使用了 `osDelay`，在 FreeRTOS 任务中调用 `GUI_Init()` 时需确保调度器已启动或使用裸机延时。
- **坐标系**：原点 (0,0) 在左上角，X 向右增大，Y 向下增大，范围为 0~127 (X) 和 0~63 (Y)。
