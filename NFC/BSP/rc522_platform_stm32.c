/**
 * @file    rc522_platform_stm32.c
 * @brief   RC522 STM32 骞冲彴閫傞厤灞?
 * @details 鏈?鏂囦欢鎻愪緵閽堝?? STM32F4 绯诲垪 (HAL 搴?) 鐨勫钩鍙? IO 鎺ュ彛瀹炵幇銆?
 *          浣跨敤 CubeMX 鐢熸垚鐨? GPIO 寮曡剼瀹氫箟锛岄€氳繃杞?浠舵ā鎷? SPI 涓? RC522 閫氳??銆?
 *
 *          寮曡剼杩炴帴 (榛樿?わ紝鍙?鍦? main.h 涓?淇?鏀?):
 *          - NFC_NSS(PE15)  -> 鐗囬€? (CS)
 *          - NFC_RST(PB15)  -> 澶嶄綅 (RST)
 *          - NFC_MOSI(PA0)  -> MOSI
 *          - NFC_MISO(PB13) -> MISO
 *          - NFC_SCK(PD9)   -> SCK
 *          - NFC_GND(PB10)  -> 鎺ュ湴鎺у埗 (杈撳嚭浣庣數骞?)
 *
 *          浣跨敤鏂规硶:
 *          1. 鍦? CubeMX 涓?閰嶇疆涓婅堪 GPIO 寮曡剼 (杈撳嚭: NSS, RST, MOSI, SCK; 杈撳叆: MISO)
 *          2. 璋冪敤 RC522_Platform_Init() 瀹屾垚鍒濆?嬪寲
 *          3. 涔嬪悗鍗冲彲璋冪敤 rc522.h 涓?鐨? API 鍑芥暟
 *
 * @note    缂栫爜: UTF-8
 */

#include "rc522.h"
#include "delay_us.h"   /* 公共微秒延时服务 */
#include "FreeRTOS.h"   /* FreeRTOS 基础头文件，须在 task.h 之前 */
#include "task.h"       /* taskENTER_CRITICAL / taskEXIT_CRITICAL / vTaskDelay */
#include "main.h"       /* 包含 NFC_XXX_Pin/GPIO_Port 定义 */

/* ======================================================
 *  GPIO 鎺у埗鍑芥暟 (RC522_IO_t 鍥炶皟瀹炵幇)
 * ====================================================== */

/**
 * @brief RC522 鐗囬€? (CS/NSS) 鎺у埗
 * @param level 0=閫変腑 (浣庣數骞?), 1=閲婃斁 (楂樼數骞?)
 */
