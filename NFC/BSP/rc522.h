/**
 * @file    rc522.h
 * @brief   MFRC522/RC522 RFID璇诲啓鑺?鐗囬┍鍔? - 涓诲ご鏂囦欢
 * @details 鏈?椹卞姩閲囩敤鍒嗗眰璁捐?★紝閫氳繃 RC522_IO_t 鎺ュ彛涓庣‖浠跺钩鍙拌В鑰︺€?
 *          鐢ㄦ埛鍙?闇€瀹炵幇 spi_transfer銆乧s_control銆乺st_control銆乨elay_us/delay_ms
 *          浜斾釜鍥炶皟鍑芥暟鍗冲彲灏嗛┍鍔ㄧЩ妞嶅埌浠绘剰骞冲彴銆?
 *
 *          浣跨敤姝ラ?わ細
 *          1. 瀹炵幇 RC522_IO_t 鎺ュ彛涓?鐨勫洖璋冨嚱鏁?
 *          2. 璋冪敤 RC522_Init() 鍒濆?嬪寲椹卞姩
 *          3. 璋冪敤 RC522_ScanCard() 瀵诲崱
 *          4. 璋冪敤 RC522_AuthState() 璁よ瘉鍚庤?诲啓鍗℃暟鎹?
 *          5. 璋冪敤 RC522_Halt() 浣垮崱杩涘叆浼戠湢
 *
 * @note    缂栫爜锛歎TF-8
 */

#ifndef RC522_H
#define RC522_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================
 *  1. 骞冲彴鎶借薄鎺ュ彛 (Platform IO Interface)
 *     鐢ㄦ埛蹇呴』瀹炵幇姝ゆ帴鍙ｄ腑鐨勬墍鏈夊洖璋冨嚱鏁?
 * ====================================================== */
typedef struct {
    /**
     * @brief 鐗囬€?(CS/NSS)寮曡剼鎺у埗
     * @param level 0=鎷変綆(閫変腑), 1=鎷夐珮(閲婃斁)
     */
    void (*cs_control)(uint8_t level);

    /**
     * @brief 澶嶄綅(RST)寮曡剼鎺у埗
     * @param level 0=鎷変綆(澶嶄綅), 1=鎷夐珮(姝ｅ父宸ヤ綔)
     */
    void (*rst_control)(uint8_t level);

    /**
     * @brief SPI鍏ㄥ弻宸ュ瓧鑺備紶杈?
     * @param data 瑕佸彂閫佺殑鏁版嵁瀛楄妭
     * @return 鎺ユ敹鍒扮殑鏁版嵁瀛楄妭
     * @note 瀵逛簬杞?浠舵ā鎷烻PI锛屾?ゅ嚱鏁伴渶鑷?琛屾帶鍒? SCK 鍜? MOSI锛?
     *       骞惰?诲彇 MISO銆傛暟鎹?鍦? SCK 涓婂崌娌垮彂閫侊紝涓嬮檷娌挎帴鏀躲€?
     *       鏃堕挓鏋佹€?(CPOL)=0, 鏃堕挓鐩镐綅(CPHA)=0 (Mode 0)銆?
     */
    uint8_t (*spi_transfer)(uint8_t data);

    /**
     * @brief 寰?绉掔骇寤舵椂
     * @param us 寤舵椂寰?绉掓暟
     */
    void (*delay_us)(uint32_t us);

    /**
     * @brief 姣?绉掔骇寤舵椂
     * @param ms 寤舵椂姣?绉掓暟
     */
    void (*delay_ms)(uint32_t ms);
} RC522_IO_t;

/* ======================================================
 *  2. 鐘舵€佺爜 (Status Codes)
 * ====================================================== */
#define RC522_OK            ((char)0)      /**< 鎿嶄綔鎴愬姛 */
#define RC522_ERR           ((char)(-1))   /**< 鎿嶄綔澶辫触 */
#define RC522_ERR_NO_TAG    ((char)(-2))   /**< 鏃犳爣绛?(鍗?)鍝嶅簲 */
#define RC522_ERR_COLLISION ((char)(-3))   /**< 闃茬?版挒澶辫触 */

