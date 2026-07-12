/**
 * @file    NFC.c
 * @brief   NFC 卡读写封装模块实现
 * @details 封装了以下流程:
 *          1. 密钥验证 + 读 Block1 (带重试)
 *          2. 校验和验证
 *          3. 卡数据字段解析
 *          4. 卡类型分类 (普通/图像/管理员)
 *          5. 姓名/部门/头像位图读取
 */

#include "NFC.h"
#include "rc522.h"
#include "uart_drv.h"
#include "delay_us.h"
#include "record.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ====== 内部常量 ====== */

/** 读卡重试次数 */
#define NFC_READ_RETRY      3

/** Mifare 默认密钥 (出厂值) */
static const uint8_t s_defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/** 单扇区数据块数 (不含密钥块) */
#define NFC_BLOCKS_PER_SEC  3

/** Block 数据长度 */
#define NFC_BLOCK_SIZE      16

/* ====== 内部辅助宏: 读一个扇区的非密钥块到缓冲区 ====== */

/**
 * @brief  读取指定扇区中前 3 个数据块到目标缓冲区
 * @param  sec    扇区号 (0~15)
 * @param  dst    目标缓冲区指针
 * @param  offset 写入偏移 (字节)
 * @note   自动处理密钥验证, 验证失败时静默跳过该扇区
 */
#define NFC_READ_SECTOR(sec, dst, offset) do {                                        \
    char _st;                                                                         \
    _st = RC522_AuthState(RC522_PICC_AUTHENT1A,                                       \
                          (uint8_t)((sec) * 4 + 3), s_defaultKey, outCard->uid);      \
    if (_st == RC522_OK) {                                                            \
        for (int _b = 0; _b < NFC_BLOCKS_PER_SEC; _b++) {                             \
            RC522_ReadBlock((sec), (uint8_t)_b,                                       \
                            (dst) + (offset) + _b * NFC_BLOCK_SIZE);                  \
        }                                                                             \
    }                                                                                 \
    osDelay(2);  /* 任务切换延时, 让出 CPU 给 guiTask 刷新 OLED */            \
} while(0)


/* ====== 公开 API ====== */

/**
 * @brief  初始化 NFC 模块
 */
void NFC_Init(void)
{
    RC522_Platform_Init();
    RC522_ConfigISOType('A');   /* ISO14443A, 内部自动开启天线 */
}

/**
 * @brief  寻卡
 */
char NFC_ScanCard(uint8_t uid[4])
{
    return RC522_ScanCard(uid);
}

/**
 * @brief  等待卡离开
 */
void NFC_WaitCardOff(void)
{
    RC522_WaitCardOff();
}

/**
 * @brief  RC522 硬件恢复: 复位芯片 + 重配置 ISO14443A + 重开天线
 * @note   在 UPDATEIMG/CLEAR 等密集操作后、或读卡连续失败后调用,
 *         防止 RC522 内部 FIFO/状态机残留导致后续通信异常。
 *         参考 NFCWriter 的 NFC_App_Recover()。
 */
void NFC_Recover(void)
{
    RC522_Reset();                 /* RST 引脚脉冲 + CMD_RESETPHASE 软复位 */
    osDelay(10);
    RC522_ConfigISOType('A');      /* 重配置 ISO14443A, 自动开天线 */
}

/**
 * @brief  读取并解析一张卡的全部数据
 * @note   处理流程:
 *          1. 捕捉刷卡时刻
 *          2. 验证扇区0 + 读取 Block1 (含重试)
 *          3. 校验和验证
 *          4. 解析字段: 卡号/工号/积分/卡类型/状态
 *          5. 分类 (管理员/图像/普通)
 *          6. 对有效卡读取姓名/部门/头像位图
 */
