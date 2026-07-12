/**
 * @file    display.c
 * @brief   OLED 显示模块实现 - 状态机驱动
 * @details 四个核心状态: CLOCK / RESULT / TIME_SET / ADMIN_SET
 *          所有 GUI 渲染函数集中在此, 仅在 guiTask 上下文中调用.
 */

#include "display.h"
#include "GUI.h"
#include "ssd1306.h"
#include "bsp_rtc.h"
#include "ds18B20.h"
#include "w25qxx.h"
#include "record.h"
#include "i2c.h"
#include <stdio.h>
#include <string.h>

/* ====== 外部字库 & 位图声明 ====== */
extern GUI_FLASH const GUI_FONT GUI_FontHZ_SimSun_16;
extern GUI_FLASH const GUI_FONT GUI_FontHZ_SimSun_32;
extern GUI_CONST_STORAGE GUI_BITMAP bmHDU;

/* ====== 模块内部变量 ====== */

/** 中文星期表 (0=周日 ~ 6=周六) */
static const char *s_weekdays_cn[] = {
    "\xe5\x91\xa8\xe6\x97\xa5",
    "\xe5\x91\xa8\xe4\xb8\x80",
    "\xe5\x91\xa8\xe4\xba\x8c",
    "\xe5\x91\xa8\xe4\xb8\x89",
    "\xe5\x91\xa8\xe5\x9b\x9b",
    "\xe5\x91\xa8\xe4\xba\x94",
    "\xe5\x91\xa8\xe5\x85\xad",
};

/** 刷卡信息共享缓冲 (nfcTask 写入 -> guiTask 读取) */
static DispCardInfo_t   s_cardInfo;
static DispCardBitmap_t s_cardBitmap;  /* 卡片位图缓存 */
static DispState_t      s_state = DISP_STATE_CLOCK;
static DispAdminCfg_t   s_adminCfg = {1, 2};  /* 默认设备1, 出入口模式 */

/** 考勤模式名称 */
static const char *s_modeNames[] = {
    "\xe5\x85\xa5\xe5\x8f\xa3",       /* 入口 */
    "\xe5\x87\xba\xe5\x8f\xa3",       /* 出口 */
    "\xe5\x87\xba\xe5\x85\xa5\xe5\x8f\xa3", /* 出入口 */
};

/** 天气滚动缓存 (defaultTask 写入, guiTask DISP_ShowClock 读取) */
static DispWeatherCache_t s_weatherCache;

/** NTP 时间偏差 (networkTask 写入, nfcTask 读取) */
static int32_t s_timeOffset = (int32_t)TIME_OFFSET_UNCALIBRATED;
static struct {
    BSP_RTC_DateTime_t dt;      /* 编辑中的时间副本 */
    uint8_t cursor;             /* 光标位置 0=年 1=月 2=日 3=时 4=分 5=秒 */
    uint32_t enterTick;         /* 进入时间 */
} s_timeSet;

/** 管理员设置内部状态 */
static struct {
    uint8_t cursor;             /* 0=设备ID 1=考勤模式 */
    uint32_t enterTick;         /* 进入时间 */
    DispAdminCfg_t backup;      /* 进入时的配置副本, 取消时恢复 */
} s_adminSet;

/* (CRC16_Calc 已迁移至 record.c) */

/* ====== RTC 时间戳工具 ====== */

/** 每月天数 (非闰年) */
static const uint8_t s_daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 * @brief  将 BSP_RTC_DateTime_t 转为距 2024-01-01 00:00:00 的秒数
 * @note   支持跨天时长计算, uint32_t 可覆盖 ~136 年
 */
uint32_t DISP_DateTimeToSeconds(const BSP_RTC_DateTime_t *dt)
{
    uint32_t days = 0;
    uint16_t y;

    /* 累加整年天数 */
    for (y = 2024; y < dt->year; y++) {
        days += BSP_RTC_IsLeapYear(y) ? 366U : 365U;
    }

    /* 累加当年整月天数 */
    for (uint8_t m = 1; m < dt->month; m++) {
        days += s_daysInMonth[m - 1];
        if (m == 2 && BSP_RTC_IsLeapYear(dt->year)) days++;
    }

    /* 累加当月天数 */
    days += (uint32_t)(dt->day - 1);

    return days * 86400U + (uint32_t)dt->hour * 3600U
                         + (uint32_t)dt->minute * 60U
                         + (uint32_t)dt->second;
}

/* (Cache_*, Config_*, Record_* 已迁移至 record.c) */

const DispAdminCfg_t *DISP_GetAdminCfg(void) { return &s_adminCfg; }

/** GUI 任务句柄 (由 freertos.c 在初始化时设置) */
extern osThreadId_t guiTaskHandle;

/* ====== 内部辅助: 绘制 LOGO & 信息页 ====== */

static void drawLogo(void);
static void drawInfoPage(void);

static void drawLogo(void)
{
    GUI_SetColor(GUI_COLOR_WHITE);
    GUI_Clear();
    GUI_DrawBitmap((GUI_BITMAP *)&bmHDU, 32, 0);
}

void DISP_ShowLogo(void)
{
    drawLogo();
    GUI_Update();
    osDelay(2000);
}

void DISP_ShowInfoPage(void)
{
    drawInfoPage();
    GUI_Update();
    osDelay(2000);
}

/* ====== 内部辅助: 绘制信息页 ====== */

static void drawInfoPage(void)
{
    GUI_Clear();
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);

    GUI_DispStringHCenterAt(
        "2026\xe7\xbb\xbc\xe5\x90\x88\xe8\xae\xbe\xe8\xae\xa1\xe4\xba\x8c", 64, 0);
    GUI_DispStringHCenterAt(
        "NFC\xe8\x80\x83\xe5\x8b\xa4\xe7\xb3\xbb\xe7\xbb\x9f", 64, 16);
    GUI_DispStringHCenterAt(
        "23041036\xe6\x9d\x9c\xe6\x98\xb1\xe8\xbe\xb0", 64, 32);
    GUI_DispStringHCenterAt(
        "23041035\xe5\xbc\xa0\xe7\x83\xa8", 64, 48);
}