/* ======================================================
 *  3. MF522 鑺?鐗囧懡浠? (PCD - Proximity Coupling Device Commands)
 * ====================================================== */
#define RC522_CMD_IDLE          0x00    /**< 鍙栨秷褰撳墠鍛戒护 */
#define RC522_CMD_AUTHENT       0x0E    /**< 瀵嗙爜楠岃瘉 */
#define RC522_CMD_RECEIVE       0x08    /**< 鎺ユ敹鏁版嵁 */
#define RC522_CMD_TRANSMIT      0x04    /**< 鍙戦€佹暟鎹? */
#define RC522_CMD_TRANSCEIVE    0x0C    /**< 鍙戦€佸苟鎺ユ敹鏁版嵁 */
#define RC522_CMD_RESETPHASE    0x0F    /**< 澶嶄綅 */
#define RC522_CMD_CALCCRC       0x03    /**< CRC 璁＄畻 */

/* ======================================================
 *  4. Mifare 鍗＄墖鍛戒护 (PICC - Proximity Integrated Circuit Card Commands)
 * ====================================================== */
#define RC522_PICC_REQIDL       0x26    /**< 瀵诲崱锛氬彧瀵诲?勪簬绌洪棽鐘舵€佺殑鍗? */
#define RC522_PICC_REQALL       0x52    /**< 瀵诲崱锛氬?绘墍鏈夊崱 */
#define RC522_PICC_ANTICOLL1    0x93    /**< 闃茬?版挒锛氫竴绾? */
#define RC522_PICC_ANTICOLL2    0x95    /**< 闃茬?版挒锛氫簩绾? */
#define RC522_PICC_AUTHENT1A    0x60    /**< 楠岃瘉 A 瀵嗛挜 */
#define RC522_PICC_AUTHENT1B    0x61    /**< 楠岃瘉 B 瀵嗛挜 */
#define RC522_PICC_READ         0x30    /**< 璇诲彇鍧? */
#define RC522_PICC_WRITE        0xA0    /**< 鍐欏叆鍧? */
#define RC522_PICC_DECREMENT    0xC0    /**< 鎵ｆ?? */
#define RC522_PICC_INCREMENT    0xC1    /**< 鍏呭€? */
#define RC522_PICC_RESTORE      0xC2    /**< 鎭㈠?嶆暟鎹?鍒扮紦鍐插尯 */
#define RC522_PICC_TRANSFER     0xB0    /**< 灏嗙紦鍐插尯鏁版嵁瀛樺叆 EEPROM */
#define RC522_PICC_HALT         0x50    /**< 浼戠湢 */

/* ======================================================
 *  5. MF522 瀵勫瓨鍣ㄥ湴鍧€ (Register Addresses)
 *     - PAGE 0 ~ PAGE 3
 * ====================================================== */