NFC_Event_t NFC_ReadCard(const uint8_t uid[4], NFC_CardData_t *outCard)
{
    char    status;
    uint8_t blockData[NFC_BLOCK_SIZE];
    int     readOk = 0;

    if (outCard == NULL || uid == NULL) {
        return NFC_EVT_INVALID;
    }

    /* ---- 初始化输出结构 ---- */
    memset(outCard, 0, sizeof(NFC_CardData_t));
    memcpy(outCard->uid, uid, 4);

    /* 捕获刷卡时刻 */
    BSP_RTC_GetDateTime(&outCard->swipeDT);

    /* ===== 第1步: 验证扇区0 + 读 Block1 (含重试) ===== */
    for (int retry = 0; retry < NFC_READ_RETRY; retry++) {
        if (retry > 0) {
            /* 重试前先 Halt + 重新寻卡 */
            RC522_Halt();
            osDelay(5);
            uint8_t reUID[4];
            status = RC522_ScanCard(reUID);
            if (status != RC522_OK) continue;
        }

        /* 验证扇区0 密钥块 (块地址 0*4+3 = 3) */
        status = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                 3, (uint8_t *)s_defaultKey, outCard->uid);
        if (status != RC522_OK) continue;

        /* 读扇区0 块1 (账户头数据) */
        status = RC522_ReadBlock(0, 1, blockData);
        if (status == RC522_OK) {
            readOk = 1;
            break;
        }
    }

    if (!readOk) {
        return NFC_EVT_INVALID;
    }

    /* ===== 第2步: 校验和验证 ===== */
    uint16_t sum = 0;
    for (int i = 0; i < 14; i++) {
        sum += blockData[i];
    }
    uint16_t checksum = blockData[14] | ((uint16_t)blockData[15] << 8);

    outCard->statusFlag = blockData[13];

    /* 校验失败 或 挂失卡 → 无效 */
    if (sum != checksum || outCard->statusFlag == 0xFF) {
        return NFC_EVT_INVALID;
    }

    /* ===== 第3步: 解析卡数据字段 ===== */
    outCard->cardId =  (uint32_t)blockData[0]        |
                      ((uint32_t)blockData[1] << 8)  |
                      ((uint32_t)blockData[2] << 16) |
                      ((uint32_t)blockData[3] << 24);

    outCard->sid   =  (uint32_t)blockData[4]        |
                      ((uint32_t)blockData[5] << 8)  |
                      ((uint32_t)blockData[6] << 16) |
                      ((uint32_t)blockData[7] << 24);

    outCard->points =  (uint32_t)blockData[8]        |
                      ((uint32_t)blockData[9] << 8)  |
                      ((uint32_t)blockData[10] << 16) |
                      ((uint32_t)blockData[11] << 24);

    outCard->cardType = blockData[12];

    /* 全零数据块视为空卡 (销卡后), 无需读取图像扇区 */
    if (outCard->cardId == 0 && outCard->cardType == 0) {
        return NFC_EVT_INVALID;
    }

    /* ===== 第4步: 卡类型分类 ===== */
    NFC_Event_t event;
    switch (outCard->cardType) {
    case 0x02:
        event = NFC_EVT_ADMIN;
        break;
    case 0x01:
    default:
        event = NFC_EVT_VALID;
        break;
    }

    /* ===== 第5步: 读取姓名/部门/头像位图 (管理员卡跳过, 节省 ~200ms) ===== */
    if (event != NFC_EVT_ADMIN) {
        DispCardBitmap_t *bm = DISP_GetBitmapBuf();
        memset(bm, 0, sizeof(DispCardBitmap_t));

        /* -- 姓名: 10 块 = 160 字节 (扇区 9~11 全 3 块 + 扇区 12 块 0) -- */
        NFC_READ_SECTOR(9,  bm->name, 0);
        NFC_READ_SECTOR(10, bm->name, 48);
        NFC_READ_SECTOR(11, bm->name, 96);
        {
            char _st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                       (uint8_t)(12 * 4 + 3),
                                       s_defaultKey, outCard->uid);
            if (_st == RC522_OK) {
                RC522_ReadBlock(12, 0, bm->name + 144);
            }
        }
        osDelay(2);

        /* -- 部门: 10 块 = 160 字节 (扇区 12 块 1~2 + 扇区 13~14 全 + 扇区 15 块 0~1) -- */
        {
            char _st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                       (uint8_t)(12 * 4 + 3),
                                       s_defaultKey, outCard->uid);
            if (_st == RC522_OK) {
                RC522_ReadBlock(12, 1, bm->dept + 0);
                RC522_ReadBlock(12, 2, bm->dept + 16);
            }
        }
        osDelay(2);
        NFC_READ_SECTOR(13, bm->dept, 32);
        NFC_READ_SECTOR(14, bm->dept, 80);
        {
            char _st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                       (uint8_t)(15 * 4 + 3),
                                       s_defaultKey, outCard->uid);
            if (_st == RC522_OK) {
                RC522_ReadBlock(15, 0, bm->dept + 128);
                RC522_ReadBlock(15, 1, bm->dept + 144);
            }
        }
        osDelay(2);

        /* -- 头像: 仅图像卡 (扇区 1~8, 共 24 块 = 384 字节位图) -- */
        if (outCard->cardType == 0x01) {
            NFC_READ_SECTOR(1, bm->avatar, 0);
            NFC_READ_SECTOR(2, bm->avatar, 48);
            NFC_READ_SECTOR(3, bm->avatar, 96);
            NFC_READ_SECTOR(4, bm->avatar, 144);
            NFC_READ_SECTOR(5, bm->avatar, 192);
            NFC_READ_SECTOR(6, bm->avatar, 240);
            NFC_READ_SECTOR(7, bm->avatar, 288);
            NFC_READ_SECTOR(8, bm->avatar, 336);
            /* 扫描全部 384 字节判断头像是否存在 (仅 32 字节易因黑边上角误判) */
            bm->hasAvatar = 0;
            for (int _j = 0; _j < (int)sizeof(bm->avatar); _j++) {
                if (bm->avatar[_j] != 0) { bm->hasAvatar = 1; break; }
            }
        }
    }

    return event;
}