/* ====== 公开 API: 初始化 & 状态管理 ====== */

/* ====== OLED 防卡顿看门狗 ====== */

/** 超过此时间(ms)无刷新视为卡顿, 触发 OLED 硬件复位 */
#define DISP_STUCK_TIMEOUT_MS  3000

/** 最后一次刷新时刻 (FreeRTOS tick) */
static uint32_t s_lastFrameTick = 0;

/**
 * @brief  喂狗: 记录当前帧刷新时刻
 * @note   每帧渲染完成后调用 (guiTask 上下文)
 */
void DISP_FeedWatchdog(void)
{
    s_lastFrameTick = osKernelGetTickCount();
}

/**
 * @brief  检测 OLED 是否卡顿
 * @return 1=卡顿 (超过 DISP_STUCK_TIMEOUT_MS 未刷新), 0=正常
 */
uint8_t DISP_CheckStuck(void)
{
    if (s_lastFrameTick == 0) return 0;  /* 尚未完成首帧渲染 */
    uint32_t elapsed = osKernelGetTickCount() - s_lastFrameTick;
    return (elapsed >= DISP_STUCK_TIMEOUT_MS) ? 1 : 0;
}

/**
 * @brief  重置 OLED: 硬件复位 + 重新初始化芯片和 GUI + 清屏刷新
 * @note   在检测到卡顿后调用, 恢复 OLED 正常显示
 */
void DISP_ResetOLED(void)
{
    SSD1306_init();       /* 重新初始化 OLED 芯片 (含自检+配置) */
    GUI_Init();           /* 重新初始化 GUI 上下文 */
    GUI_Clear();          /* 清空帧缓冲 */
    GUI_SetColor(GUI_COLOR_WHITE);
    GUI_Update();         /* 立即刷新一帧黑屏, 清除可能的花屏 */
    DISP_FeedWatchdog();  /* 重置看门狗时间戳 */
}

/* ================================================================ */

void DISP_Init(void)
{
    memset(&s_cardInfo, 0, sizeof(s_cardInfo));
    memset(&s_cardBitmap, 0, sizeof(s_cardBitmap));
    s_state = DISP_STATE_CLOCK;
    s_adminCfg.deviceId = 1;
    s_adminCfg.attendMode = 2;  /* 默认出入口模式 */
    s_lastFrameTick = 0;  /* 重置看门狗时间戳 */
}

/**
 * @brief  应用 I2C 时钟配置 (覆盖 CubeMX 默认值)
 * @note   在 MX_I2C1_Init() 之后调用, 确保 I2C 频率与 DISP_I2C_TARGET_HZ 一致
 */
void DISP_ApplyI2CConfig(void)
{
    if (hi2c1.Init.ClockSpeed != DISP_I2C_TARGET_HZ) {
        hi2c1.Init.ClockSpeed = DISP_I2C_TARGET_HZ;
        HAL_I2C_DeInit(&hi2c1);
        HAL_I2C_Init(&hi2c1);
    }
}

DispState_t DISP_GetState(void)
{
    return s_state;
}

void DISP_SetState(DispState_t state)
{
    /* 从管理员设置退出且非 K6 保存 → 恢复旧配置 */
    if (s_state == DISP_STATE_ADMIN_SET && state != DISP_STATE_ADMIN_SET) {
        s_adminCfg = s_adminSet.backup;
    }
    s_state = state;
    /* 进入时间设置时复制当前 RTC 值 */
    if (state == DISP_STATE_TIME_SET) {
        BSP_RTC_GetDateTime(&s_timeSet.dt);
        s_timeSet.cursor = 0;
        s_timeSet.enterTick = osKernelGetTickCount();
    }
    /* 进入管理员设置时记录时间 */
    if (state == DISP_STATE_ADMIN_SET) {
        s_adminSet.cursor = 0;
        s_adminSet.enterTick = osKernelGetTickCount();
        s_adminSet.backup = s_adminCfg;  /* 保存副本, 取消时恢复 */
    }
}

void DISP_RunStartup(void)
{
    drawLogo();
    GUI_Update();
    osDelay(2000);  /* LOGO 显示 2s */

    drawInfoPage();
    GUI_Update();
    osDelay(1000);
}

/**
 * @brief  显示系统初始化进度信息 (OLED 居中单行)
 * @param  msg  中文提示字符串
 */
void DISP_ShowInitMsg(const char *msg)
{
    GUI_Clear();
    GUI_SetColor(GUI_COLOR_WHITE);
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);
    GUI_DispStringHCenterAt("System Init", 64, 8);
    GUI_DispStringHCenterAt(msg, 64, 32);
    GUI_Update();
}

/* ====== 公开 API: CLOCK 状态 ====== */

/**
 * @brief  预计算 UTF-8 字符串中每个字符的字节长度和像素宽度
 * @note   SimSun_16 字体: ASCII→8px, CJK(3B UTF-8)→16px
 */
static void computeCharWidths(const char *str,
                              uint8_t *charByteLen, uint8_t *charPixelW,
                              uint8_t *charCount, uint16_t *totalW,
                              uint8_t maxChars)
{
    const uint8_t *p = (const uint8_t *)str;
    uint16_t w = 0;
    uint8_t  n = 0;

    while (*p && n < maxChars) {
        uint8_t blen = 1;
        uint8_t pw   = 8;

        if (*p <= 0x7F) {
            blen = 1; pw = 8;
        } else if ((*p & 0xE0) == 0xC0) {
            blen = 2; pw = 8;
        } else if ((*p & 0xF0) == 0xE0) {
            blen = 3; pw = 16;
        } else if ((*p & 0xF8) == 0xF0) {
            blen = 4; pw = 16;
        }

        charByteLen[n] = blen;
        charPixelW[n]  = pw;
        w += pw;
        n++;
        p += blen;
    }

    *charCount = n;
    *totalW    = w;
}

