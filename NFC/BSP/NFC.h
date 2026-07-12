/**
 * @file    NFC.h
 * @brief   NFC 卡读写封装模块 - 将寻卡、验证、读取、分类、位图读取集中管理
 * @details 封装 RC522 底层操作, 提供:
 *          - NFC_Init()        初始化 RC522 芯片
 *          - NFC_ScanCard()    寻卡 (替代直接调 RC522_ScanCard)
 *          - NFC_ReadCard()    读卡→校验→分类→读位图, 一次完成
 *          - NFC_WaitCardOff() 等待卡离开
 *
 *          卡数据结构 NFC_CardData_t 包含所有解析后的字段,
 *          图像位图数据通过 DISP_GetBitmapBuf() 写入共享缓冲区.
 */

#ifndef __NFC_H__
#define __NFC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "display.h"
#include "bsp_rtc.h"
#include <stdint.h>

/* ====== NFC 读卡事件类型 ====== */

typedef enum {
    NFC_EVT_NONE    = 0,  /**< 无卡/未检测到 */
    NFC_EVT_VALID,        /**< 有效普通卡或图像卡 */
    NFC_EVT_INVALID,      /**< 无效卡 (校验失败 / 挂失) */
    NFC_EVT_DUP,          /**< 重复刷卡 (上层处理) */
    NFC_EVT_ADMIN,        /**< 管理员卡 */
} NFC_Event_t;

/* ====== 卡数据解析结果 ====== */

typedef struct {
    uint8_t  uid[4];           /**< 卡 UID (4 字节) */
    uint32_t cardId;           /**< 卡号 (Block1 偏移 0~3) */
    uint32_t sid;              /**< 学号/工号 (Block1 偏移 4~7, 8 位十进制如 23041036) */
    uint32_t points;           /**< 积分 (Block1 偏移 8~11) */
    uint8_t  cardType;         /**< 卡类型: 0x00=普通 0x01=图像卡 0x02=管理员 */
    uint8_t  statusFlag;       /**< 状态标志: 0x00=正常 0xFF=挂失 */
    BSP_RTC_DateTime_t swipeDT; /**< 刷卡时刻 */
} NFC_CardData_t;

/* ====== API ====== */

void NFC_Init(void);
char NFC_ScanCard(uint8_t uid[4]);
NFC_Event_t NFC_ReadCard(const uint8_t uid[4], NFC_CardData_t *outCard);
NFC_Event_t NFC_ReadCardHeader(const uint8_t uid[4], NFC_CardData_t *outCard);
void NFC_WaitCardOff(void);
void NFC_Recover(void);

/* ====== 串口发卡协议 ====== */

/** 图像块缓存定义
 *  头像: 24 块 (扇区1~8, 块0~2) → IMGA00~IMGA23
 *  姓名: 10 块 (扇区9~12 块0) → IMGN00~IMGN09
 *  部门: 10 块 (扇区12块1~15块1) → IMGD00~IMGD09 */
#define NFC_IMG_AVATAR_BLOCKS   24
#define NFC_IMG_NAME_BLOCKS     10
#define NFC_IMG_DEPT_BLOCKS     10
#define NFC_IMG_BLOCK_SIZE      16

/**
 * @brief  处理上位机下发的一条命令
 * @param  cmd      上位机下发的命令字符串 (以 \n 结尾)
 * @param  respBuf  响应缓冲区
 * @param  bufSize  缓冲区大小
 * @return 响应字符串指针 (指向 respBuf)
 */
char *NFC_ProcessCommand(const char *cmd, char *respBuf, uint16_t bufSize);

/**
 * @brief  获取最近一次读卡的 UID 字符串 (用于 READ 命令响应)
 */
const char *NFC_GetLastUID(void);

/**
 * @brief  清空图像块缓存
 */
void NFC_ClearImageCache(void);

/* ====== 全量重读标志 (ISSUE 图像卡 / UPDATEIMG 后通知 nfcTask 刷新位图) ====== */
void    NFC_SetFullReread(void);        /**< 置位: 需要全量重读含图像扇区 */
uint8_t NFC_IsFullRereadNeeded(void);   /**< 查询: 是否需要全量重读 */
void    NFC_ClearFullReread(void);      /**< 清除标志 */

/* ====== RC522 并发锁 (串口任务 vs 刷卡任务) ====== */
uint8_t NFC_LockReader(void);    /**< 尝试锁定, 返回 1=成功 0=已被占用 */
void    NFC_UnlockReader(void);
uint8_t NFC_IsReaderLocked(void);

/* ====== 命令完成通知 (uartTask → nfcTask) ====== */
#define NFC_FLAG_CMD_DONE  (1UL << 0)   /**< 上位机命令已执行, nfcTask 应重读卡片 */
void NFC_NotifyCmdDone(void);           /**< uartTask 调用, 通知 nfcTask 命令完成 */
void NFC_ClearCmdDone(void);            /**< nfcTask 调用, 清除通知标志 */
uint8_t NFC_IsCmdDone(void);            /**< nfcTask 轮询, 检查是否有待处理的通知 */
void NFC_FlushCmdNotify(void);          /**< uartTask 发送响应后调用, 延迟提交 CmdDone */

#ifdef __cplusplus
}
#endif

#endif /* __NFC_H__ */