/**
 * @brief  轻量读卡: 仅读 Block1 账户头, 不读图像扇区
 * @note   用于 CmdDone 快速重读 (printf 上报仅需账户头字段),
 *         大幅缩短持锁时间 (约 10ms vs 全量读的 ~1s)。
 *         不调用 DISP_GetBitmapBuf(), 不修改 s_cardBitmap。
 */
NFC_Event_t NFC_ReadCardHeader(const uint8_t uid[4], NFC_CardData_t *outCard)
{
    char    status;
    uint8_t blockData[NFC_BLOCK_SIZE];
    int     readOk = 0;

    if (outCard == NULL || uid == NULL) {
        return NFC_EVT_INVALID;
    }

    memset(outCard, 0, sizeof(NFC_CardData_t));
    memcpy(outCard->uid, uid, 4);
    BSP_RTC_GetDateTime(&outCard->swipeDT);

    /* 读扇区0 Block1 (含重试) */
    for (int retry = 0; retry < NFC_READ_RETRY; retry++) {
        if (retry > 0) {
            RC522_Halt();
            osDelay(5);
            uint8_t reUID[4];
            status = RC522_ScanCard(reUID);
            if (status != RC522_OK) continue;
        }
        status = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                 3, (uint8_t *)s_defaultKey, outCard->uid);
        if (status != RC522_OK) continue;
        status = RC522_ReadBlock(0, 1, blockData);
        if (status == RC522_OK) { readOk = 1; break; }
    }

    if (!readOk) return NFC_EVT_INVALID;

    /* 校验和 */
    uint16_t sum = 0;
    for (int i = 0; i < 14; i++) sum += blockData[i];
    uint16_t checksum = blockData[14] | ((uint16_t)blockData[15] << 8);
    outCard->statusFlag = blockData[13];
    if (sum != checksum || outCard->statusFlag == 0xFF)
        return NFC_EVT_INVALID;

    /* 解析字段 */
    outCard->cardId =  (uint32_t)blockData[0]        |
                      ((uint32_t)blockData[1] << 8)  |
                      ((uint32_t)blockData[2] << 16) |
                      ((uint32_t)blockData[3] << 24);
    outCard->sid   =  (uint32_t)blockData[4]        |
                      ((uint32_t)blockData[5] << 8)  |
                      ((uint32_t)blockData[6] << 16) |
                      ((uint32_t)blockData[7] << 24);
    outCard->points =  (uint32_t)blockData[8]        |
                      ((uint32_t)blockData[9] << 8)  |
                      ((uint32_t)blockData[10] << 16) |
                      ((uint32_t)blockData[11] << 24);
    outCard->cardType = blockData[12];

    if (outCard->cardId == 0 && outCard->cardType == 0)
        return NFC_EVT_INVALID;

    switch (outCard->cardType) {
    case 0x02: return NFC_EVT_ADMIN;
    case 0x01:
    default:   return NFC_EVT_VALID;
    }
}