/** 单行滚动文本拼接 + 渲染 (负 x 平滑滚动) */
static void renderScrollLine(const char *src, int srcLen,
                             const uint8_t *charBL, const uint8_t *charPW,
                             uint8_t charCnt, int totalW,
                             int scrollOff, int yPos)
{
    /* 找到 scrollOff 对应的起始字符 */
    int charIdx = 0, byteOff = 0, accumW = 0;
    while (charIdx < charCnt &&
           accumW + charPW[charIdx] <= scrollOff) {
        accumW  += charPW[charIdx];
        byteOff += charBL[charIdx];
        charIdx++;
    }
    int subOff = scrollOff - accumW;
    if (subOff < 0) subOff = 0;

    /* 拼接显示缓冲 */
    static char dispBuf[200];
    int  bufPos    = 0;
    int  curChar   = charIdx;
    int  curByte   = byteOff;
    int  renderedW = 0;
    int  needW     = 128 + subOff;

    while (renderedW < needW && bufPos < (int)sizeof(dispBuf) - 4) {
        if (curChar >= charCnt) {
            /* 循环: 2 空格间隔 */
            dispBuf[bufPos++] = ' '; dispBuf[bufPos++] = ' ';
            renderedW += 16;
            curChar = 0; curByte = 0;
            if (renderedW >= needW) break;
            continue;
        }
        uint8_t blen = charBL[curChar];
        if (bufPos + blen >= (int)sizeof(dispBuf) - 1) break;
        memcpy(dispBuf + bufPos, src + curByte, blen);
        bufPos    += blen;
        renderedW += charPW[curChar];
        curByte   += blen;
        curChar++;
    }
    dispBuf[bufPos] = '\0';
    GUI_DispStringAt(dispBuf, -subOff, yPos);
}

/* 温度缓存: defaultTask 异步更新, guiTask 只读 (避免阻塞 OneWire) */
static volatile float g_cachedTemp = 25.0f;

void DISP_UpdateTempCache(void)
{
    float t = ds18b20_read();
    if (t >= -55.0f && t <= 125.0f) {
        g_cachedTemp = t;
    }
}

float DISP_GetCachedTemp(void)
{
    return g_cachedTemp;
}

void DISP_ShowClock(void)
{
    BSP_RTC_DateTime_t dt;
    char buf[100];
    float temp;
    static uint32_t s_scrollTick = 0;

    BSP_RTC_GetDateTime(&dt);
    temp = g_cachedTemp;  /* 异步缓存, 不阻塞 */

    GUI_Clear();
    GUI_SetColor(GUI_COLOR_WHITE);

    /* ===== 第一行: 日期信息 (16px, 滚动) ===== */
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);

    {
        snprintf(buf, sizeof(buf),
                 "%04d\xe5\xb9\xb4%02d\xe6\x9c\x88%02d\xe6\x97\xa5 %s "
                 "\xe8\xae\xbe\xe5\xa4\x87:%03d "
                 "\xe6\xa8\xa1\xe5\xbc\x8f:%s",
                 dt.year, dt.month, dt.day, s_weekdays_cn[dt.weekday],
                 s_adminCfg.deviceId,
                 s_modeNames[s_adminCfg.attendMode]);

        uint8_t  dBL[64], dPW[64];
        uint8_t  dCnt;
        uint16_t dTW;
        computeCharWidths(buf, dBL, dPW, &dCnt, &dTW, 64);
        int datePixelW = (int)dTW;
        if (datePixelW <= 0) datePixelW = 128;

        uint32_t now = osKernelGetTickCount();
        if (s_scrollTick == 0) s_scrollTick = now;

        if (datePixelW <= 128) {
            GUI_DispStringHCenterAt(buf, 64, 0);
        } else {
            int cycleW = datePixelW + 16;
            int scrollOff = (int)(((now - s_scrollTick) / DISP_SCROLL_MS_PER_PX)
                                  % (uint32_t)cycleW);
            renderScrollLine(buf, (int)strlen(buf), dBL, dPW, dCnt,
                             datePixelW, scrollOff, 0);
        }
    }

    /* ===== 第二行: 时间 (32px 大字居中) ===== */
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_32);
    sprintf(buf, "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
    GUI_DispStringHCenterAt(buf, 64, 16);

    /* ===== 第三行: 天气滚动 + 本地温度 ===== */
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);

    if (s_weatherCache.ready && s_weatherCache.line[0] != '\0') {
        uint32_t now = osKernelGetTickCount();
        if (s_scrollTick == 0) s_scrollTick = now;

        int textPixelW = (int)s_weatherCache.totalPixelW;
        if (textPixelW <= 0) textPixelW = 128;

        /* 温度后缀: "本地温度：25.5℃" (5CJK×16+4ASCII×8+℃×16=128px) */
        char tempStr[48];
        snprintf(tempStr, sizeof(tempStr),
                 "\xe6\x9c\xac\xe5\x9c\xb0\xe6\xb8\xa9\xe5\xba\xa6\xef\xbc\x9a"
                 "%.1f\xe2\x84\x83", temp);
#define TEMP_PIXEL_W  128
#define SCROLL_GAP    16

        int cycleW = textPixelW + SCROLL_GAP + TEMP_PIXEL_W + SCROLL_GAP;
        if (cycleW <= 0) cycleW = 128;

        int scrollOff = (int)(((now - s_scrollTick) / DISP_SCROLL_MS_PER_PX)
                              % (uint32_t)cycleW);

        /* 总宽不超屏 → 静态居中 */
        if (textPixelW + SCROLL_GAP + TEMP_PIXEL_W <= 128) {
            char combined[192];
            snprintf(combined, sizeof(combined), "%s  %s",
                     s_weatherCache.line, tempStr);
            GUI_DispStringHCenterAt(combined, 64, 48);
        } else {
            /* 查找起始字符 & 子像素偏移 */
            int charIdx = 0, byteOff = 0, accumW = 0;
            while (charIdx < s_weatherCache.charCount &&
                   accumW + s_weatherCache.charPixelW[charIdx] <= scrollOff) {
                accumW  += s_weatherCache.charPixelW[charIdx];
                byteOff += s_weatherCache.charByteLen[charIdx];
                charIdx++;
            }
            int subOff = scrollOff - accumW;
            if (subOff < 0) subOff = 0;

            /* 拼接: 天气文本 → 间隔 → 温度 → 间隔 → 循环 */
            static char dispBuf[200];
            int  bufPos    = 0;
            int  curChar   = charIdx;
            int  curByte   = byteOff;
            int  renderedW = 0;
            int  needW     = 128 + subOff;
            int  phase     = 0;   /* 0=天气 1=间隔 2=温度 3=间隔 */
            int  tempSent  = 0;

            while (renderedW < needW && bufPos < (int)sizeof(dispBuf) - 8) {
                if (phase == 0) {
                    if (curChar >= s_weatherCache.charCount) {
                        phase = 1; continue;
                    }
                    uint8_t blen = s_weatherCache.charByteLen[curChar];
                    if (bufPos + blen >= (int)sizeof(dispBuf) - 1) break;
                    memcpy(dispBuf + bufPos,
                           s_weatherCache.line + curByte, blen);
                    bufPos    += blen;
                    renderedW += s_weatherCache.charPixelW[curChar];
                    curByte   += blen;
                    curChar++;
                } else if (phase == 1) {
                    dispBuf[bufPos++] = ' ';
                    dispBuf[bufPos++] = ' ';
                    renderedW += SCROLL_GAP;
                    phase = 2;
                } else if (phase == 2) {
                    if (!tempSent) {
                        int tlen = (int)strlen(tempStr);
                        if (bufPos + tlen < (int)sizeof(dispBuf) - 1) {
                            memcpy(dispBuf + bufPos, tempStr, tlen);
                            bufPos    += tlen;
                            renderedW += TEMP_PIXEL_W;
                        }
                        tempSent = 1;
                    }
                    phase = 3;
                } else {
                    dispBuf[bufPos++] = ' ';
                    dispBuf[bufPos++] = ' ';
                    renderedW += SCROLL_GAP;
                    curChar = 0; curByte = 0;
                    phase   = 0;
                }
            }
            dispBuf[bufPos] = '\0';
            GUI_DispStringAt(dispBuf, -subOff, 48);
        }
    } else {
        /* 天气未就绪: 仅显示温度 */
        snprintf(buf, sizeof(buf), "\xe6\x9c\xac\xe5\x9c\xb0\xe6\xb8\xa9\xe5\xba\xa6\xef\xbc\x9a%.1f\xe2\x84\x83", temp);
        GUI_DispStringHCenterAt(buf, 64, 48);
    }

    GUI_Update();
}

