/**
 * @file    display.h
 * @brief   OLED 显示模块 - 封装所有显示场景, 统一在 guiTask 上下文中调用
 * @details 状态机驱动:
 *          - CLOCK: 时钟待机 (默认), 刷卡→RESULT, K5长按→TIME_SET
 *          - RESULT: 刷卡结果, 3s自动返回 / 按键→CLOCK, 管理卡→ADMIN_SET
 *          - TIME_SET: 时间设置, K6保存→CLOCK
 *          - ADMIN_SET: 管理员设置, 120s超时 / K6保存→CLOCK
 *
 *          与 nfcTask 通信机制:
 *          nfcTask 通过 osThreadFlagsSet(guiTaskHandle, DISP_EVT_xxx) 发送事件,
 *          guiTask 通过 osThreadFlagsWait() 接收后调用本模块渲染.
 */

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os.h"
#include "bsp_rtc.h"
#include "record.h"
#include <stdint.h>

/* ================================================================
 *  显示性能调参区 (修改后重新编译即可生效, 无需改多处代码)
 * ================================================================ */

/** GUI 刷新间隔 (ms) — 越小帧率越高, 受 I2C 带宽限制
 *  @note  I2C@400kHz 传输 1048B/帧 ≈ 32ms, 加上中断开销建议 ≥ 55ms
 *         设置过小会导致 I2C 被抢占→帧不完整→花屏 */
#define DISP_FRAME_INTERVAL_MS   55

/** 滚动速度: 每像素耗时 (ms/px), 越小滚动越快
 *  @note  62=~16px/s (默认), 31=~32px/s, 15=~66px/s */
#define DISP_SCROLL_MS_PER_PX    62

/** I2C 目标时钟频率 (Hz) — 需与 CubeMX ioc 中的 I2C1 ClockSpeed 保持一致
 *  @note  STM32F4 I2C Fast Mode 最大 400kHz, 无需修改 */
#define DISP_I2C_TARGET_HZ       400000

/* ====== 显示状态枚举 ====== */

typedef enum {
    DISP_STATE_CLOCK     = 0,
    DISP_STATE_RESULT    = 1,
    DISP_STATE_TIME_SET  = 2,
    DISP_STATE_ADMIN_SET = 3,
} DispState_t;

/* ====== 显示事件标志 ====== */

#define DISP_EVT_NONE          0x00000000U
#define DISP_EVT_CARD_VALID    (1UL << 0)
#define DISP_EVT_CARD_INVALID  (1UL << 1)
#define DISP_EVT_CARD_DUP      (1UL << 2)
#define DISP_EVT_CARD_ADMIN    (1UL << 3)
#define DISP_EVT_ALL_NFC       (DISP_EVT_CARD_VALID | DISP_EVT_CARD_INVALID | \
                                DISP_EVT_CARD_DUP   | DISP_EVT_CARD_ADMIN)
#define DISP_EVT_KEY_PRESSED   (1UL << 16)

/* ====== 刷卡信息结构 ====== */

typedef struct {
    uint8_t  uid[7];
    uint8_t  uidLen;
    uint32_t event;
    uint32_t cardId;
    uint32_t sid;
    uint32_t points;
    uint8_t  cardType;
    uint8_t  statusFlag;
    uint8_t  swipeHour;
    uint8_t  swipeMinute;
    uint8_t  swipeSecond;
    uint8_t  eventType;
    uint8_t  recordStatus;
    uint32_t durationSec;
} DispCardInfo_t;

/* ====== 卡片位图缓存 ====== */

#define DISP_AVATAR_W   48
#define DISP_AVATAR_H   64
#define DISP_AVATAR_BYTES  ((DISP_AVATAR_W * DISP_AVATAR_H) / 8)
#define DISP_NAME_W     80
#define DISP_NAME_H     16
#define DISP_NAME_BYTES ((DISP_NAME_W * DISP_NAME_H) / 8)
#define DISP_DEPT_W     80
#define DISP_DEPT_H     16
#define DISP_DEPT_BYTES ((DISP_DEPT_W * DISP_DEPT_H) / 8)

typedef struct {
    uint8_t  hasAvatar;
    uint8_t  avatar[DISP_AVATAR_BYTES];
    uint8_t  name[DISP_NAME_BYTES];
    uint8_t  dept[DISP_DEPT_BYTES];
} DispCardBitmap_t;

/* ====== 管理员配置 (运行时) ====== */

typedef struct {
    uint16_t deviceId;
    uint8_t  attendMode;
} DispAdminCfg_t;

/* ====== RTC 时间戳工具 ====== */
uint32_t DISP_DateTimeToSeconds(const BSP_RTC_DateTime_t *dt);

/* 获取运行时配置 */
const DispAdminCfg_t *DISP_GetAdminCfg(void);

/* ====== 卡片位图缓存 ====== */

/* ====== 天气缓存 ====== */

/** 天气滚动信息缓冲区 (defaultTask 写入, guiTask 读取) */
#define DISP_WEATHER_LINE_LEN   128
#define DISP_WEATHER_MAX_CHARS  64          /**< 单行最多字符数 */