/* ====== 串口发卡协议实现 ====== */

/** 图像块缓存 */
static uint8_t s_imgAvatar[NFC_IMG_AVATAR_BLOCKS][NFC_IMG_BLOCK_SIZE];
static uint8_t s_imgName[NFC_IMG_NAME_BLOCKS][NFC_IMG_BLOCK_SIZE];
static uint8_t s_imgDept[NFC_IMG_DEPT_BLOCKS][NFC_IMG_BLOCK_SIZE];

/** 最近一次读卡 UID 缓存 (用于 READ 响应) */
static uint8_t s_lastUID[4];
static char    s_lastUIDStr[16];

/** RC522 并发锁 (0=空闲, 1=被占用) */
static volatile uint8_t s_readerLocked = 0;

uint8_t NFC_LockReader(void)   {
    __disable_irq();
    if (s_readerLocked) { __enable_irq(); return 0; }
    s_readerLocked = 1;
    __enable_irq();
    return 1;
}
void    NFC_UnlockReader(void) { s_readerLocked = 0; }
uint8_t NFC_IsReaderLocked(void) { return s_readerLocked; }

/** 命令完成通知标志 (uartTask 设置, nfcTask 轮询并清除) */
static volatile uint8_t s_cmdDone = 0;
/** 延迟通知标志: uartTask 先发响应再置位 s_cmdDone, 保证串口输出有序 */
static volatile uint8_t s_pendingNotify = 0;

void NFC_NotifyCmdDone(void) { s_cmdDone = 1; }
void NFC_ClearCmdDone(void)  { s_cmdDone = 0; }
uint8_t NFC_IsCmdDone(void)  { return s_cmdDone; }

/**
 * @brief  冲刷延迟通知: uartTask 在发送完响应后调用, 将待处理通知提交给 nfcTask
 */
void NFC_FlushCmdNotify(void)
{
    if (s_pendingNotify) {
        s_pendingNotify = 0;
        s_cmdDone = 1;
    }
}

/** 全量重读标志: ISSUE 图像卡 / UPDATEIMG 后置位, nfcTask CmdDone 处理时检查 */
static volatile uint8_t s_needFullReread = 0;

void NFC_SetFullReread(void)        { s_needFullReread = 1; }
uint8_t NFC_IsFullRereadNeeded(void) { return s_needFullReread; }
void NFC_ClearFullReread(void)      { s_needFullReread = 0; }

/* ====== 内部辅助 ====== */

/** 解析 HEX 字符串 (2字符→1字节) */
static int HexToByte(char hi, char lo, uint8_t *out)
{
    int val = 0;
    if (hi >= '0' && hi <= '9') val = (hi - '0') << 4;
    else if (hi >= 'A' && hi <= 'F') val = (hi - 'A' + 10) << 4;
    else if (hi >= 'a' && hi <= 'f') val = (hi - 'a' + 10) << 4;
    else return -1;
    if (lo >= '0' && lo <= '9') val |= (lo - '0');
    else if (lo >= 'A' && lo <= 'F') val |= (lo - 'A' + 10);
    else if (lo >= 'a' && lo <= 'f') val |= (lo - 'a' + 10);
    else return -1;
    *out = (uint8_t)val;
    return 0;
}