// ------ PAGE 0 ------
#define RC522_REG_RFU00             0x00    /**< 淇濈暀 */
#define RC522_REG_COMMAND           0x01    /**< 鍛戒护瀵勫瓨鍣? */
#define RC522_REG_COMIEN            0x02    /**< 涓?鏂?浣胯兘 */
#define RC522_REG_DIVLEN            0x03    /**< 鍒嗛?戜腑鏂?浣胯兘 */
#define RC522_REG_COMIRQ            0x04    /**< 涓?鏂?璇锋眰 */
#define RC522_REG_DIVIRQ            0x05    /**< 鍒嗛?戜腑鏂?璇锋眰 */
#define RC522_REG_ERROR             0x06    /**< 閿欒??鏍囧織 */
#define RC522_REG_STATUS1           0x07    /**< 鐘舵€佸瘎瀛樺櫒 1 */
#define RC522_REG_STATUS2           0x08    /**< 鐘舵€佸瘎瀛樺櫒 2 */
#define RC522_REG_FIFODATA          0x09    /**< FIFO 鏁版嵁瀵勫瓨鍣? */
#define RC522_REG_FIFOLEVEL         0x0A    /**< FIFO 鐢靛钩瀵勫瓨鍣? */
#define RC522_REG_WATERLEVEL        0x0B    /**< FIFO 姘寸嚎瀵勫瓨鍣? */
#define RC522_REG_CONTROL           0x0C    /**< 鎺у埗瀵勫瓨鍣? */
#define RC522_REG_BITFRAMING        0x0D    /**< 浣嶅抚璋冩暣瀵勫瓨鍣? */
#define RC522_REG_COLL              0x0E    /**< 纰版挒妫€娴嬪瘎瀛樺櫒 */
#define RC522_REG_RFU0F             0x0F    /**< 淇濈暀 */
// ------ PAGE 1 ------
#define RC522_REG_RFU10             0x10    /**< 淇濈暀 */
#define RC522_REG_MODE              0x11    /**< 妯″紡瀵勫瓨鍣? */
#define RC522_REG_TXMODE            0x12    /**< 鍙戦€佹ā寮? */
#define RC522_REG_RXMODE            0x13    /**< 鎺ユ敹妯″紡 */
#define RC522_REG_TXCONTROL         0x14    /**< 鍙戦€佹帶鍒? */
#define RC522_REG_TXAUTO            0x15    /**< 鍙戦€佽嚜鍔? */
#define RC522_REG_TXSEL             0x16    /**< 鍙戦€侀€氶亾閫夋嫨 */
#define RC522_REG_RXSEL             0x17    /**< 鎺ユ敹閫氶亾閫夋嫨 */
#define RC522_REG_RXTHRESHOLD       0x18    /**< 鎺ユ敹闃堝€? */
#define RC522_REG_DEMOD             0x19    /**< 瑙ｈ皟鎺у埗 */
#define RC522_REG_RFU1A             0x1A    /**< 淇濈暀 */
#define RC522_REG_RFU1B             0x1B    /**< 淇濈暀 */
#define RC522_REG_MIFARE            0x1C    /**< Mifare 涓撶敤瀵勫瓨鍣? */
#define RC522_REG_RFU1D             0x1D    /**< 淇濈暀 */
#define RC522_REG_RFU1E             0x1E    /**< 淇濈暀 */
#define RC522_REG_SERIALSPEED       0x1F    /**< 涓茶?岄€熷害 */
// ------ PAGE 2 ------
#define RC522_REG_RFU20             0x20    /**< 淇濈暀 */
#define RC522_REG_CRCRESULTM        0x21    /**< CRC 缁撴灉楂樹綅 */
#define RC522_REG_CRCRESULTL        0x22    /**< CRC 缁撴灉浣庝綅 */
#define RC522_REG_RFU23             0x23    /**< 淇濈暀 */
#define RC522_REG_MODWIDTH          0x24    /**< 璋冨埗瀹藉害 */
#define RC522_REG_RFCFG             0x26    /**< 灏勯?戦厤缃? */
#define RC522_REG_GSN               0x27    /**< 鍙戝皠鍣ㄥ?炵泭 */
#define RC522_REG_CWGSCFG           0x28    /**< CW 澧炵泭鎺у埗 */
#define RC522_REG_MODGSCFG          0x29    /**< 璋冨埗澧炵泭鎺у埗 */
#define RC522_REG_TMODE             0x2A    /**< 瀹氭椂鍣ㄦā寮? */
#define RC522_REG_TPRESCALER        0x2B    /**< 瀹氭椂鍣ㄩ?勫垎棰? */
#define RC522_REG_TRELOADH          0x2C    /**< 瀹氭椂鍣ㄩ噸杞藉€奸珮浣? */
#define RC522_REG_TRELOADL          0x2D    /**< 瀹氭椂鍣ㄩ噸杞藉€间綆浣? */
#define RC522_REG_TCOUNTERVALUEH    0x2E    /**< 瀹氭椂鍣ㄨ?℃暟鍊奸珮浣? */
#define RC522_REG_TCOUNTERVALUEL    0x2F    /**< 瀹氭椂鍣ㄨ?℃暟鍊间綆浣? */
// ------ PAGE 3 ------
#define RC522_REG_RFU30             0x30    /**< 淇濈暀 */
#define RC522_REG_TESTSEL1          0x31    /**< 娴嬭瘯閫夋嫨 1 */
#define RC522_REG_TESTSEL2          0x32    /**< 娴嬭瘯閫夋嫨 2 */
#define RC522_REG_TESTPINEN         0x33    /**< 娴嬭瘯寮曡剼浣胯兘 */
#define RC522_REG_TESTPINVALUE      0x34    /**< 娴嬭瘯寮曡剼鍊? */
#define RC522_REG_TESTBUS           0x35    /**< 娴嬭瘯鎬荤嚎 */
#define RC522_REG_AUTOTEST          0x36    /**< 鑷?鍔ㄦ祴璇? */
#define RC522_REG_VERSION           0x37    /**< 鐗堟湰鍙? */
#define RC522_REG_ANALOGTEST        0x38    /**< 妯℃嫙娴嬭瘯 */
#define RC522_REG_TESTDAC1          0x39    /**< DAC 娴嬭瘯 1 */
#define RC522_REG_TESTDAC2          0x3A    /**< DAC 娴嬭瘯 2 */
#define RC522_REG_TESTADC           0x3B    /**< ADC 娴嬭瘯 */
#define RC522_REG_RFU3C             0x3C    /**< 淇濈暀 */
#define RC522_REG_RFU3D             0x3D    /**< 淇濈暀 */
#define RC522_REG_RFU3E             0x3E    /**< 淇濈暀 */
#define RC522_REG_RFU3F             0x3F    /**< 淇濈暀 */