/* ====== 公开 API: RESULT 状态 ====== */

void DISP_ShowCardResult(const DispCardInfo_t *pInfo, int countdownSec)
{
    char buf[48];

    if (pInfo == NULL) return;

    GUI_Clear();
    GUI_SetColor(GUI_COLOR_WHITE);
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);

    /* ===== 重复刷卡 / 无效重复 ===== */
    if (pInfo->event == DISP_EVT_CARD_DUP || pInfo->recordStatus == 1) {
        GUI_DispStringHCenterAt(
            "\xe8\xaf\xb7\xe5\x8b\xbf\xe9\x87\x8d\xe5\xa4\x8d\xe5\x88\xb7\xe5\x8d\xa1", 64, 16);
        sprintf(buf, "%d \xe7\xa7\x92\xe5\x90\x8e\xe8\xbf\x94\xe5\x9b\x9e", countdownSec);
        GUI_DispStringHCenterAt(buf, 64, 40);
        GUI_Update();
        return;
    }

    /* ===== 无入场记录 ===== */
    if (pInfo->recordStatus == 2) {
        GUI_DispStringHCenterAt(
            "\xc3\x97 \xe6\x97\xa0\xe5\x85\xa5\xe5\x9c\xba\xe8\xae\xb0\xe5\xbd\x95", 64, 16);
        GUI_DispStringHCenterAt(
            "\xe8\xaf\xb7\xe5\x85\x88\xe7\xad\xbe\xe5\x88\xb0", 64, 40);
        GUI_Update();
        return;
    }

    /* ===== 无效卡 ===== */
    if (pInfo->event == DISP_EVT_CARD_INVALID) {
        GUI_DispStringHCenterAt(
            "\xc3\x97 \xe6\x97\xa0\xe6\x95\x88\xe5\x8d\xa1", 64, 0);
        if (pInfo->statusFlag == 0xFF) {
            GUI_DispStringAt("\xe5\xb7\xb2\xe6\x8c\x82\xe5\xa4\xb1", 52, 24);
        } else {
            GUI_DispStringAt("\xe6\xa0\xa1\xe9\xaa\x8c\xe5\xa4\xb1\xe8\xb4\xa5", 52, 24);
        }
        GUI_Update();
        return;
    }

    /* ===== 入口/出口模式: 简化 UI, 不显示卡片详情 ===== */
    {
        uint8_t mode = DISP_GetAdminCfg()->attendMode;

        /* ---- 入口模式: 签到 / 时刻 / 日期 ---- */
        if (mode == 0) {
            /* 行1: 签到 (16px) */
            GUI_DispStringHCenterAt(
                "\xe7\xad\xbe\xe5\x88\xb0", 64, 8);
            /* 行2: 时刻 (16px) */
            sprintf(buf, "%02d:%02d:%02d",
                    pInfo->swipeHour, pInfo->swipeMinute, pInfo->swipeSecond);
            GUI_DispStringHCenterAt(buf, 64, 24);
            /* 行3: 日期 (16px) */
            {
                BSP_RTC_DateTime_t dt;
                BSP_RTC_GetDateTime(&dt);
                sprintf(buf, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
            }
            GUI_DispStringHCenterAt(buf, 64, 44);
            GUI_Update();
            return;
        }

        /* ---- 出口模式: 离开 / 时长(>1h 分两行, <=1h 单行) ---- */
        if (mode == 1) {
            /* 行1: 离开 (16px) */
            GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);
            GUI_DispStringHCenterAt(
                "\xe7\xa6\xbb\xe5\xbc\x80", 64, 8);

            if (pInfo->durationSec > 0) {
                uint32_t d = pInfo->durationSec / 86400;
                uint32_t h = (pInfo->durationSec % 86400) / 3600;
                uint32_t m = (pInfo->durationSec % 3600) / 60;
                uint32_t s = pInfo->durationSec % 60;

                if (d > 0 || h > 0) {
                    /* > 1h: 行2=天+时, 行3=分+秒 */
                    if (d > 0)
                        sprintf(buf, "%lu\xe5\xa4\xa9%lu\xe6\x97\xb6", d, h);
                    else
                        sprintf(buf, "%lu\xe6\x97\xb6", h);
                    GUI_DispStringHCenterAt(buf, 64, 24);

                    sprintf(buf, "%lu\xe5\x88\x86%lu\xe7\xa7\x92", m, s);
                    GUI_DispStringHCenterAt(buf, 64, 40);
                } else {
                    /* <= 1h: 行2=正常时长(分+秒) */
                    if (m > 0)
                        sprintf(buf, "%lu\xe5\x88\x86%lu\xe7\xa7\x92", m, s);
                    else
                        sprintf(buf, "%lu\xe7\xa7\x92", s);
                    GUI_DispStringHCenterAt(buf, 64, 28);
                }
            } else {
                /* 无 duration: 显示离开时刻 */
                sprintf(buf, "%02d:%02d:%02d",
                        pInfo->swipeHour, pInfo->swipeMinute, pInfo->swipeSecond);
                GUI_DispStringHCenterAt(buf, 64, 28);
            }
            GUI_Update();
            return;
        }
    }

    /* ===== 图像卡: 头像(可选) + 位图 ===== */
    if (pInfo->cardType == 0x01) {
        int xOff = s_cardBitmap.hasAvatar ? 48 : 0;

        /* 头像 48×64 (有数据才绘制) */
        if (s_cardBitmap.hasAvatar) {
            GUI_BITMAP bmAv;
            bmAv.XSize = DISP_AVATAR_W;
            bmAv.YSize = DISP_AVATAR_H;
            bmAv.BytesPerLine = DISP_AVATAR_W / 8;
            bmAv.BitsPerPixel = 1;
            bmAv.pData = s_cardBitmap.avatar;
            bmAv.pPal = NULL;
            GUI_DrawBitmap(&bmAv, 0, 0);
        }

        /* 姓名位图 80×16 */
        {
            GUI_BITMAP bm;
            bm.XSize = DISP_NAME_W; bm.YSize = DISP_NAME_H;
            bm.BytesPerLine = DISP_NAME_W / 8; bm.BitsPerPixel = 1;
            bm.pData = s_cardBitmap.name; bm.pPal = NULL;
            GUI_DrawBitmap(&bm, xOff, 0);
        }
        /* 部门位图 80×16 */
        {
            GUI_BITMAP bm;
            bm.XSize = DISP_DEPT_W; bm.YSize = DISP_DEPT_H;
            bm.BytesPerLine = DISP_DEPT_W / 8; bm.BitsPerPixel = 1;
            bm.pData = s_cardBitmap.dept; bm.pPal = NULL;
            GUI_DrawBitmap(&bm, xOff, 16);
        }
        /* 签到/离开 + 时长 */
        if (pInfo->eventType == 0) {
            sprintf(buf, "\xe7\xad\xbe\xe5\x88\xb0 %02d:%02d:%02d",
                    pInfo->swipeHour, pInfo->swipeMinute, pInfo->swipeSecond);
        } else if (pInfo->durationSec > 0) {
            uint32_t d = pInfo->durationSec / 86400;
            uint32_t h = (pInfo->durationSec % 86400) / 3600;
            uint32_t m = (pInfo->durationSec % 3600) / 60;
            uint32_t s = pInfo->durationSec % 60;
            if (d > 0)
                sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %lud\xe5\xa4\xa9%luh\xe6\x97\xb6%lum\xe5\x88\x86%lus\xe7\xa7\x92", d, h, m, s);
            else if (h > 0)
                sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %luh\xe6\x97\xb6%lum\xe5\x88\x86%lus\xe7\xa7\x92", h, m, s);
            else if (m > 0)
                sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %lum\xe5\x88\x86%lus\xe7\xa7\x92", m, s);
            else
                sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %lus\xe7\xa7\x92", s);
        } else {
            sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %02d:%02d:%02d",
                    pInfo->swipeHour, pInfo->swipeMinute, pInfo->swipeSecond);
        }
        GUI_DispStringAt(buf, xOff, 48);

        /* 无头像时用工号填充 */
        if (!s_cardBitmap.hasAvatar) {
            sprintf(buf, "\xe5\xb7\xa5\xe5\x8f\xb7:%lu", (unsigned long)pInfo->sid);
            GUI_DispStringAt(buf, xOff, 32);
        }

        GUI_Update();
        return;
    }

    /* ===== 普通卡/管理员卡: 位图 80×16 居中 + 文本 ===== */
    /* 行1 y=0: 姓名位图 80×16 (居中: (128-80)/2=24) */
    {
        GUI_BITMAP bm;
        bm.XSize = DISP_NAME_W; bm.YSize = DISP_NAME_H;
        bm.BytesPerLine = DISP_NAME_W / 8; bm.BitsPerPixel = 1;
        bm.pData = s_cardBitmap.name; bm.pPal = NULL;
        GUI_DrawBitmap(&bm, 24, 0);
    }

    /* 行2 y=16: 部门位图 80×16 (居中) */
    {
        GUI_BITMAP bm;
        bm.XSize = DISP_DEPT_W; bm.YSize = DISP_DEPT_H;
        bm.BytesPerLine = DISP_DEPT_W / 8; bm.BitsPerPixel = 1;
        bm.pData = s_cardBitmap.dept; bm.pPal = NULL;
        GUI_DrawBitmap(&bm, 24, 16);
    }

    /* 行3 y=32: 工号 (居中) */
    sprintf(buf, "\xe5\xb7\xa5\xe5\x8f\xb7:%lu",
            (unsigned long)pInfo->sid);
    GUI_DispStringHCenterAt(buf, 64, 32);

    /* 行4 y=48: 签到/离开 + 时刻 (+ 时长) */
    if (pInfo->eventType == 0) {
        sprintf(buf, "\xe7\xad\xbe\xe5\x88\xb0 %02d:%02d:%02d",
                pInfo->swipeHour, pInfo->swipeMinute, pInfo->swipeSecond);
    } else if (pInfo->durationSec > 0) {
        uint32_t d = pInfo->durationSec / 86400;
        uint32_t h = (pInfo->durationSec % 86400) / 3600;
        uint32_t m = (pInfo->durationSec % 3600) / 60;
        uint32_t s = pInfo->durationSec % 60;
        if (d > 0)
            sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %lud\xe5\xa4\xa9%luh\xe6\x97\xb6%lum\xe5\x88\x86%lus\xe7\xa7\x92", d, h, m, s);
        else if (h > 0)
            sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %luh\xe6\x97\xb6%lum\xe5\x88\x86%lus\xe7\xa7\x92", h, m, s);
        else if (m > 0)
            sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %lum\xe5\x88\x86%lus\xe7\xa7\x92", m, s);
        else
            sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %lus\xe7\xa7\x92", s);
    } else {
        sprintf(buf, "\xe7\xa6\xbb\xe5\xbc\x80 %02d:%02d:%02d",
                pInfo->swipeHour, pInfo->swipeMinute, pInfo->swipeSecond);
    }
    GUI_DispStringHCenterAt(buf, 64, 48);

    GUI_Update();
}

