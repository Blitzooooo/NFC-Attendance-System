/**
 * @file    record.c
 * @brief   打卡记录存储模块实现
 */

#include "record.h"
#include "w25qxx.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

/* ====== 模块内部变量 ====== */

static SysConfig_t    s_sysCfg;
static RecordHeader_t s_recHead;
static CardCache_t    s_cardCache[CARD_CACHE_SIZE];

/* 管理员配置运行时副本 (与 display 共享) */
static uint16_t s_deviceId   = 1;
static uint8_t  s_attendMode = 2;

/* ====== CRC16 (CCITT-FALSE) ====== */

uint16_t CRC16_Calc(const uint8_t *pData, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint16_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)pData[i] << 8;
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc = (crc << 1);
        }
    }
    return crc;
}

/* ====== 系统配置 ====== */

void Config_Init(void) { }

void Config_Load(void)
{
    SysConfig_t cfg;
    W25QXX_Read((uint8_t*)&cfg, CONFIG_FLASH_ADDR, sizeof(cfg));
    if (cfg.magic == CONFIG_MAGIC) {
        uint16_t crc = CRC16_Calc((uint8_t*)&cfg, 14);
        if (crc == cfg.crc16) {
            s_deviceId   = cfg.deviceId;
            s_attendMode = cfg.attendMode;
            memcpy(&s_sysCfg, &cfg, sizeof(cfg));
            return;
        }
    }
    /* 尝试备份 */
    W25QXX_Read((uint8_t*)&cfg, CONFIG_BAK_ADDR, sizeof(cfg));
    if (cfg.magic == CONFIG_MAGIC) {
        uint16_t crc = CRC16_Calc((uint8_t*)&cfg, 14);
        if (crc == cfg.crc16) {
            s_deviceId   = cfg.deviceId;
            s_attendMode = cfg.attendMode;
            memcpy(&s_sysCfg, &cfg, sizeof(cfg));
            return;
        }
    }
    /* 默认值 */
    s_deviceId   = 1;
    s_attendMode = 2;
}

void Config_Save(void)
{
    s_sysCfg.deviceId   = s_deviceId;
    s_sysCfg.attendMode = s_attendMode;
    s_sysCfg.timeOffset = 0;
    s_sysCfg.magic      = CONFIG_MAGIC;
    memset(s_sysCfg.reserved, 0, sizeof(s_sysCfg.reserved));
    s_sysCfg.crc16 = CRC16_Calc((uint8_t*)&s_sysCfg, 14);

    W25QXX_Write((uint8_t*)&s_sysCfg, CONFIG_BAK_ADDR, sizeof(s_sysCfg));
    W25QXX_Write((uint8_t*)&s_sysCfg, CONFIG_FLASH_ADDR, sizeof(s_sysCfg));
}

void Config_SetAdmin(uint16_t deviceId, uint8_t attendMode)
{
    s_deviceId   = deviceId;
    s_attendMode = attendMode;
}

uint16_t Config_GetDeviceId(void)  { return s_deviceId; }
uint8_t  Config_GetAttendMode(void) { return s_attendMode; }

/* ====== 刷卡记录 ====== */

void Record_Init(void)
{
    RecordHeader_t rh;
    W25QXX_Read((uint8_t*)&rh, RECHEAD_FLASH_ADDR, sizeof(rh));
    if (rh.magic == RECHEAD_MAGIC) {
        memcpy(&s_recHead, &rh, sizeof(rh));
    } else {
        memset(&s_recHead, 0, sizeof(s_recHead));
        s_recHead.magic   = RECHEAD_MAGIC;
        s_recHead.version = 1;
        W25QXX_Write((uint8_t*)&s_recHead, RECHEAD_FLASH_ADDR, sizeof(s_recHead));
    }
}

void Record_Add(const SwipeRecord_t *pRec)
{
    uint32_t addr = RECORD_FLASH_ADDR + s_recHead.writeOffset;
    SwipeRecord_t rec;

    memcpy(&rec, pRec, sizeof(rec));
    rec.seqNum = (uint32_t)(s_recHead.writeOffset / RECORD_SIZE) + 1;
    rec.crc16  = CRC16_Calc((uint8_t*)&rec, 30);

    W25QXX_Write((uint8_t*)&rec, addr, RECORD_SIZE);

    s_recHead.writeOffset += RECORD_SIZE;
    if (s_recHead.writeOffset >= RECORD_AREA_SIZE)
        s_recHead.writeOffset = 0;
    s_recHead.totalCount++;

    s_recHead.checksum = (uint16_t)(s_recHead.writeOffset ^ s_recHead.totalCount
                                    ^ s_recHead.uploadOffset);
    W25QXX_Write((uint8_t*)&s_recHead, RECHEAD_FLASH_ADDR, sizeof(s_recHead));
}