/** FIFO 鏈€澶ч暱搴? */
#define RC522_FIFO_MAX_LEN          64

/** Mifare 1K 鍗℃瘡鍧楀瓧鑺傛暟 */
#define RC522_BLOCK_SIZE            16

/** Mifare 1K 鍗℃墖鍖烘暟 */
#define RC522_SECTOR_COUNT          16

/** Mifare 1K 鍗℃瘡鎵囧尯鍧楁暟 */
#define RC522_BLOCKS_PER_SECTOR     4

/* ======================================================
 *  6. 椹卞姩 API (Application Programming Interface)
 * ====================================================== */

// ---------- 鍒濆?嬪寲涓庨厤缃? ----------

/**
 * @brief 鍒濆?嬪寲 RC522 鑺?鐗?
 * @param io 骞冲彴 IO 鎺ュ彛鎸囬拡锛屽繀椤绘寚鍚戞湁鏁堢殑 RC522_IO_t 缁撴瀯浣?
 * @note  璋冪敤姝ゅ嚱鏁板悗锛孯C522 瀹屾垚澶嶄綅骞惰繘鍏ュ垵濮嬬姸鎬併€?
 *        鐢ㄦ埛闇€纭?淇? io 缁撴瀯浣撲腑鐨勬墍鏈夊洖璋冨嚱鏁板凡姝ｇ‘瀹炵幇銆?
 */
void RC522_Init(RC522_IO_t *io);

/**
 * @brief 澶嶄綅 RC522 鑺?鐗?
 * @note  杞?澶嶄綅鑺?鐗囧唴閮ㄥ瘎瀛樺櫒鍒伴粯璁ゅ€?
 */
void RC522_Reset(void);

/**
 * @brief 閰嶇疆 ISO14443 A 绫婚€氳??绫诲瀷
 * @param type 绫诲瀷锛屽綋鍓嶄粎鏀?鎸? 'A'(ISO14443_A)
 */
void RC522_ConfigISOType(char type);