/** 解析 32 字节 HEX 字符串 → 16 字节数据 */
static int ParseHex32(const char *hex, uint8_t buf[16])
{
    for (int i = 0; i < 16; i++) {
        if (HexToByte(hex[i*2], hex[i*2+1], &buf[i]) != 0)
            return -1;
    }
    return 0;
}

/** 将 UID 4 字节转为 HEX 字符串 */
static void UIDToStr(const uint8_t uid[4], char *str)
{
    sprintf(str, "%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
}

/* ====== 命令处理 ====== */

/**
 * @brief  写账户头到卡 Block1
 * @param  cardId   卡号 (4B HEX)
 * @param  sid      学号/工号 (十进制, 8 位如 23041036)
 * @param  points   积分
 * @param  cardType 卡类型
 * @return 响应字符串
 */
static const char *NFC_CmdIssue(const char *args)
{
    uint32_t cardId, sid, points;
    int      cardType;
    uint8_t  blockData[16];
    uint8_t  uid[4];
    char     status;

    if (sscanf(args, "%x,%u,%u,%d", &cardId, &sid, &points, &cardType) < 4) {
        return "ERR:PARSE\n";
    }
    if (cardType < 0 || cardType > 2) return "ERR:PARSE\n";

    /* 寻卡 (先 Halt 复位, 避免卡留在 ACTIVE 态) */
    RC522_Halt();
    osDelay(2);
    status = RC522_ScanCard(uid);
    if (status != RC522_OK) return "ERR:NOCARD\n";

    /* 构造 Block1 数据 */
    memset(blockData, 0, sizeof(blockData));
    blockData[0]  = (uint8_t)(cardId);
    blockData[1]  = (uint8_t)(cardId >> 8);
    blockData[2]  = (uint8_t)(cardId >> 16);
    blockData[3]  = (uint8_t)(cardId >> 24);
    blockData[4]  = (uint8_t)(sid);
    blockData[5]  = (uint8_t)(sid >> 8);
    blockData[6]  = (uint8_t)(sid >> 16);
    blockData[7]  = (uint8_t)(sid >> 24);
    blockData[8]  = (uint8_t)(points);
    blockData[9]  = (uint8_t)(points >> 8);
    blockData[10] = (uint8_t)(points >> 16);
    blockData[11] = (uint8_t)(points >> 24);
    blockData[12] = (uint8_t)cardType;
    blockData[13] = 0x00;  /* statusFlag: 正常 */

    /* 校验和 (前14字节累加) */
    uint16_t sum = 0;
    for (int i = 0; i < 14; i++) sum += blockData[i];
    blockData[14] = (uint8_t)(sum);
    blockData[15] = (uint8_t)(sum >> 8);

    /* 认证 + 写入 */
    status = RC522_AuthState(RC522_PICC_AUTHENT1A, 3, (uint8_t*)s_defaultKey, uid);
    if (status != RC522_OK) { NFC_Recover(); return "ERR:AUTH\n"; }

    status = RC522_WriteBlock(0, 1, blockData);
    if (status != RC522_OK) { NFC_Recover(); return "ERR:WRITE_B1\n"; }

    /* 写完后 Halt 卡, 使卡回休眠态 */
    RC522_Halt();

    /* 缓存 UID */
    memcpy(s_lastUID, uid, 4);
    UIDToStr(uid, s_lastUIDStr);

    return "OK\n";
}

/**
 * @brief  读卡验证并返回数据
 */
static const char *NFC_CmdRead(char *respBuf, uint16_t bufSize)
{
    uint8_t uid[4];
    uint8_t blockData[16];
    char    status;

    RC522_Halt();
    osDelay(2);
    status = RC522_ScanCard(uid);
    if (status != RC522_OK) return "ERR:NOCARD\n";

    status = RC522_AuthState(RC522_PICC_AUTHENT1A, 3, (uint8_t*)s_defaultKey, uid);
    if (status != RC522_OK) return "ERR:AUTH\n";

    status = RC522_ReadBlock(0, 1, blockData);
    if (status != RC522_OK) return "ERR:WRITE_B1\n";

    /* 缓存 UID */
    memcpy(s_lastUID, uid, 4);
    UIDToStr(uid, s_lastUIDStr);

    uint32_t sid = (uint32_t)blockData[4]       | ((uint32_t)blockData[5] << 8) |
                     ((uint32_t)blockData[6] << 16) | ((uint32_t)blockData[7] << 24);

    snprintf(respBuf, bufSize,
             "UID:%s\nSID:%lu\nTYPE:%d\n",
             s_lastUIDStr, (unsigned long)sid, blockData[12]);
    return respBuf;
}

/**
 * @brief  IMGA/IMGN/IMGD 图像块缓存
 * @param  hex32  32 字符 HEX
 * @param  idx    块索引
 * @param  cache  目标缓存数组
 * @param  maxIdx 最大索引
 */
static const char *NFC_CmdImageBlock(const char *hex32, int idx,
                                      uint8_t cache[][16], int maxIdx)
{
    if (idx < 0 || idx >= maxIdx) return "ERR:IMG_BLOCK\n";
    if (strlen(hex32) < 32) return "ERR:IMG_HEX\n";
    if (ParseHex32(hex32, cache[idx]) != 0) return "ERR:IMG_HEX\n";
    return "OK\n";
}

/**
 * @brief  UPDATEIMG: 将缓存的图像块写入卡片
 *         头像: 扇区1~8 (1*4+3=7 ~ 8*4+3=35)
 *         姓名: 扇区9~12块0
 *         部门: 扇区12块1~15块1
 */
static const char *NFC_CmdUpdateImg(void)
{
    uint8_t uid[4];
    char    status;
    int     i, sec, blk;

    RC522_Halt();
    osDelay(2);
    status = RC522_ScanCard(uid);
    if (status != RC522_OK) return "ERR:NOCARD\n";

    /* ---- 写头像 (扇区 1~8, 每扇区 3 块) ---- */
    for (i = 0; i < NFC_IMG_AVATAR_BLOCKS; i++) {
        sec = 1 + i / 3;
        blk = i % 3;
        status = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                 (uint8_t)(sec * 4 + 3),
                                 (uint8_t*)s_defaultKey, uid);
        if (status != RC522_OK) { NFC_Recover(); return "ERR:AUTH\n"; }
        status = RC522_WriteBlock(sec, blk, s_imgAvatar[i]);
        if (status != RC522_OK) { NFC_Recover(); return "ERR:WRITE_B1\n"; }
        osDelay(10);  /* 等待卡 EEPROM 写入完成, 同时让出 CPU 给 guiTask 刷新 OLED */
    }

    /* ---- 写姓名 (扇区 9~11 全 + 扇区12 块0) ---- */
    for (i = 0; i < NFC_IMG_NAME_BLOCKS; i++) {
        sec = 9 + i / 3;
        blk = i % 3;
        status = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                 (uint8_t)(sec * 4 + 3),
                                 (uint8_t*)s_defaultKey, uid);
        if (status != RC522_OK) { NFC_Recover(); return "ERR:AUTH\n"; }
        status = RC522_WriteBlock(sec, blk, s_imgName[i]);
        if (status != RC522_OK) { NFC_Recover(); return "ERR:WRITE_B1\n"; }
        osDelay(10);
    }

    /* ---- 写部门 (扇区 12 块1~2 + 扇区 13~14 全 + 扇区 15 块0~1) ---- */
    for (i = 0; i < NFC_IMG_DEPT_BLOCKS; i++) {
        if (i < 2) { sec = 12; blk = 1 + i; }           /* 扇区12 块1,2 */
        else if (i < 5) { sec = 13; blk = i - 2; }       /* 扇区13 块0,1,2 */
        else if (i < 8) { sec = 14; blk = i - 5; }       /* 扇区14 块0,1,2 */
        else             { sec = 15; blk = i - 8; }       /* 扇区15 块0,1 */
        status = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                 (uint8_t)(sec * 4 + 3),
                                 (uint8_t*)s_defaultKey, uid);
        if (status != RC522_OK) { NFC_Recover(); return "ERR:AUTH\n"; }
        status = RC522_WriteBlock(sec, blk, s_imgDept[i]);
        if (status != RC522_OK) { NFC_Recover(); return "ERR:WRITE_B1\n"; }
        osDelay(10);
    }

    /* 写完后 Halt 卡 + 硬件恢复 RC522 (密集写入可能腐败芯片状态) */
    RC522_Halt();
    osDelay(5);
    NFC_Recover();
    NFC_ClearImageCache();  /* 释放 704 字节静态缓存 */

    /* 通知 nfcTask 全量重读以刷新 OLED 位图 */
    NFC_SetFullReread();

    return "OK\n";
}