int Record_QueryLast(uint32_t uid, SwipeRecord_t *pOut)
{
    uint32_t off = s_recHead.writeOffset;
    int scanned = 0;

    if (off == 0) off = RECORD_AREA_SIZE;

    while (scanned < RECORD_MAX_COUNT) {
        off = (off >= RECORD_SIZE) ? (off - RECORD_SIZE) : (RECORD_AREA_SIZE - RECORD_SIZE);
        W25QXX_Read((uint8_t*)pOut, RECORD_FLASH_ADDR + off, RECORD_SIZE);
        scanned++;

        if (pOut->seqNum == 0 || pOut->seqNum == 0xFFFFFFFFU) continue;
        {
            uint16_t calcCrc = CRC16_Calc((uint8_t*)pOut, 30);
            if (calcCrc != pOut->crc16) continue;
        }
        if (pOut->uid == uid) return 1;
    }
    return 0;
}

void Record_List(uint32_t maxCount)
{
    SwipeRecord_t rec;
    uint32_t curOff;
    uint32_t outputCount = 0;

    uint32_t total = (s_recHead.totalCount < RECORD_MAX_COUNT)
                     ? s_recHead.totalCount : RECORD_MAX_COUNT;

    if (total == 0) {
        printf("LIST:0\n");
        return;
    }

    if (maxCount == 0 || maxCount > total) maxCount = total;
    printf("LIST:%lu\n", (unsigned long)maxCount);

    curOff = s_recHead.writeOffset;
    for (int i = 0; i < RECORD_MAX_COUNT && outputCount < maxCount; i++) {
        curOff = (curOff >= RECORD_SIZE) ? (curOff - RECORD_SIZE)
                                         : (RECORD_AREA_SIZE - RECORD_SIZE);
        W25QXX_Read((uint8_t*)&rec, RECORD_FLASH_ADDR + curOff, RECORD_SIZE);

        if (rec.seqNum == 0 || rec.seqNum == 0xFFFFFFFFU) continue;
        if (rec.uid == 0) continue;
        {
            uint16_t calcCrc = CRC16_Calc((uint8_t*)&rec, 30);
            if (calcCrc != rec.crc16) continue;
        }

        printf("REC:SEQ=%lu|UID=%08lX|SID=%lu|TYPE%u|%04u-%02u-%02u %02u:%02u:%02u|DEV=%u|OFS=%d|%s\n",
            (unsigned long)rec.seqNum,
            (unsigned long)rec.uid,
            (unsigned long)rec.sid,
            (unsigned)rec.cardType,
            (unsigned)(2000U + rec.year), (unsigned)rec.month, (unsigned)rec.day,
            (unsigned)rec.hour, (unsigned)rec.minute, (unsigned)rec.second,
            (unsigned)rec.deviceId,
            (int)(int16_t)rec.timeOffset,
            rec.status == 0 ? (rec.eventType == 0 ? "IN" : "OUT") :
            rec.status == 1 ? "DUP" :
            rec.status == 2 ? "NOIN" :
            rec.status == 4 ? "LOST" : "INV");
        outputCount++;
    }
}

void Record_MarkAllUploaded(void)
{
    s_recHead.uploadOffset = s_recHead.totalCount;
    /* 回写头部到 Flash */
    W25QXX_Erase_Sector(RECHEAD_FLASH_ADDR / 4096);
    s_recHead.checksum = (uint16_t)(s_recHead.writeOffset ^ s_recHead.totalCount
                                    ^ s_recHead.uploadOffset);
    W25QXX_Write((uint8_t*)&s_recHead, RECHEAD_FLASH_ADDR, sizeof(s_recHead));
}

uint32_t Record_GetUnuploadCount(void)
{
    if (s_recHead.totalCount >= s_recHead.uploadOffset)
        return s_recHead.totalCount - s_recHead.uploadOffset;
    return 0;
}

void Record_FixAllTimeOffsets(int32_t deltaSecs)
{
    static uint8_t sectorBuf[RECORD_AREA_SIZE];
    uint16_t i;
    int fixed = 0;

    if (deltaSecs == 0) return;

    W25QXX_Read(sectorBuf, RECORD_FLASH_ADDR, RECORD_AREA_SIZE);

    for (i = 0; i < REC_PER_SECTOR; i++) {
        SwipeRecord_t *rec = (SwipeRecord_t *)(sectorBuf + i * RECORD_SIZE);

        if (rec->seqNum == 0 || rec->seqNum == 0xFFFFFFFFU || rec->uid == 0)
            continue;

        uint16_t calcCrc = CRC16_Calc((uint8_t*)rec, 30);
        if (calcCrc != rec->crc16) continue;

        if (rec->timeOffset != 0xFFFF) continue;

        rec->timeOffset = (uint16_t)(int16_t)deltaSecs;
        rec->crc16 = CRC16_Calc((uint8_t*)rec, 30);
        fixed++;
    }

    if (fixed > 0) {
        W25QXX_Erase_Sector(RECORD_FLASH_ADDR);
        W25QXX_Write(sectorBuf, RECORD_FLASH_ADDR, RECORD_AREA_SIZE);
    }

    printf("[Record] Fixed %d records, OFS=%d\r\n", fixed, (int)deltaSecs);
}