/**
 * @brief 寮€鍚?澶╃嚎
 * @note 姣忔?￠€氳??鍚庡ぉ绾夸細鑷?鍔ㄥ叧闂?锛岄渶瑕侀噸鏂板紑鍚?
 */
void RC522_AntennaOn(void);

/**
 * @brief 鍏抽棴澶╃嚎
 */
void RC522_AntennaOff(void);

// ---------- 鍗＄墖鍩烘湰鎿嶄綔 ----------

/**
 * @brief 瀵诲崱 (Request)
 * @param req_code 瀵诲崱妯″紡锛?
 *        - RC522_PICC_REQALL (0x52): 瀵绘墍鏈夊崱
 *        - RC522_PICC_REQIDL (0x26): 鍙?瀵绘湭浼戠湢鐨勫崱
 * @param pTagType 杈撳嚭鍙傛暟锛屽崱鐗囩被鍨嬬爜 (2 瀛楄妭)
 *        - 0x4400 = Mifare UltraLight
 *        - 0x0400 = Mifare One(S50)
 *        - 0x0200 = Mifare One(S70)
 *        - 0x0800 = Mifare Pro(X)
 *        - 0x4403 = Mifare DESFire
 * @return RC522_OK 鎴愬姛锛孯C522_ERR_NO_TAG 鏃犲崱锛孯C522_ERR 澶辫触
 */
char RC522_Request(uint8_t req_code, uint8_t *pTagType);

/**
 * @brief 闃茬?版挒 (Anticollision)
 * @param pSnr 杈撳嚭鍙傛暟锛屽崱鐗囧簭鍒楀彿 (4 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 * @note 褰撳?氬紶鍗″悓鏃跺湪鍦烘椂锛屾?ゅ嚱鏁伴€氳繃绠楁硶閫夊嚭涓€寮犲崱
 */
char RC522_Anticoll(uint8_t *pSnr);

/**
 * @brief 閫夊崱 (Select)
 * @param pSnr 鍗＄墖搴忓垪鍙? (4 瀛楄妭锛岀敱 RC522_Anticoll 鑾峰彇)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 * @note 閫変腑鍗″悗锛屽悗缁?鎿嶄綔鍙?閽堝?规?ゅ崱
 */
char RC522_Select(uint8_t *pSnr);

/**
 * @brief 楠岃瘉瀵嗙爜 (Authentication)
 * @param auth_mode 楠岃瘉妯″紡锛歊C522_PICC_AUTHENT1A (0x60) 鎴? RC522_PICC_AUTHENT1B (0x61)
 * @param addr     瑕侀獙璇佺殑鍧楀湴鍧€
 * @param pKey     瀵嗛挜 (6 瀛楄妭)锛岄粯璁ゅ嚭鍘傚瘑閽ヤ负 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}
 * @param pSnr     鍗＄墖搴忓垪鍙? (4 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 * @note 楠岃瘉閫氳繃鍚庢墠鑳借繘琛岃?诲啓鎿嶄綔銆傚瘑閽ュ瓨鍌ㄥ湪姣忎釜鎵囧尯鐨勭?? 3 鍧椼€?
 */
char RC522_AuthState(uint8_t auth_mode, uint8_t addr, uint8_t *pKey, uint8_t *pSnr);

/**
 * @brief 璇诲潡鏁版嵁 (Read)
 * @param addr  鍧楀湴鍧€ (Mifare 1K: 0~63)
 * @param pData 杈撳嚭鍙傛暟锛岃?诲彇鐨勬暟鎹? (16 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 */
char RC522_Read(uint8_t addr, uint8_t *pData);

/**
 * @brief 鍐欏潡鏁版嵁 (Write)
 * @param addr  鍧楀湴鍧€ (Mifare 1K: 0~63)
 * @param pData 瑕佸啓鍏ョ殑鏁版嵁 (16 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 * @note 鍧? 0 閫氬父涓嶅彲鍐欙紙瀛樻湁鍘傚晢鏁版嵁锛夛紝鎵囧尯绗? 3 鍧楀瓨鍌ㄥ瘑閽ラ渶璋ㄦ厧鎿嶄綔
 */
