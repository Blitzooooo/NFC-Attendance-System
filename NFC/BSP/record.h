/**
 * @file    record.h
 * @brief   �򿨼�¼�洢ģ�� - SPI Flash ����/��¼/����/CRC
 * @details �� display.h ���, �������� Flash �־û�����:
 *          - ϵͳ���� (˫����)
 *          - ˢ����¼ (ѭ��д��)
 *          - LRU ��Ƭ����
 *          - CRC16 У��
 */

#ifndef __RECORD_H__
#define __RECORD_H__

#include "main.h"
#include <stdint.h>

/* ====== ϵͳ���� (SPI Flash �־û�) ====== */

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

/* ====== ��¼��ͷ�� (SPI Flash ����2) ====== */

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

/* ====== ˢ����¼ (32B, �� 128 �� = 1 ���� 4KB) ====== */

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

/* ====== LRU ��Ƭ���� ====== */

#define CARD_CACHE_SIZE  32
#define CACHE_MISS       0xFF

typedef struct {
    uint32_t cardId;
    uint8_t  eventType;
    uint32_t enterSecs;
    uint32_t accessTick;
} CardCache_t;

/* ====== ������ (SPI Flash ����4) ====== */

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

/* ====== �������� ====== */

/* CRC16 */
uint16_t CRC16_Calc(const uint8_t *pData, uint16_t len);

/* ϵͳ���� */
void Config_Load(void);
void Config_Save(void);

/* ˢ����¼ */
void Record_Init(void);
void Record_Add(const SwipeRecord_t *pRec);
int  Record_QueryLast(uint32_t cardId, SwipeRecord_t *pOut);
void Record_List(uint32_t maxCount);
void Record_FixAllTimeOffsets(int32_t deltaSecs);
void     Record_MarkAllUploaded(void);      /**< ������м�¼���ϴ� */
uint32_t Record_GetUnuploadCount(void);     /**< ��ȡδ�ϴ���¼�� */

/* LRU ���� */
uint8_t Cache_Find(uint32_t cardId, uint32_t *pEnterSecs);
void    Cache_Update(uint32_t cardId, uint8_t eventType, uint32_t enterSecs);
void    Cache_Clear(void);

/* ����ʱ��ȡ����Ա���� */
void Config_SetAdmin(uint16_t deviceId, uint8_t attendMode);
uint16_t Config_GetDeviceId(void);
uint8_t  Config_GetAttendMode(void);

/* ���������� */
void    Blacklist_Init(void);
uint8_t Blacklist_Add(uint32_t uid);
uint8_t Blacklist_Remove(uint32_t uid);
uint8_t Blacklist_IsBlocked(uint32_t uid);

void    Blacklist_Save(void);
uint16_t Blacklist_GetCount(void);

#endif /* __RECORD_H__ */