/* ====== 公开 API: TIME_SET 状态 ====== */

/** 光标闪烁周期 (ms) */
#define CURSOR_BLINK_MS   400

void DISP_ShowTimeSet(void)
{
    BSP_RTC_DateTime_t *dt = &s_timeSet.dt;
    char buf[32];
    int blinkOn;
    int xPos[6] = {24, 64, 88, 32, 56, 80};  /* 居中后的字段 X 坐标 */
    int yLine1 = 16, yLine2 = 32;

    /* 光标闪烁 */
    uint32_t elapsed = osKernelGetTickCount() - s_timeSet.enterTick;
    blinkOn = ((elapsed / CURSOR_BLINK_MS) % 2) == 0;

    GUI_Clear();
    GUI_SetColor(GUI_COLOR_WHITE);
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);

    /* 标题 */
    GUI_DispStringHCenterAt(
        "\xe6\x97\xb6\xe9\x97\xb4\xe8\xae\xbe\xe7\xbd\xae", 64, 0);  /* 时间设置 */

    /* 第一行: 年-月-日 (居中) */
    sprintf(buf, "%04d-%02d-%02d", dt->year, dt->month, dt->day);
    GUI_DispStringHCenterAt(buf, 64, yLine1);

    /* 第二行: 时:分:秒 (居中) */
    sprintf(buf, "%02d:%02d:%02d", dt->hour, dt->minute, dt->second);
    GUI_DispStringHCenterAt(buf, 64, yLine2);

    /* 光标: 反色绘制当前选中字段 */
    if (blinkOn) {
        int cx = xPos[s_timeSet.cursor];
        int cy = (s_timeSet.cursor < 3) ? yLine1 : yLine2;
        int cw = (s_timeSet.cursor == 0) ? 32 : 16;  /* 年宽4位, 其他宽2位 */
        GUI_SetColor(GUI_COLOR_WHITE);
        GUI_FillRect(cx, cy - 1, cx + cw - 1, cy + 15);
        GUI_SetColor(GUI_COLOR_BLACK);

        switch (s_timeSet.cursor) {
        case 0: sprintf(buf, "%04d", dt->year);   break;
        case 1: sprintf(buf, "%02d", dt->month);  break;
        case 2: sprintf(buf, "%02d", dt->day);    break;
        case 3: sprintf(buf, "%02d", dt->hour);   break;
        case 4: sprintf(buf, "%02d", dt->minute); break;
        case 5: sprintf(buf, "%02d", dt->second); break;
        }
        GUI_DispStringAt(buf, cx, cy);

        GUI_SetColor(GUI_COLOR_WHITE);
    }

    /* 底部按键提示 (居中) */
    GUI_DispStringHCenterAt(
        "1+4- <> 5\xe8\xbf\x94" "6\xe5\xad\x98", 64, 48);  /* 1+4- <> 5返6存 */

    GUI_Update();
}

