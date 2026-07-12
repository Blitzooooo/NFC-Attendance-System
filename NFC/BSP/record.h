/**
 * @file    record.h
 * @brief   打卡记录存储模块 - SPI Flash 配置/记录/缓存/CRC
 * @details 从 display.h 拆分, 独立管理 Flash 持久化数据:
 *          - 系统配置 (双备份)
 *          - 刷卡记录 (循环写入)
 *          - LRU 卡片缓存
 *          - CRC16 校验
 */

#ifndef __RECORD_H__
#define __RECORD_H__

#include "main.h"
#include <stdint.h>

/* ====== 系统配置 (SPI Flash 持久化) ====== */

#define CONFIG_FLASH_ADDR     0x00000000U
#define CONFIG_BAK_ADDR       0x00001000U
#define CONFIG_MAGIC          0x4E46U   /* "NF" */

typedef struct {
    uint16_t deviceId;
    uint8_t  attendMode;
    uint16_t timeOffset;
    uint8_t  reserved[7];
    uint16_t magic;
    uint16_t crc16;
} SysConfig_t;

/* ====== 记录区头部 (SPI Flash 扇区2) ====== */

#define RECHEAD_FLASH_ADDR    0x00002000U
#define RECHEAD_MAGIC         0x4E46436EU  /* "NFCn" */

typedef struct {
    uint32_t magic;
    uint32_t writeOffset;
    uint32_t totalCount;
    uint32_t uploadOffset;
    uint16_t checksum;
    uint16_t version;
    uint8_t  reserved[12];
} RecordHeader_t;

/* ====== 刷卡记录 (32B, 共 128 条 = 1 扇区 4KB) ====== */

#define RECORD_FLASH_ADDR     0x00003000U
#define RECORD_SIZE           32
#define REC_PER_SECTOR        128
#define RECORD_AREA_SIZE      (REC_PER_SECTOR * RECORD_SIZE)  /* 4096 */
#define RECORD_MAX_COUNT      REC_PER_SECTOR                  /* 128 */

typedef struct {
    uint32_t seqNum;
    uint16_t deviceId;
    uint32_t uid;
    uint32_t sid;
    uint8_t  year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  eventType;
    uint8_t  status;
    uint32_t durationSec;
    uint16_t timeOffset;
    uint16_t crc16;
    uint8_t  cardType;
    uint8_t  reserved;
} SwipeRecord_t;

/* ====== LRU 卡片缓存 ====== */

#define CARD_CACHE_SIZE  32
#define CACHE_MISS       0xFF

typedef struct {
    uint32_t cardId;
    uint8_t  eventType;
    uint32_t enterSecs;
    uint32_t accessTick;
} CardCache_t;

/* ====== 黑名单 (SPI Flash 扇区4) ====== */

#define BLACKLIST_FLASH_ADDR  0x00004000U
#define BLACKLIST_MAGIC       0x424BU   /* "BK" */
#define MAX_BLACKLIST         50

typedef struct {
    uint16_t magic;
    uint16_t count;
    uint32_t uidList[MAX_BLACKLIST];
    uint16_t crc16;
    uint8_t  reserved[2];
} BlacklistData_t;

/* ====== 函数声明 ====== */

/* CRC16 */
uint16_t CRC16_Calc(const uint8_t *pData, uint16_t len);

/* 系统配置 */
void Config_Init(void);
void Config_Load(void);
void Config_Save(void);

/* 刷卡记录 */
void Record_Init(void);
void Record_Add(const SwipeRecord_t *pRec);
int  Record_QueryLast(uint32_t cardId, SwipeRecord_t *pOut);
void Record_List(uint32_t maxCount);
void Record_FixAllTimeOffsets(int32_t deltaSecs);
void     Record_MarkAllUploaded(void);      /**< 标记所有记录已上传 */
uint32_t Record_GetUnuploadCount(void);     /**< 获取未上传记录数 */

/* LRU 缓存 */
uint8_t Cache_Find(uint32_t cardId, uint32_t *pEnterSecs);
void    Cache_Update(uint32_t cardId, uint8_t eventType, uint32_t enterSecs);
void    Cache_Clear(void);

/* 运行时获取管理员配置 */
void Config_SetAdmin(uint16_t deviceId, uint8_t attendMode);
uint16_t Config_GetDeviceId(void);
uint8_t  Config_GetAttendMode(void);

/* 黑名单管理 */
void    Blacklist_Init(void);
uint8_t Blacklist_Add(uint32_t uid);
uint8_t Blacklist_Remove(uint32_t uid);
uint8_t Blacklist_IsBlocked(uint32_t uid);

void    Blacklist_Save(void);
uint16_t Blacklist_GetCount(void);

#endif /* __RECORD_H__ */
