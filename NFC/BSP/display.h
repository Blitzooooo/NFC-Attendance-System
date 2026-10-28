/**
 * @file    display.h
 * @brief   OLED ��ʾģ�� - ��װ������ʾ����, ͳһ�� guiTask �������е���
 * @details ״̬������:
 *          - CLOCK: ʱ�Ӵ��� (Ĭ��), ˢ����RESULT, K5������TIME_SET
 *          - RESULT: ˢ�����, 3s�Զ����� / ������CLOCK, ��������ADMIN_SET
 *          - TIME_SET: ʱ������, K6�����CLOCK
 *          - ADMIN_SET: ����Ա����, 120s��ʱ / K6�����CLOCK
 *
 *          �� nfcTask ͨ�Ż���:
 *          nfcTask ͨ�� osThreadFlagsSet(guiTaskHandle, DISP_EVT_xxx) �����¼�,
 *          guiTask ͨ�� osThreadFlagsWait() ���պ���ñ�ģ����Ⱦ.
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
 *  ��ʾ���ܵ����� (�޸ĺ����±��뼴����Ч, ����Ķദ����)
 * ================================================================ */

/** GUI ˢ�¼�� (ms) �� ԽС֡��Խ��, �� I2C ��������
 *  @note  I2C@400kHz ���� 1048B/֡ �� 32ms, �����жϿ������� �� 55ms
 *         ���ù�С�ᵼ�� I2C ����ռ��֡������������ */
#define DISP_FRAME_INTERVAL_MS   55

/** �����ٶ�: ÿ���غ�ʱ (ms/px), ԽС����Խ��
 *  @note  62=~16px/s (Ĭ��), 31=~32px/s, 15=~66px/s */
#define DISP_SCROLL_MS_PER_PX    62

/** I2C Ŀ��ʱ��Ƶ�� (Hz) �� ���� CubeMX ioc �е� I2C1 ClockSpeed ����һ��
 *  @note  STM32F4 I2C Fast Mode ��� 400kHz, �����޸� */
#define DISP_I2C_TARGET_HZ       400000

/* ====== ��ʾ״̬ö�� ====== */

typedef enum {
    DISP_STATE_CLOCK     = 0,
    DISP_STATE_RESULT    = 1,
    DISP_STATE_TIME_SET  = 2,
    DISP_STATE_ADMIN_SET = 3,
} DispState_t;

/* ====== ��ʾ�¼���־ ====== */

#define DISP_EVT_NONE          0x00000000U
#define DISP_EVT_CARD_VALID    (1UL << 0)
#define DISP_EVT_CARD_INVALID  (1UL << 1)
#define DISP_EVT_CARD_DUP      (1UL << 2)
#define DISP_EVT_CARD_ADMIN    (1UL << 3)
#define DISP_EVT_ALL_NFC       (DISP_EVT_CARD_VALID | DISP_EVT_CARD_INVALID | \
                                DISP_EVT_CARD_DUP   | DISP_EVT_CARD_ADMIN)
#define DISP_EVT_KEY_PRESSED   (1UL << 16)

/* ====== ˢ����Ϣ�ṹ ====== */

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

/* ====== ��Ƭλͼ���� ====== */

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

/* ====== ����Ա���� (����ʱ) ====== */

typedef struct {
    uint16_t deviceId;
    uint8_t  attendMode;
} DispAdminCfg_t;

/* ====== RTC ʱ������� ====== */
uint32_t DISP_DateTimeToSeconds(const BSP_RTC_DateTime_t *dt);

/* ��ȡ����ʱ���� */
const DispAdminCfg_t *DISP_GetAdminCfg(void);

/* ====== ��Ƭλͼ���� ====== */

/* ====== �������� ====== */

/** ����������Ϣ������ (defaultTask д��, guiTask ��ȡ) */
#define DISP_WEATHER_LINE_LEN   128
#define DISP_WEATHER_MAX_CHARS  64          /**< ��������ַ��� */

typedef struct {
    char     line[DISP_WEATHER_LINE_LEN];   /**< �����ı��� (UTF-8), \0 ��β */
    uint8_t  charByteLen[DISP_WEATHER_MAX_CHARS]; /**< ÿ���ַ��� UTF-8 �ֽڳ��� */
    uint8_t  charPixelW[DISP_WEATHER_MAX_CHARS];  /**< ÿ���ַ�����Ⱦ���ؿ��� */
    uint8_t  charCount;                     /**< �ַ����� */
    uint16_t totalPixelW;                   /**< ���������ؿ��� */
    uint8_t  ready;                         /**< 1=�����Ѿ��� (volatile: �������д) */
} DispWeatherCache_t;