void DISP_TimeSetKey(uint8_t keyIdx, uint8_t isShort, uint8_t isLong)
{
    BSP_RTC_DateTime_t *dt = &s_timeSet.dt;

    if (!isShort && !isLong) return;

    /* ===== K2 光标左移 (上一个字段) ===== */
    if (keyIdx == 1 && isShort) {
        if (s_timeSet.cursor > 0) s_timeSet.cursor--;
        osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        return;
    }

    /* ===== K3 光标右移 (下一个字段) ===== */
    if (keyIdx == 2 && isShort) {
        if (s_timeSet.cursor < 5) s_timeSet.cursor++;
        osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        return;
    }

    /* ===== K6 保存并退出 ===== */
    if (keyIdx == 5 && isShort) {
        BSP_RTC_SetDateTime(dt);
        s_state = DISP_STATE_CLOCK;
        osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        return;
    }

    /* ===== K1 加 / K4 减 ===== */
    if (keyIdx != 0 && keyIdx != 3) return;

    /* year 是 uint16_t, 其余字段是 uint8_t */
    if (s_timeSet.cursor == 0) {
        /* 年份: uint16_t, 范围 2020~2099 */
        uint16_t *pYear = &dt->year;
        if (keyIdx == 0) {  /* K1 加 */
            if (*pYear >= 2099U) *pYear = 2020U;
            else (*pYear)++;
        } else {            /* K4 减 */
            if (*pYear <= 2020U) *pYear = 2099U;
            else (*pYear)--;
        }
    } else {
        /* 月/日/时/分/秒: uint8_t, min/max 各不同 */
        uint8_t *pU8 = NULL;
        uint8_t  minVal = 0;
        uint8_t  maxVal = 0;

        switch (s_timeSet.cursor) {
        case 1: /* 月: 1~12 */
            pU8 = &dt->month; minVal = 1;  maxVal = 12;
            break;
        case 2: /* 日: 1~当月最大天数 */
            pU8 = &dt->day;   minVal = 1;
            maxVal = BSP_RTC_DaysInMonth(dt->year, dt->month);
            break;
        case 3: /* 时: 0~23 */
            pU8 = &dt->hour;  minVal = 0;  maxVal = 23;
            break;
        case 4: /* 分: 0~59 */
            pU8 = &dt->minute; minVal = 0; maxVal = 59;
            break;
        case 5: /* 秒: 0~59 */
            pU8 = &dt->second; minVal = 0; maxVal = 59;
            break;
        default: return;
        }

        if (keyIdx == 0) {  /* K1 加 */
            if (*pU8 >= maxVal) *pU8 = minVal;
            else (*pU8)++;
        } else {            /* K4 减 */
            if (*pU8 <= minVal) *pU8 = maxVal;
            else (*pU8)--;
        }
    }

    /* 月份/年份变动后, 钳位 day 到新月份的有效范围 */
    {
        uint8_t daysMax = BSP_RTC_DaysInMonth(dt->year, dt->month);
        if (dt->day > daysMax) dt->day = daysMax;
    }

    osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
}