char RC522_Write(uint8_t addr, uint8_t *pData);

/**
 * @brief 浣垮崱杩涘叆浼戠湢鐘舵€? (Halt)
 * @return RC522_OK
 */
char RC522_Halt(void);

/**
 * @brief 绛夊緟鍗＄?诲紑灏勯?戝満
 * @note 闃诲?炵洿鍒版?€娴嬩笉鍒板崱
 */
void RC522_WaitCardOff(void);

// ---------- 楂樼骇鏁版嵁鎿嶄綔 ----------

/**
 * @brief 璇诲彇鎸囧畾鎵囧尯涓?鐨勬煇涓?鍧?
 * @param secNum 鎵囧尯鍙? (0~15)
 * @param blkNum 鍧楀彿 (0~3)
 * @param buf    杈撳嚭缂撳啿鍖? (鑷冲皯 16 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 */
char RC522_ReadBlock(uint8_t secNum, uint8_t blkNum, uint8_t buf[16]);

/**
 * @brief 鍐欏叆鎸囧畾鎵囧尯涓?鐨勬煇涓?鍧?
 * @param secNum 鎵囧尯鍙? (0~15)
 * @param blkNum 鍧楀彿 (0~3)锛屽潡0閫氬父涓嶅彲鍐?
 * @param buf    瑕佸啓鍏ョ殑鏁版嵁 (16 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 */
char RC522_WriteBlock(uint8_t secNum, uint8_t blkNum, uint8_t buf[16]);

/**
 * @brief 璇诲彇鏁翠釜鎵囧尯鏁版嵁
 * @param secNum 鎵囧尯鍙? (0~15)
 * @param buf    杈撳嚭缂撳啿鍖? (64 瀛楄妭 = 4鍧? x 16瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 */
char RC522_ReadSector(uint8_t secNum, uint8_t buf[64]);

/**
 * @brief 鍐欏叆鎵囧尯鏁版嵁锛堜粎鍓?3鍧楋紝绗?4鍧椾负瀵嗛挜鍖猴級
 * @param secNum 鎵囧尯鍙? (0~15)
 * @param buf    瑕佸啓鍏ョ殑鏁版嵁 (48 瀛楄妭 = 3鍧? x 16瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 */
char RC522_WriteSector(uint8_t secNum, uint8_t buf[48]);

/**
 * @brief 蹇?閫熷?诲崱锛氫竴娆″畬鎴? Request + Anticollision + Select
 * @param cn 杈撳嚭鍙傛暟锛屽崱鐗囧簭鍒楀彿 (4 瀛楄妭)
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 */
char RC522_ScanCard(uint8_t cn[4]);

/**
 * @brief 璇诲彇鏁村紶鍗＄墖鎵€鏈夋墖鍖烘暟鎹? (16鎵囧尯 x 64瀛楄妭)
 * @param data 杈撳嚭缂撳啿鍖? [64][16] 瀛楄妭
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 * @note 鑷?鍔ㄥ?勭悊瀵诲崱銆侀槻纰版挒銆侀€夊崱銆佸瘑鐮侀獙璇佺瓑娴佺▼
 */
char RC522_ReadAllSectors(uint8_t data[64][16]);

/**
 * @brief 鍐欏叆鏁村紶鍗＄墖鎵€鏈夋墖鍖烘暟鎹? (16鎵囧尯 x 48瀛楄妭)
 * @param data 瑕佸啓鍏ョ殑鏁版嵁 [64][16] 瀛楄妭
 * @return RC522_OK 鎴愬姛锛屽叾浠栧€煎け璐?
 * @note 鍙?鍐欏叆姣忎釜鎵囧尯鐨勫墠3鍧楋紝璺宠繃瀵嗛挜鍖?
 */
char RC522_WriteAllSectors(uint8_t data[64][16]);

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
void RC522_Platform_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* RC522_H */
