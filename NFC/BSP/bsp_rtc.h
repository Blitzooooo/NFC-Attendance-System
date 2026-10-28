/**
 * @file    bsp_rtc.h
 * @brief   RTC 鏃ュ巻椹卞姩 - 灏佽?? STM32 HAL RTC, 鎻愪緵鏄撶敤鐨勬棩鏈熸椂闂磋?诲啓鎺ュ彛
 * @note    渚濊禆 CubeMX 鐢熸垚鐨? MX_RTC_Init() 宸插畬鎴? RTC 纭?浠跺垵濮嬪寲 (hrtc)
 *          鏈?椹卞姩閫氳繃澶囦唤瀵勫瓨鍣ㄥ垽鏂?鏄?鍚﹂?栨?′笂鐢?, 閬垮厤姣忔?″?嶄綅閲嶇疆鏃堕棿
 *          鏂?鐢典繚鎸侀渶瑕? VBAT 寮曡剼鎺ョ數姹犳垨骞舵帴 VDD
 */
#ifndef __BSP_RTC_H__
#define __BSP_RTC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* -----------------------------------------------------------
 *  鏃ユ湡鏃堕棿缁撴瀯浣?
 * --------------------------------------------------------- */
typedef struct {
    uint16_t year;    /* 骞?, 瀹屾暣 4 浣?, 濡? 2026            */
    uint8_t  month;   /* 鏈?, 1-12                          */
    uint8_t  day;     /* 鏃?, 1-31                          */
    uint8_t  weekday; /* 鏄熸湡, 0=鍛ㄦ棩 1=鍛ㄤ竴 ... 6=鍛ㄥ叚     */
    uint8_t  hour;    /* 鏃?, 0-23                          */
    uint8_t  minute;  /* 鍒?, 0-59                          */
    uint8_t  second;  /* 绉?, 0-59                          */
} BSP_RTC_DateTime_t;

/* 澶囦唤瀵勫瓨鍣ㄩ厤缃? 鈥斺€? 鐢ㄤ簬鍒ゆ柇鏄?鍚﹂?栨?′笂鐢? */
#define BSP_RTC_BKUP_DR     RTC_BKP_DR0
#define BSP_RTC_BKUP_MAGIC  0x5051U

/* -----------------------------------------------------------
 *  鍏ㄥ眬鏃ユ湡鏃堕棿鍙橀噺
 *  姣忔?? BSP_RTC_GetDateTime() 璇诲彇鍚庤嚜鍔ㄦ洿鏂?, 鏂逛究鍏朵粬浠诲姟鐩存帴璁块棶
 * --------------------------------------------------------- */
extern BSP_RTC_DateTime_t g_rtc_datetime;

/* -----------------------------------------------------------
 *  鍏?鍏辨帴鍙?
 * --------------------------------------------------------- */

/**
 * @brief  鍒ゆ柇鏄?鍚﹂?栨?′笂鐢? (澶囦唤瀵勫瓨鍣ㄤ腑鏃犻瓟鏁?)
 * @retval 1: 棣栨?′笂鐢?   0: 闈為?栨?? (RTC 宸茶??鍒濆?嬪寲杩?)
 */
uint8_t BSP_RTC_IsFirstPowerOn(void);

/**
 * @brief  鏍囪?? RTC 宸插垵濮嬪寲 (鍚戝?囦唤瀵勫瓨鍣ㄥ啓鍏ラ瓟鏁?)
 */
void BSP_RTC_MarkInitialized(void);

/**
 * @brief  璇诲彇 RTC 鏃ユ湡鏃堕棿
 * @param  dt: 杈撳嚭, 鎸囧悜鏃ユ湡鏃堕棿缁撴瀯浣?; 浼? NULL 鏃朵粎鏇存柊鍏ㄥ眬鍙橀噺
 * @retval HAL_OK 鎴愬姛, 鍏朵粬澶辫触
 * @note   璇诲彇缁撴灉浼氬悓鏃跺瓨鍏ュ叏灞€鍙橀噺 g_rtc_datetime, 鏂逛究鍏朵粬浠诲姟璁块棶
 */
HAL_StatusTypeDef BSP_RTC_GetDateTime(BSP_RTC_DateTime_t *dt);

/**
 * @brief  璁剧疆 RTC 鏃ユ湡
 * @param  year:  骞?, 濡? 2026
 * @param  month: 鏈? 1-12
 * @param  day:   鏃? 1-31
 * @retval HAL_OK 鎴愬姛, 鍏朵粬澶辫触
 */
HAL_StatusTypeDef BSP_RTC_SetDate(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief  璁剧疆 RTC 鏃堕棿
 * @param  hour:   鏃? 0-23
 * @param  minute: 鍒? 0-59
 * @param  second: 绉? 0-59
 * @retval HAL_OK 鎴愬姛, 鍏朵粬澶辫触
 */
HAL_StatusTypeDef BSP_RTC_SetTime(uint8_t hour, uint8_t minute, uint8_t second);

/**
 * @brief  鍚屾椂璁剧疆 RTC 鏃ユ湡鍜屾椂闂?
 * @param  dt: 鎸囧悜鏃ユ湡鏃堕棿缁撴瀯浣?
 * @retval HAL_OK 鎴愬姛, 鍏朵粬澶辫触
 */
HAL_StatusTypeDef BSP_RTC_SetDateTime(const BSP_RTC_DateTime_t *dt);

/**
 * @brief  鏍规嵁鏃ユ湡璁＄畻鏄熸湡
 * @param  year:  骞?
 * @param  month: 鏈?
 * @param  day:   鏃?
 * @retval 0=鍛ㄦ棩, 1=鍛ㄤ竴, ... 6=鍛ㄥ叚
 */
uint8_t BSP_RTC_CalcWeekday(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief  鍒ゆ柇闂板勾
 * @param  year: 骞?
 * @retval 1: 闂板勾  0: 骞冲勾
 */
uint8_t BSP_RTC_IsLeapYear(uint16_t year);

/**
 * @brief  鑾峰彇鏌愭湀鏈€澶уぉ鏁?
 * @param  year:  骞?
 * @param  month: 鏈?
 * @retval 璇ユ湀澶╂暟 (28-31)
 */
uint8_t BSP_RTC_DaysInMonth(uint16_t year, uint8_t month);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_RTC_H__ */