/* ====== 公开 API: ADMIN_SET 状态 ====== */

void DISP_ShowAdminSet(void)
{
    char buf[32];
    int blinkOn;

    uint32_t elapsed = osKernelGetTickCount() - s_adminSet.enterTick;
    blinkOn = ((elapsed / CURSOR_BLINK_MS) % 2) == 0;

    GUI_Clear();
    GUI_SetColor(GUI_COLOR_WHITE);
    GUI_SetFont((GUI_FONT *)&GUI_FontHZ_SimSun_16);

    /* 标题 */
    GUI_DispStringHCenterAt(
        "\xe7\xae\xa1\xe7\x90\x86\xe5\x91\x98\xe8\xae\xbe\xe7\xbd\xae", 64, 0);

    /* 1. 设备ID */
    sprintf(buf, "1.\xe8\xae\xbe\xe5\xa4\x87ID: %03d", s_adminCfg.deviceId);
    if (s_adminSet.cursor == 0 && blinkOn) {
        GUI_SetColor(GUI_COLOR_WHITE);
        GUI_FillRect(2, 22, 124, 39);
        GUI_SetColor(GUI_COLOR_BLACK);
        GUI_DispStringAt(buf, 4, 24);
    } else {
        GUI_SetColor(GUI_COLOR_WHITE);
        GUI_DispStringAt(buf, 4, 24);
    }

    /* 2. 考勤模式 */
    sprintf(buf, "2.\xe6\xa8\xa1\xe5\xbc\x8f: %s", s_modeNames[s_adminCfg.attendMode]);
    if (s_adminSet.cursor == 1 && blinkOn) {
        GUI_SetColor(GUI_COLOR_WHITE);
        GUI_FillRect(2, 44, 124, 61);
        GUI_SetColor(GUI_COLOR_BLACK);
        GUI_DispStringAt(buf, 4, 46);
        GUI_SetColor(GUI_COLOR_WHITE);  /* 恢复颜色, 防止下次渲染反色 */
    } else {
        GUI_SetColor(GUI_COLOR_WHITE);
        GUI_DispStringAt(buf, 4, 46);
    }

    GUI_Update();
}

void DISP_AdminSetKey(uint8_t keyIdx, uint8_t isShort, uint8_t isLong)
{
    if (!isShort && !isLong) return;

    switch (keyIdx) {
    case 0: /* K1 - 加 */
        if (isShort || isLong) {
            if (s_adminSet.cursor == 0) {
                if (s_adminCfg.deviceId < 999) s_adminCfg.deviceId++;
                else s_adminCfg.deviceId = 1;
            } else {
                if (s_adminCfg.attendMode < 2) s_adminCfg.attendMode++;
                else s_adminCfg.attendMode = 0;
            }
            osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        }
        break;
    case 1: /* K2 - 光标左移 */
        if (isShort) {
            if (s_adminSet.cursor > 0) s_adminSet.cursor--;
            osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        }
        break;
    case 2: /* K3 - 光标右移 */
        if (isShort && s_adminSet.cursor < 1) {
            s_adminSet.cursor++;
            osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        }
        break;
    case 3: /* K4 - 减 */
        if (isShort || isLong) {
            if (s_adminSet.cursor == 0) {
                if (s_adminCfg.deviceId > 1) s_adminCfg.deviceId--;
                else s_adminCfg.deviceId = 999;
            } else {
                if (s_adminCfg.attendMode > 0) s_adminCfg.attendMode--;
                else s_adminCfg.attendMode = 2;
            }
            osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        }
        break;
    case 4: /* K5 - 返回 (不保存, 恢复旧值) */
        if (isShort) {
            s_adminCfg = s_adminSet.backup;  /* 恢复副本 */
            s_state = DISP_STATE_CLOCK;
            osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        }
        break;
    case 5: /* K6 - 保存并退出 */
        if (isShort) {
            Config_Save();
            Cache_Clear();  /* 模式切换清缓存 */
            s_state = DISP_STATE_CLOCK;
            osThreadFlagsSet(guiTaskHandle, DISP_EVT_KEY_PRESSED);
        }
        break;
    default:
        break;
    }
}

/* ====== 公开 API: NFC 通信 ====== */

const DispCardInfo_t *DISP_GetCardInfo(void)
{
    return &s_cardInfo;
}