static void cs_control(uint8_t level)
{
    if (level) {
        HAL_GPIO_WritePin(NFC_NSS_GPIO_Port, NFC_NSS_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(NFC_NSS_GPIO_Port, NFC_NSS_Pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief RC522 澶嶄綅 (RST) 鎺у埗
 * @param level 0=澶嶄綅 (浣庣數骞?), 1=姝ｅ父宸ヤ綔 (楂樼數骞?)
 */
static void rst_control(uint8_t level)
{
    if (level) {
        HAL_GPIO_WritePin(NFC_RST_GPIO_Port, NFC_RST_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(NFC_RST_GPIO_Port, NFC_RST_Pin, GPIO_PIN_RESET);
    }
}

/**
 * @brief SPI 鍏ㄥ弻宸ュ瓧鑺備紶杈? (杞?浠舵ā鎷? SPI Mode 0)
 * @param data 瑕佸彂閫佺殑鏁版嵁瀛楄妭
 * @return 鎺ユ敹鍒扮殑鏁版嵁瀛楄妭
 *
 * @note  SPI Mode 0 (CPOL=0, CPHA=0):
 *        - 绌洪棽鏃? SCK = 浣庣數骞?
 *        - 鏁版嵁鍦? SCK 涓婂崌娌垮彂閫? (MOSI 鏇存柊)
 *        - 鏁版嵁鍦? SCK 涓嬮檷娌挎帴鏀? (MISO 閲囨牱)
 *
 *        鏃跺簭:
 *        for 8 bits:
 *            SCK = LOW
 *            set MOSI (鏍规嵁 data 鐨勬渶楂樹綅)
 *            SCK = HIGH
 *            read MISO
 *            data <<= 1
 *            data |= MISO
 *        SCK = LOW (缁撴潫鐘舵€?)
 */
static uint8_t spi_transfer_byte(uint8_t data)
{
    uint8_t rx = 0;

    /* 涓寸晫鍖轰繚鎶わ細闃叉??浠诲姟鍒囨崲瀵艰嚧 SCK 鏃跺簭鏂?瑁?
     * 涓€涓?瀛楄妭浼犺緭绾? 32 碌s锛屼复鐣屽尯闀垮害鍙?鎺ュ彈 */
    taskENTER_CRITICAL();

    for (int8_t i = 7; i >= 0; i--) {
        /* SCK 涓嬮檷娌? -> 鎷変綆 SCK */
        HAL_GPIO_WritePin(NFC_SCK_GPIO_Port, NFC_SCK_Pin, GPIO_PIN_RESET);
        delay_us(1);

        /* 璁剧疆 MOSI 鏁版嵁浣? (MSB 浼樺厛) */
        if (data & 0x80) {
            HAL_GPIO_WritePin(NFC_MOSI_GPIO_Port, NFC_MOSI_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(NFC_MOSI_GPIO_Port, NFC_MOSI_Pin, GPIO_PIN_RESET);
        }
        delay_us(1);

        /* SCK 涓婂崌娌? -> 浠庢満閲囨牱 MOSI */
        HAL_GPIO_WritePin(NFC_SCK_GPIO_Port, NFC_SCK_Pin, GPIO_PIN_SET);
        delay_us(1);

        /* 鍦? SCK 楂樼數骞虫椂閲囨牱 MISO */
        rx <<= 1;
        if (HAL_GPIO_ReadPin(NFC_MISO_GPIO_Port, NFC_MISO_Pin) == GPIO_PIN_SET) {
            rx |= 0x01;
        }
        delay_us(1);

        data <<= 1;
    }

    /* 缁撴潫鐘舵€?: SCK 鍥炲埌浣庣數骞? */
    HAL_GPIO_WritePin(NFC_SCK_GPIO_Port, NFC_SCK_Pin, GPIO_PIN_RESET);
    delay_us(1);

    taskEXIT_CRITICAL();

    return rx;
}

/**
 * @brief 毫秒延时 (使用 FreeRTOS 任务延时, 释放 CPU 给其他任务)
 * @param ms 延时毫秒数
 */
static void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* ======================================================
 *  骞冲彴 IO 鎺ュ彛瀹炰緥
 * ====================================================== */

/** RC522 骞冲彴 IO 鎺ュ彛锛屼緵 RC522_Init() 浣跨敤 */
static const RC522_IO_t s_rc522_io = {
    .cs_control   = cs_control,
    .rst_control  = rst_control,
    .spi_transfer = spi_transfer_byte,
    .delay_us     = delay_us,
    .delay_ms     = delay_ms
};

/* ======================================================
 *  鍏?寮€ API
 * ====================================================== */

/**
 * @brief 鍒濆?嬪寲 RC522 STM32 骞冲彴
 * @note  姝ゅ嚱鏁板畬鎴?:
 *        1. DWT 瀹氭椂鍣ㄥ垵濮嬪寲 (寰?绉掑欢鏃?)
 *        2. NFC 妯″潡鎺ュ湴寮曡剼鎷変綆 (濡傛灉鏈夊畾涔?)
 *        3. 璋冪敤 RC522_Init() 鍒濆?嬪寲 RC522 鑺?鐗?
 *
 *        鍦? FreeRTOS 浠诲姟涓?璋冪敤姝ゅ嚱鏁板墠闇€纭?淇? HAL_Init()
 *        鍜? MX_GPIO_Init() 绛夌郴缁熷垵濮嬪寲宸插畬鎴愩€?
 */
void RC522_Platform_Init(void)
{
    /* 鍒濆?嬪寲鍏?鍏卞井绉掑欢鏃舵湇鍔? */
    delay_us_init();

    /* 鎷変綆 NFC_GND 寮曡剼锛屼负妯″潡鎻愪緵鍙傝€冨湴 */
#if defined(NFC_GND_Pin) && defined(NFC_GND_GPIO_Port)
    HAL_GPIO_WritePin(NFC_GND_GPIO_Port, NFC_GND_Pin, GPIO_PIN_RESET);
#endif

    /* 鍒濆?嬪寲 RC522 鑺?鐗? (浼犻€掑钩鍙? IO 鎺ュ彛) */
    RC522_Init((RC522_IO_t *)&s_rc522_io);
}