/* ====== NFC ˢ����Ϣ (nfcTask �� myQueue04 �� guiTask) ====== */

typedef struct {
    DispCardInfo_t   info;
    DispCardBitmap_t bitmap;
} NfcCardMsg_t;

/* ====== API ====== */

void DISP_Init(void);
void DISP_RunStartup(void);
void DISP_ApplyI2CConfig(void);      /**< Ӧ�� I2C ʱ������, ���� CubeMX Ĭ��ֵ */

/* OLED �����ٿ��Ź� */
void    DISP_FeedWatchdog(void);    /**< ι��: ÿ֡��Ⱦ����� */
uint8_t DISP_CheckStuck(void);      /**< ��� OLED �Ƿ񿨶� (>3s ��ˢ��) */
void    DISP_ResetOLED(void);       /**< ǿ�Ƹ�λ OLED Ӳ�� + ���� */
void DISP_ShowInitMsg(const char *msg);
void DISP_ShowLogo(void);
void DISP_ShowInfoPage(void);

/* ״̬���� */
DispState_t DISP_GetState(void);
void        DISP_SetState(DispState_t state);

/* ��״̬��Ⱦ */
void DISP_ShowClock(void);
void DISP_UpdateTempCache(void);    /**< defaultTask ����, �첽�����¶Ȼ��� */
float DISP_GetCachedTemp(void);     /**< ��ȡ������¶�ֵ */
void DISP_ShowCardResult(const DispCardInfo_t *pInfo, int countdownSec);
void DISP_ShowTimeSet(void);
void DISP_ShowAdminSet(void);

/* ʱ������״̬�´������� */
void DISP_TimeSetKey(uint8_t keyIdx, uint8_t isShort, uint8_t isLong);

/* ����Ա����״̬�´������� */
void DISP_AdminSetKey(uint8_t keyIdx, uint8_t isShort, uint8_t isLong);

/* NFC ͨ�� */
const DispCardInfo_t *DISP_GetCardInfo(void);

/* NFC queue: nfcTask -> guiTask via DISP_SendNfcEvent/DISP_CheckNfcEvent */
void     DISP_SendNfcEvent(osMessageQueueId_t queue,
                           uint32_t event, const uint8_t *pUid, uint8_t uidLen,
                           uint32_t cardId, uint32_t sid, uint32_t points,
                           uint8_t cardType, uint8_t statusFlag,
                           uint8_t hour, uint8_t minute, uint8_t second,
                           uint8_t eventType, uint8_t recStatus, uint32_t durationSec);
uint32_t DISP_CheckNfcEvent(osMessageQueueId_t queue);

/* ��Ƭλͼ�洢 (nfcTask ������д��ӿ���ȡ��ͼ������) */
DispCardBitmap_t *DISP_GetBitmapBuf(void);

/* ��������д�� (defaultTask д��, guiTask �� DISP_ShowClock �ж�ȡ) */
void DISP_SetWeatherCache(const char *line);

/* ====== NTP ʱ��ƫ����� ====== */

/** δУ׼���: timeOffset == 0xFFFF ��ʾ RTC ��δ�� NTP У׼ */
#define TIME_OFFSET_UNCALIBRATED  0xFFFF

/** ���� NTP У׼ƫ�� (networkTask �� NTP �ɹ������) */
void DISP_SetTimeOffset(int32_t offsetSec);

/** ��ȡ��ǰʱ��ƫ��, TIME_OFFSET_UNCALIBRATED ��ʾ��δУ׼ */
int32_t DISP_GetTimeOffset(void);

/** RTC �Ƿ��Ѿ��� NTP У׼ (1=�� 0=��) */
uint8_t DISP_IsRtcCalibrated(void);

/* ====== ʱ��ͬ���� (У׼��д�ڼ��ֹˢ��/����) ====== */

/** ����ʱ��ͬ��״̬, ��ֹ NFC/�������� */
void DISP_EnterTimeSync(void);

/** �˳�ʱ��ͬ��״̬ */
void DISP_ExitTimeSync(void);

/** �Ƿ�����ʱ��ͬ�� (nfcTask/uartTask �ڲ���ǰ���) */
uint8_t DISP_IsTimeSyncBusy(void);

/* ====== ��¼���������� ====== */

/**
 * @brief  ���� Flash �����д򿨼�¼, �� OFS=0xFFFF ������Ϊ deltaSecs
 * @param  deltaSecs  NTP ������ (��), ��ֵ=RTC����
 * @note   �ڲ������������¼����, ��ʱԼ 500ms
 */
void Record_FixAllTimeOffsets(int32_t deltaSecs);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_H__ */