void DISP_NotifyCardEvent(uint32_t event, const uint8_t *pUid, uint8_t uidLen,
                          uint32_t cardId, uint32_t sid, uint32_t points,
                          uint8_t cardType, uint8_t statusFlag,
                          uint8_t hour, uint8_t minute, uint8_t second,
                          uint8_t eventType, uint8_t recStatus, uint32_t durationSec)
{
    s_cardInfo.event = event;
    if (pUid != NULL && uidLen > 0) {
        uint8_t len = (uidLen > 7) ? 7 : uidLen;
        memcpy(s_cardInfo.uid, pUid, len);
        s_cardInfo.uidLen = len;
    } else {
        s_cardInfo.uidLen = 0;
    }
    s_cardInfo.cardId    = cardId;
    s_cardInfo.sid       = sid;
    s_cardInfo.points    = points;
    s_cardInfo.cardType  = cardType;
    s_cardInfo.statusFlag = statusFlag;
    s_cardInfo.swipeHour = hour;
    s_cardInfo.swipeMinute = minute;
    s_cardInfo.swipeSecond = second;
    s_cardInfo.eventType    = eventType;
    s_cardInfo.recordStatus = recStatus;
    s_cardInfo.durationSec  = durationSec;

    if (guiTaskHandle != NULL) {
        osThreadFlagsSet(guiTaskHandle, event);
    }
}

/** NFC 队列发送 — nfcTask 调用, 消除共享内存竞态 */
void DISP_SendNfcEvent(osMessageQueueId_t queue,
                       uint32_t event, const uint8_t *pUid, uint8_t uidLen,
                       uint32_t cardId, uint32_t sid, uint32_t points,
                       uint8_t cardType, uint8_t statusFlag,
                       uint8_t hour, uint8_t minute, uint8_t second,
                       uint8_t eventType, uint8_t recStatus, uint32_t durationSec)
{
    NfcCardMsg_t msg;
    memset(&msg, 0, sizeof(msg));

    /* 拷贝卡片信息 */
    msg.info.event = event;
    if (pUid != NULL && uidLen > 0) {
        uint8_t len = (uidLen > 7) ? 7 : uidLen;
        memcpy(msg.info.uid, pUid, len);
        msg.info.uidLen = len;
    }
    msg.info.cardId    = cardId;
    msg.info.sid       = sid;
    msg.info.points    = points;
    msg.info.cardType  = cardType;
    msg.info.statusFlag = statusFlag;
    msg.info.swipeHour = hour;
    msg.info.swipeMinute = minute;
    msg.info.swipeSecond = second;
    msg.info.eventType    = eventType;
    msg.info.recordStatus = recStatus;
    msg.info.durationSec  = durationSec;

    /* 拷贝当前位图缓存 (nfcTask 已通过 NFC_ReadCard 填充) */
    msg.bitmap = s_cardBitmap;

    osMessageQueuePut(queue, &msg, 0, 0);
}

/** NFC 队列接收 — guiTask 每轮循环调用, 非阻塞 */
uint32_t DISP_CheckNfcEvent(osMessageQueueId_t queue)
{
    NfcCardMsg_t msg;

    if (osMessageQueueGet(queue, &msg, NULL, 0) != osOK) {
        return DISP_EVT_NONE;
    }

    /* 写入本地缓存 (仅 guiTask 访问, 无竞态) */
    s_cardInfo   = msg.info;
    s_cardBitmap = msg.bitmap;

    /* 复用原有标志位机制驱动状态机 */
    if (guiTaskHandle != NULL) {
        osThreadFlagsSet(guiTaskHandle, msg.info.event);
    }
    return msg.info.event;
}

DispCardBitmap_t *DISP_GetBitmapBuf(void)
{
    return &s_cardBitmap;
}

void DISP_SetWeatherCache(const char *line)
{
    if (line == NULL) {
        s_weatherCache.line[0] = '\0';
        s_weatherCache.ready = 0;
        s_weatherCache.charCount = 0;
        s_weatherCache.totalPixelW = 0;
        return;
    }
    strncpy(s_weatherCache.line, line, DISP_WEATHER_LINE_LEN - 1);
    s_weatherCache.line[DISP_WEATHER_LINE_LEN - 1] = '\0';

    /* 预计算每个 UTF-8 字符的字节长度和像素宽度
     * SimSun_16 字体: ASCII → 8px, 中文(CJK) → 16px */
    {
        const uint8_t *p = (const uint8_t *)s_weatherCache.line;
        uint16_t totalW = 0;
        uint8_t  count  = 0;

        while (*p && count < DISP_WEATHER_MAX_CHARS) {
            uint8_t blen = 1;
            uint8_t pw   = 8;   /* 默认 ASCII 半角宽度 */

            if (*p <= 0x7F) {
                blen = 1;
                pw   = 8;
            } else if ((*p & 0xE0) == 0xC0) {
                blen = 2;
                pw   = 8;       /* 拉丁扩展, 近似半角 */
            } else if ((*p & 0xF0) == 0xE0) {
                blen = 3;
                pw   = 16;      /* 中文/日韩 全角 */
            } else if ((*p & 0xF8) == 0xF0) {
                blen = 4;
                pw   = 16;      /* Emoji 等补充平面, 近似全角 */
            }

            s_weatherCache.charByteLen[count] = blen;
            s_weatherCache.charPixelW[count]  = pw;
            totalW += pw;
            count++;
            p += blen;
        }

        s_weatherCache.charCount  = count;
        s_weatherCache.totalPixelW = totalW;
    }

    /* 内存屏障: 确保以上所有写入在 ready=1 之前对 guiTask 可见 */
    __asm__ volatile("dmb" ::: "memory");
    s_weatherCache.ready = 1;
}

void DISP_SetTimeOffset(int32_t offsetSec)
{
    s_timeOffset = offsetSec;
}

int32_t DISP_GetTimeOffset(void)
{
    return s_timeOffset;
}

uint8_t DISP_IsRtcCalibrated(void)
{
    return (s_timeOffset != (int32_t)TIME_OFFSET_UNCALIBRATED) ? 1 : 0;
}

/* ====== 时间同步锁 ====== */

static uint8_t s_timeSyncBusy = 0;

void DISP_EnterTimeSync(void)
{
    s_timeSyncBusy = 1;
}

void DISP_ExitTimeSync(void)
{
    s_timeSyncBusy = 0;
}

uint8_t DISP_IsTimeSyncBusy(void)
{
    return s_timeSyncBusy;
}

/* (Record_FixAllTimeOffsets 已迁移至 record.c) */