typedef struct {
    char     line[DISP_WEATHER_LINE_LEN];   /**< 滚动文本行 (UTF-8), \0 结尾 */
    uint8_t  charByteLen[DISP_WEATHER_MAX_CHARS]; /**< 每个字符的 UTF-8 字节长度 */
    uint8_t  charPixelW[DISP_WEATHER_MAX_CHARS];  /**< 每个字符的渲染像素宽度 */
    uint8_t  charCount;                     /**< 字符总数 */
    uint16_t totalPixelW;                   /**< 整行总像素宽度 */
    uint8_t  ready;                         /**< 1=数据已就绪 (volatile: 跨任务读写) */
} DispWeatherCache_t;

/* ====== NFC 刷卡消息 (nfcTask → myQueue04 → guiTask) ====== */

typedef struct {
    DispCardInfo_t   info;
    DispCardBitmap_t bitmap;
} NfcCardMsg_t;

/* ====== API ====== */

void DISP_Init(void);
void DISP_RunStartup(void);
void DISP_ApplyI2CConfig(void);      /**< 应用 I2C 时钟配置, 覆盖 CubeMX 默认值 */

/* OLED 防卡顿看门狗 */
void    DISP_FeedWatchdog(void);    /**< 喂狗: 每帧渲染后调用 */
uint8_t DISP_CheckStuck(void);      /**< 检测 OLED 是否卡顿 (>3s 无刷新) */
void    DISP_ResetOLED(void);       /**< 强制复位 OLED 硬件 + 清屏 */
void DISP_ShowInitMsg(const char *msg);
void DISP_ShowLogo(void);
void DISP_ShowInfoPage(void);

/* 状态管理 */
DispState_t DISP_GetState(void);
void        DISP_SetState(DispState_t state);

/* 各状态渲染 */
void DISP_ShowClock(void);
void DISP_UpdateTempCache(void);    /**< defaultTask 调用, 异步更新温度缓存 */
float DISP_GetCachedTemp(void);     /**< 获取缓存的温度值 */
void DISP_ShowCardResult(const DispCardInfo_t *pInfo, int countdownSec);
void DISP_ShowTimeSet(void);
void DISP_ShowAdminSet(void);

/* 时间设置状态下处理按键 */
void DISP_TimeSetKey(uint8_t keyIdx, uint8_t isShort, uint8_t isLong);

/* 管理员设置状态下处理按键 */
void DISP_AdminSetKey(uint8_t keyIdx, uint8_t isShort, uint8_t isLong);

/* NFC 通信 */
const DispCardInfo_t *DISP_GetCardInfo(void);
void DISP_NotifyCardEvent(uint32_t event, const uint8_t *pUid, uint8_t uidLen,
                          uint32_t cardId, uint32_t sid, uint32_t points,
                          uint8_t cardType, uint8_t statusFlag,
                          uint8_t hour, uint8_t minute, uint8_t second,
                          uint8_t eventType, uint8_t recStatus, uint32_t durationSec);

/* NFC 队列通信 (替代直接调 DISP_NotifyCardEvent, 消除竞态) */
void     DISP_SendNfcEvent(osMessageQueueId_t queue,
                           uint32_t event, const uint8_t *pUid, uint8_t uidLen,
                           uint32_t cardId, uint32_t sid, uint32_t points,
                           uint8_t cardType, uint8_t statusFlag,
                           uint8_t hour, uint8_t minute, uint8_t second,
                           uint8_t eventType, uint8_t recStatus, uint32_t durationSec);
uint32_t DISP_CheckNfcEvent(osMessageQueueId_t queue);

/* 卡片位图存储 (nfcTask 调用来写入从卡读取的图像数据) */
DispCardBitmap_t *DISP_GetBitmapBuf(void);

/* 天气缓存写入 (defaultTask 写入, guiTask 在 DISP_ShowClock 中读取) */
void DISP_SetWeatherCache(const char *line);

/* ====== NTP 时间偏差管理 ====== */

/** 未校准标记: timeOffset == 0xFFFF 表示 RTC 尚未经 NTP 校准 */
#define TIME_OFFSET_UNCALIBRATED  0xFFFF

/** 设置 NTP 校准偏差 (networkTask 在 NTP 成功后调用) */
void DISP_SetTimeOffset(int32_t offsetSec);

/** 获取当前时间偏差, TIME_OFFSET_UNCALIBRATED 表示尚未校准 */
int32_t DISP_GetTimeOffset(void);

/** RTC 是否已经过 NTP 校准 (1=是 0=否) */
uint8_t DISP_IsRtcCalibrated(void);

/* ====== 时间同步锁 (校准回写期间禁止刷卡/发卡) ====== */

/** 进入时间同步状态, 禁止 NFC/发卡操作 */
void DISP_EnterTimeSync(void);

/** 退出时间同步状态 */
void DISP_ExitTimeSync(void);

/** 是否正在时间同步 (nfcTask/uartTask 在操作前检查) */
uint8_t DISP_IsTimeSyncBusy(void);

/* ====== 记录区批量修正 ====== */

/**
 * @brief  遍历 Flash 中所有打卡记录, 将 OFS=0xFFFF 的修正为 deltaSecs
 * @param  deltaSecs  NTP 修正量 (秒), 正值=RTC慢了
 * @note   内部会擦除整个记录扇区, 耗时约 500ms
 */
void Record_FixAllTimeOffsets(int32_t deltaSecs);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_H__ */