/* ====== LRU 缓存 ====== */

uint8_t Cache_Find(uint32_t cardId, uint32_t *pEnterSecs)
{
    uint32_t now = osKernelGetTickCount();
    int i;
    if (cardId == 0) return CACHE_MISS;
    for (i = 0; i < CARD_CACHE_SIZE; i++) {
        if (s_cardCache[i].cardId == cardId) {
            s_cardCache[i].accessTick = now;
            if (pEnterSecs) *pEnterSecs = s_cardCache[i].enterSecs;
            return s_cardCache[i].eventType;
        }
    }
    return CACHE_MISS;
}

void Cache_Update(uint32_t cardId, uint8_t eventType, uint32_t enterSecs)
{
    uint32_t now = osKernelGetTickCount();
    int i, target = -1;

    if (cardId == 0) return;

    for (i = 0; i < CARD_CACHE_SIZE; i++) {
        if (s_cardCache[i].cardId == cardId) {
            s_cardCache[i].eventType  = eventType;
            s_cardCache[i].enterSecs  = enterSecs;
            s_cardCache[i].accessTick = now;
            return;
        }
    }

    for (i = 0; i < CARD_CACHE_SIZE; i++) {
        if (s_cardCache[i].cardId == 0) { target = i; break; }
    }

    if (target < 0) {
        uint32_t oldestTick = 0xFFFFFFFFU;
        for (i = 0; i < CARD_CACHE_SIZE; i++) {
            if (s_cardCache[i].accessTick < oldestTick) {
                oldestTick = s_cardCache[i].accessTick;
                target = i;
            }
        }
    }

    if (target >= 0) {
        s_cardCache[target].cardId     = cardId;
        s_cardCache[target].eventType  = eventType;
        s_cardCache[target].enterSecs  = enterSecs;
        s_cardCache[target].accessTick = now;
    }
}

void Cache_Clear(void)
{
    memset(s_cardCache, 0, sizeof(s_cardCache));
}

/* ====== 黑名单管理 ====== */

static BlacklistData_t s_blacklist;

void Blacklist_Init(void)
{
    BlacklistData_t bl;
    W25QXX_Read((uint8_t*)&bl, BLACKLIST_FLASH_ADDR, sizeof(bl));
    if (bl.magic == BLACKLIST_MAGIC) {
        uint16_t crc = CRC16_Calc((uint8_t*)&bl, offsetof(BlacklistData_t, crc16));
        if (crc == bl.crc16 && bl.count <= MAX_BLACKLIST) {
            memcpy(&s_blacklist, &bl, sizeof(bl));
            return;
        }
    }
    /* 无有效数据则初始化为空 */
    memset(&s_blacklist, 0, sizeof(s_blacklist));
    s_blacklist.magic = BLACKLIST_MAGIC;
}

void Blacklist_Save(void)
{
    s_blacklist.magic = BLACKLIST_MAGIC;
    s_blacklist.crc16 = CRC16_Calc((uint8_t*)&s_blacklist, offsetof(BlacklistData_t, crc16));
    W25QXX_Erase_Sector(BLACKLIST_FLASH_ADDR / 4096);
    W25QXX_Write((uint8_t*)&s_blacklist, BLACKLIST_FLASH_ADDR, sizeof(s_blacklist));
}

uint8_t Blacklist_Add(uint32_t uid)
{
    uint16_t i;
    if (uid == 0) return 0;
    /* 检查是否已存在 */
    for (i = 0; i < s_blacklist.count; i++) {
        if (s_blacklist.uidList[i] == uid) return 1;  /* 已存在 */
    }
    if (s_blacklist.count >= MAX_BLACKLIST) return 0;
    s_blacklist.uidList[s_blacklist.count++] = uid;
    Blacklist_Save();
    return 1;
}

uint8_t Blacklist_Remove(uint32_t uid)
{
    uint16_t i;
    for (i = 0; i < s_blacklist.count; i++) {
        if (s_blacklist.uidList[i] == uid) {
            /* 用最后一个元素覆盖当前, 减少 count */
            s_blacklist.uidList[i] = s_blacklist.uidList[--s_blacklist.count];
            Blacklist_Save();
            return 1;
        }
    }
    return 0;  /* 未找到 */
}

uint8_t Blacklist_IsBlocked(uint32_t uid)
{
    uint16_t i;
    for (i = 0; i < s_blacklist.count; i++) {
        if (s_blacklist.uidList[i] == uid) return 1;
    }
    return 0;
}



uint16_t Blacklist_GetCount(void)
{
    return s_blacklist.count;
}