/* ====== 公开 API ====== */

const char *NFC_GetLastUID(void)
{
    return s_lastUIDStr;
}

void NFC_ClearImageCache(void)
{
    memset(s_imgAvatar, 0, sizeof(s_imgAvatar));
    memset(s_imgName,   0, sizeof(s_imgName));
    memset(s_imgDept,   0, sizeof(s_imgDept));
}

/**
 * @brief  处理上位机下发的一条命令
 */
char *NFC_ProcessCommand(const char *cmd, char *respBuf, uint16_t bufSize)
{
    const char *resp = "ERR:UNKNOWN_CMD\n";

    if (cmd == NULL || respBuf == NULL) return NULL;

    while (*cmd == ' ' || *cmd == '\r' || *cmd == '\n') cmd++;
    if (*cmd == '\0') { strncpy(respBuf, "OK\n", bufSize); return respBuf; }

    /* ---- 无需 RC522 的命令 (仅缓存/查询, 不锁) ---- */
    if (strncmp(cmd, "IMGA", 4) == 0) {
        int idx; const char *hex;
        if (sscanf(cmd + 4, "%d:", &idx) == 1 && (hex = strchr(cmd + 4, ':')))
            resp = NFC_CmdImageBlock(hex + 1, idx, s_imgAvatar, NFC_IMG_AVATAR_BLOCKS);
        else resp = "ERR:IMG_BLOCK\n";
    }
    else if (strncmp(cmd, "IMGN", 4) == 0) {
        int idx; const char *hex;
        if (sscanf(cmd + 4, "%d:", &idx) == 1 && (hex = strchr(cmd + 4, ':')))
            resp = NFC_CmdImageBlock(hex + 1, idx, s_imgName, NFC_IMG_NAME_BLOCKS);
        else resp = "ERR:IMG_BLOCK\n";
    }
    else if (strncmp(cmd, "IMGD", 4) == 0) {
        int idx; const char *hex;
        if (sscanf(cmd + 4, "%d:", &idx) == 1 && (hex = strchr(cmd + 4, ':')))
            resp = NFC_CmdImageBlock(hex + 1, idx, s_imgDept, NFC_IMG_DEPT_BLOCKS);
        else resp = "ERR:IMG_BLOCK\n";
    }
    else if (strncmp(cmd, "LIST", 4) == 0) {
        uint32_t maxCount = 0;  /* 0 = 全部 */
        if (cmd[4] == ':') {
            if (strncmp(cmd + 5, "ALL", 3) != 0) {
                maxCount = (uint32_t)atoi(cmd + 5);
            }
        }
        Record_List(maxCount);
        Record_MarkAllUploaded();  /* LIST 查询过即视为已上传 */
        resp = "LIST:END\n";
    }
    else if (strncmp(cmd, "LOST:", 5) == 0) {
        /* 挂失: 将卡 UID 加入黑名单 */
        uint32_t uid = 0;
        if (sscanf(cmd + 5, "%8lx", (unsigned long*)&uid) == 1 && uid != 0) {
            if (Blacklist_Add(uid)) {
                printf("[BL] ADD UID=%08lX, count=%u\r\n",
                       (unsigned long)uid, (unsigned)Blacklist_GetCount());
                resp = "OK\n";
            } else {
                resp = "ERR:BL_FULL\n";
            }
        } else {
            resp = "ERR:PARSE\n";
        }
    }
    else if (strncmp(cmd, "UNLOST:", 7) == 0) {
        /* 解除挂失: 从黑名单移除 */
        uint32_t uid = 0;
        if (sscanf(cmd + 7, "%8lx", (unsigned long*)&uid) == 1 && uid != 0) {
            if (Blacklist_Remove(uid)) {
                printf("[BL] REMOVE UID=%08lX, count=%u\r\n",
                       (unsigned long)uid, (unsigned)Blacklist_GetCount());
                resp = "OK\n";
            } else {
                resp = "ERR:NOT_FOUND\n";
            }
        } else {
            resp = "ERR:PARSE\n";
        }
    }
    else {
        /* ---- 需要 RC522 的命令: 先锁定再执行 (带重试, 容忍 nfcTask 短暂持锁) ---- */
        {
            int retry;
            for (retry = 0; retry < 50; retry++) {
                if (NFC_LockReader()) break;
                osDelay(10);
            }
            if (retry >= 50) {
                strncpy(respBuf, "ERR:BUSY\n", bufSize);
                return respBuf;
            }
        }

        if (strncmp(cmd, "READ", 4) == 0) {
            resp = NFC_CmdRead(respBuf, bufSize);
        }
        else if (strncmp(cmd, "ISSUE:", 6) == 0) {
            resp = NFC_CmdIssue(cmd + 6);
        }
        else if (strncmp(cmd, "UPDATEIMG", 9) == 0) {
            resp = NFC_CmdUpdateImg();
        }
        else if (strncmp(cmd, "CLEAR", 5) == 0) {
            uint8_t uid[4]; char st;
            RC522_Halt();
            osDelay(2);
            st = RC522_ScanCard(uid);
            if (st != RC522_OK) resp = "ERR:NOCARD\n";
            else {
                NFC_ClearImageCache();
                uint8_t zero[16] = {0};

                /* 清零账户头 Block1 */
                st = RC522_AuthState(RC522_PICC_AUTHENT1A, 3, (uint8_t*)s_defaultKey, uid);
                if (st != RC522_OK) { NFC_Recover(); resp = "ERR:AUTH\n"; }
                else {
                    st = RC522_WriteBlock(0, 1, zero);
                    if (st != RC522_OK) { NFC_Recover(); resp = "ERR:WRITE_B1\n"; }
                    else {
                        /* 清零所有图像数据块 (扇区 1~15, 每扇区块 0~2) */
                        int sec, blk;
                        for (sec = 1; sec <= 15 && st == RC522_OK; sec++) {
                            st = RC522_AuthState(RC522_PICC_AUTHENT1A,
                                                 (uint8_t)(sec * 4 + 3),
                                                 (uint8_t*)s_defaultKey, uid);
                            if (st != RC522_OK) break;
                            for (blk = 0; blk < 3; blk++) {
                                st = RC522_WriteBlock(sec, blk, zero);
                                if (st != RC522_OK) break;
                            }
                        }
                        if (st == RC522_OK) {
                            RC522_Halt();
                            osDelay(5);
                            NFC_Recover();
                            resp = "OK\n";
                        } else {
                            NFC_Recover();
                            resp = "ERR:CLR_IMG\n";
                        }
                    }
                }
            }
        }
        else {
            resp = "ERR:UNKNOWN_CMD\n";
        }

        NFC_UnlockReader();

        /* 标记待通知, 由 uartTask 在发送响应后调用 NFC_FlushCmdNotify() */
        s_pendingNotify = 1;
    }

    if (resp != respBuf) { strncpy(respBuf, resp, bufSize); }
    return respBuf;
}
