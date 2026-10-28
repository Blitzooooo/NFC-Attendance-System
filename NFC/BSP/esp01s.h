/**
 * @file    esp01s.h
 * @brief   ESP-01S WiFi妯″潡椹卞姩 - 鍩轰簬UartDrv閫氱敤涓插彛椹卞姩鐨勪笂灞傚簲鐢ㄩ┍鍔?
 * @version 1.0
 *
 * @par 鏋舵瀯姒傝堪
 *   鏈?椹卞姩閲囩敤"鍗曚緥瀹炰緥 + 鐘舵€佹満"鏋舵瀯,灏佽?匛SP-01S妯″潡鐨勫畬鏁村伐浣滄祦绋?:
 *   - 椹卞姩鍐呴儴缁存姢鍞?涓€鐨勫崟渚嬪疄渚? s_esp01s,鐢ㄦ埛鏃犻渶瀹氫箟ESP01S_t鍙橀噺
 *   - 閫氳繃鐘舵€佹満(ESP01S_State_t)绠＄悊杩炴帴鐢熷懡鍛ㄦ湡: IDLE鈫扐T_OK鈫扺iFi鈫扵CP鈫掗€忎紶
 *   - 涓婂眰渚濊禆UartDrv涓插彛椹卞姩,閫氳繃鍥炶皟鏈哄埗鎺ユ敹AT鍝嶅簲鍜屾暟鎹?
 *   - 鏀?鎸丯TP缃戠粶鎺堟椂(AT SNTP / UDP NTP鍙屾ā寮忚嚜鍔ㄥ洖閫€)
 *   - 鏀?鎸丷TC鏍℃椂鍜屾椂闂村瓧绗︿覆鏍煎紡鍖栬緭鍑?
 *
 * @par 杩炴帴娴佺▼
 *   ESP01S_Start() 瀹屾垚浠ヤ笅姝ラ??:
 *   1. 閫€鍑洪€忎紶妯″紡(闃叉??涓婃?￠仐鐣?)
 *   2. AT閫氫俊娴嬭瘯
 *   3. 鍏抽棴鍥炴樉鍜屾畫鐣欒繛鎺?
 *   4. 閰嶇疆鍗曡繛鎺ユā寮?(CIPMUX=0,閫忎紶鍓嶆彁)
 *   5. 鏌ヨ?㈡ā鍧椾俊鎭?鍜屽綋鍓嶈繛鎺ョ姸鎬?
 *   6. 杩炴帴WiFi鐑?鐐?
 *   7. NTP鎺堟椂(鍙?閫?)
 *   8. 杩炴帴TCP鏈嶅姟鍣?
 *   9. 寮€鍚?閫忎紶妯″紡
 *
 * @par 浣跨敤鏂规硶
 *   1. 璋冪敤 UartDrv_Init() 鍒濆?嬪寲搴曞眰涓插彛椹卞姩
 *   2. 璋冪敤 ESP01S_Init() 鍒濆?嬪寲ESP01S椹卞姩(浣跨敤榛樿?ら厤缃?)
 *   3. (鍙?閫?) 璋冪敤 ESP01S_SetWiFi/SetTcpServer/SetNtpServer 淇?鏀归厤缃?
 *   4. 璋冪敤 ESP01S_RegisterDataCb() 娉ㄥ唽鏁版嵁鎺ユ敹鍥炶皟(鍙?閫?)
 *   5. 璋冪敤 ESP01S_Start() 鍚?鍔╓iFi杩炴帴鍜孴CP杩炴帴
 *   6. 閫忎紶妯″紡涓嬭皟鐢? ESP01S_SendStr()/ESP01S_SendData() 鍙戦€佹暟鎹?
 *   7. 璋冪敤 ESP01S_GetDateTime() 鑾峰彇褰撳墠鏃堕棿瀛楃?︿覆
 *   8. 璋冪敤 ESP01S_SetRtcFromNtp() 灏哊TP鏃堕棿鍐欏叆RTC
 *
 * @par 閰嶇疆鏂瑰紡
 *   - 鏂瑰紡1: 鐩存帴淇?鏀? esp01s.c 涓?鐨? s_defaultConfig 榛樿?ゅ€?(鏈€绠€鍗?)
 *   - 鏂瑰紡2: 璋冪敤 ESP01S_SetConfig() 涓€娆℃€ц?嗙洊鍏ㄩ儴閰嶇疆
 *   - 鏂瑰紡3: 璋冪敤 ESP01S_SetWiFi/SetTcpServer/SetNtpServer 鍒嗛」淇?鏀?
 *   娉ㄦ剰: 淇?鏀归厤缃?鍚庨渶閲嶆柊璋冪敤 ESP01S_Start() 鎵嶄細鐢熸晥
 *
 * @par NTP鎺堟椂璇存槑
 *   鏀?鎸佷袱绉峃TP鎺堟椂鏂瑰紡(鑷?鍔ㄥ洖閫€):
 *   - 鏂瑰紡1: AT鎸囦护SNTP (AT+CIPSNTPCFG + AT+CIPSNTPTIME),闇€鍥轰欢v1.7.0+
 *   - 鏂瑰紡2: UDP NTP鍗忚??,閫氳繃UDP鐩磋繛NTP鏈嶅姟鍣ㄥ彂閫?/瑙ｆ瀽NTP鎶ユ枃,閫氱敤鏂规??
 *   濡傛灉config.ntpServer涓虹┖瀛楃?︿覆,鍒欒烦杩嘚TP鎺堟椂
 *
 * @par 閫忎紶妯″紡娉ㄦ剰浜嬮」
 *   - 閫忎紶妯″紡涓嬪彧鑳介€氳繃 ESP01S_SendStr/SendData 鍙戦€佸師濮嬫暟鎹?
 *   - 閫€鍑洪€忎紶妯″紡闇€璋冪敤 ESP01S_ExitTransparent(),鍐呴儴鍙戦€?"+++"鍛戒护
 *   - "+++"鍓嶅悗闇€鍚?500ms闈欓粯,涓嶅姞鎹㈣?岀??(ESP8266閫忎紶閫€鍑哄崗璁?瑕佹眰)
 *   - 閫€鍑哄悗鍙?閲嶆柊鍙戦€丄T鎸囦护鎴栦慨鏀归厤缃?鍚庨噸鏂癝tart
 */

#ifndef __ESP01S_H
#define __ESP01S_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "uart_drv.h"

#ifndef ESP01S_NO_FREERTOS
#include "cmsis_os.h"
#endif

/* ====== 瀵煎嚭绫诲瀷瀹氫箟 ====== */

/**
 * @brief  ESP01S杩炴帴鐘舵€佹灇涓?
 * @note   鐘舵€佽浆绉婚『搴?(姝ｅ父娴佺▼):
 *         IDLE 鈫? AT_OK 鈫? WIFI_CONNECTING 鈫? WIFI_CONNECTED
 *              鈫? TCP_CONNECTING 鈫? TCP_CONNECTED 鈫? TRANSPARENT
 *
 *         鐘舵€佸垽鏂?閫昏緫:
 *         - ESP01S_Start()涓?閫氳繃 "state < ESP01S_STATE_xxx" 鍒ゆ柇鏄?鍚﹁揪鍒版煇闃舵??
 *         - 鍥炶皟涓?鏍规嵁AT鍝嶅簲瀛楃?︿覆鎺ㄥ姩鐘舵€佸墠杩?
 *         - 浠讳綍闃舵?靛け璐ュ垯鍋滅暀鍦ㄥ綋鍓嶇姸鎬?,Start()杩斿洖瀵瑰簲閿欒??鐮?
 */
typedef enum {
    ESP01S_STATE_IDLE = 0,          /**< 绌洪棽,鏈?鍒濆?嬪寲鎴栧垰鍒濆?嬪寲 */
    ESP01S_STATE_AT_OK,             /**< AT閫氫俊姝ｅ父(妯″潡鍝嶅簲AT鎸囦护) */
    ESP01S_STATE_WIFI_CONNECTING,   /**< 姝ｅ湪杩炴帴WiFi(AT+CWJAP宸插彂閫?) */
    ESP01S_STATE_WIFI_CONNECTED,    /**< WiFi宸茶繛鎺?(鏀跺埌WIFI CONNECTED+OK) */
    ESP01S_STATE_TCP_CONNECTING,    /**< 姝ｅ湪杩炴帴TCP鏈嶅姟鍣?(AT+CIPSTART宸插彂閫?) */
    ESP01S_STATE_TCP_CONNECTED,     /**< TCP宸茶繛鎺?(鏀跺埌CONNECT+OK) */
    ESP01S_STATE_TRANSPARENT,       /**< 閫忎紶妯″紡(AT+CIPSEND宸插彂閫佸苟鎴愬姛) */
} ESP01S_State_t;

/**
 * @brief  ESP01S閰嶇疆缁撴瀯
 * @note   閰嶇疆鏂规硶:
 *         - 鐩存帴淇?鏀? esp01s.c 涓?鐨? s_defaultConfig
 *         - 璋冪敤 ESP01S_SetConfig() / ESP01S_SetWiFi() / ESP01S_SetTcpServer() / ESP01S_SetNtpServer()
 *         淇?鏀瑰悗闇€璋冪敤 ESP01S_Start() 閲嶆柊杩炴帴鎵嶇敓鏁?
 */
typedef struct {
    char     ssid[32];              /**< WiFi鐑?鐐瑰悕绉?(SSID),鏈€闀?31瀛楃?? */
    char     password[64];          /**< WiFi瀵嗙爜,鏈€闀?63瀛楃?? */
    char     tcpServerIP[48];       /**< TCP鏈嶅姟鍣↖P鍦板潃(鐐瑰垎鍗佽繘鍒舵垨鍩熷悕) */
    uint16_t tcpPort;               /**< TCP鏈嶅姟鍣ㄧ??鍙ｅ彿(1~65535) */
    char     ntpServer[48];         /**< NTP鏈嶅姟鍣ㄥ煙鍚?(濡?"ntp.aliyun.com"),绌哄瓧绗︿覆鍒欎笉鍚?鐢∟TP */
    int8_t   ntpTimezone;           /**< 鏃跺尯灏忔椂鏁?(涓?鍥戒笢鍏?鍖?=8),鑼冨洿-12~14 */
} ESP01S_Config_t;

/**
 * @brief  NTP鏃堕棿缁撴瀯
 * @note   鏃堕棿涓烘湰鍦版椂闂?(宸插惈鏃跺尯鍋忕Щ),鐢盢TP鍝嶅簲鎴朥DP NTP鎶ユ枃瑙ｆ瀽寰楀埌銆?
 *         閫氳繃 ESP01S_GetNtpTime() 鑾峰彇,鎴栫敱 ESP01S_GetDateTime() 闂存帴浣跨敤銆?
 */
typedef struct {
    uint16_t year;      /**< 骞? (2024~) */
    uint8_t  month;     /**< 鏈? (1~12) */
    uint8_t  day;       /**< 鏃? (1~31) */
    uint8_t  hour;      /**< 鏃? (0~23, 宸插惈鏃跺尯鍋忕Щ) */
    uint8_t  minute;    /**< 鍒? (0~59) */
    uint8_t  second;    /**< 绉? (0~59) */
    uint8_t  weekday;   /**< 鏄熸湡 (1=Mon, 2=Tue, ..., 7=Sun) */
} ESP01S_NtpTime_t;

/**
 * @brief  鏃ユ湡鏃堕棿鏍煎紡鏋氫妇
 * @note   鐢ㄤ簬 ESP01S_GetDateTime() 鐨? format 鍙傛暟,鎺у埗杈撳嚭鏍煎紡
 */
typedef enum {
    DT_DATE = 0,        /**< 浠呮棩鏈?: "2026-04-12" */
    DT_TIME,            /**< 浠呮椂闂?: "14:30:45" */
    DT_ALL,             /**< 鏃ユ湡+鏃堕棿: "2026-04-12 14:30:45" */
} DT_Format_t;

/**
 * @brief  ESP01S閫忎紶鏁版嵁鎺ユ敹鍥炶皟鍑芥暟绫诲瀷
 * @param  pData      鎺ユ敹鍒扮殑鍘熷?嬫暟鎹?鎸囬拡(閫忎紶妯″紡涓婽CP鏈嶅姟鍣ㄥ彂鏉ョ殑鏁版嵁)
 * @param  len        鎺ユ敹鏁版嵁闀垮害
 * @param  pUserCtx   鐢ㄦ埛涓婁笅鏂囨寚閽?(娉ㄥ唽鏃朵紶鍏?,鍥炶皟鏃跺師鏍蜂紶鍥?)
 * @note   鍥炶皟鍦ㄤ腑鏂?涓婁笅鏂囦腑鎵ц??,搴斿敖蹇?杩斿洖
 */
typedef void (*ESP01S_DataCallback_t)(const uint8_t *pData, uint16_t len, void *pUserCtx);

/* ====== 瀵煎嚭鍑芥暟澹版槑 ====== */

/* ---- 鍒濆?嬪寲涓庡惎鍋? ---- */

/**
 * @brief  鍒濆?嬪寲ESP01S椹卞姩(浣跨敤esp01s.c涓?鐨勯粯璁ら厤缃?)
 * @param  pUartDrv 搴曞眰涓插彛椹卞姩鎸囬拡(闇€鍏堝垵濮嬪寲,濡? UartDrv_Init(&g_uart4Drv, &huart4))
 * @note   鍒濆?嬪寲娴佺▼:
 *         1. 娓呴浂鍐呴儴鍗曚緥瀹炰緥
 *         2. 缁戝畾涓插彛椹卞姩
 *         3. 鎷疯礉榛樿?ら厤缃?鍒板疄渚?
 *         4. 娉ㄥ唽涓插彛鎺ユ敹鍥炶皟(ESP01S_RxCallback)
 *         璋冪敤鍚庣姸鎬佷负 ESP01S_STATE_IDLE,闇€璋冪敤 ESP01S_Start() 寮€濮嬭繛鎺?
 */
void ESP01S_Init(UartDrv_t *pUartDrv);

/**
 * @brief  鍚?鍔‥SP01S瀹屾暣娴佺▼: AT娴嬭瘯 鈫? 杩濿iFi 鈫? NTP鎺堟椂 鈫? 杩濼CP 鈫? 閫忎紶
 * @retval 0:鎴愬姛  -1:AT閫氫俊澶辫触  -2:WiFi杩炴帴澶辫触  -3:TCP杩炴帴澶辫触
 * @note   鏈?鍑芥暟涓洪樆濉炲紡,鍐呴儴閫氳繃HAL_Delay绛夊緟AT鍝嶅簲,鏁翠釜娴佺▼绾﹂渶10~20绉掋€?
 *         璋冪敤鍓嶉渶纭?淇?:
 *         - ESP01S_Init() 宸茶皟鐢?
 *         - 閰嶇疆鍙傛暟(SSID/瀵嗙爜/TCP鏈嶅姟鍣?)宸叉?ｇ‘璁剧疆
 *         - 搴曞眰涓插彛宸插垵濮嬪寲骞跺惎鍔ㄦ帴鏀?
 *         鎴愬姛鍚庤繘鍏ラ€忎紶妯″紡,鍙?鐩存帴浣跨敤 SendStr/SendData 鍙戦€佹暟鎹?
 */
int ESP01S_Start(void);

/**
 * @brief  閫€鍑洪€忎紶妯″紡
 * @note   鍐呴儴鍙戦€?"+++"鍛戒护閫€鍑洪€忎紶,鍓嶅悗鍚勭瓑寰?500ms闈欓粯(ESP8266鍗忚??瑕佹眰):
 *         - 鍓嶅??500ms: 纭?淇濅箣鍓嶇殑鏁版嵁鍙戦€佸凡瀹屾垚
 *         - 鍙戦€?"+++": 涓嶅姞鎹㈣?岀??(鍚﹀垯浼氳??褰撲綔鏁版嵁鍙戦€?)
 *         - 鍚庣画500ms: 绛夊緟妯″潡璇嗗埆閫€鍑哄懡浠?
 *         閫€鍑哄悗鐘舵€佸洖閫€鍒? ESP01S_STATE_TCP_CONNECTED,鍙?閲嶆柊鍙慉T鎸囦护
 *         鎴栦慨鏀归厤缃?鍚庤皟鐢? ESP01S_Start() 閲嶆柊杩炴帴
 */
void ESP01S_ExitTransparent(void);

/* ---- 閰嶇疆API ---- */

/**
 * @brief  涓€娆℃€ц?剧疆鍏ㄩ儴閰嶇疆鍙傛暟
 * @param  pConfig 閰嶇疆鍙傛暟鎸囬拡(鍐呭?逛細琚?鎷疯礉,璋冪敤鍚庡彲閲婃斁)
 * @note   淇?鏀归厤缃?鍚庨渶璋冪敤ESP01S_Start()閲嶆柊杩炴帴鎵嶇敓鏁?
 *         浣跨敤绀轰緥:
 *         @code
 *         ESP01S_Config_t cfg = {
 *             .ssid = "MyWiFi", .password = "12345678",
 *             .tcpServerIP = "192.168.1.100", .tcpPort = 8080,
 *             .ntpServer = "ntp.aliyun.com", .ntpTimezone = 8
 *         };
 *         ESP01S_SetConfig(&cfg);
 *         @endcode
 */
void ESP01S_SetConfig(const ESP01S_Config_t *pConfig);

/**
 * @brief  鑾峰彇褰撳墠閰嶇疆(鍙?璇?)
 * @retval 褰撳墠閰嶇疆鎸囬拡,鎸囧悜椹卞姩鍐呴儴閰嶇疆(涓嶈?佷慨鏀硅繑鍥炵殑鎸囬拡鍐呭??)
 * @note   杩斿洖鐨勬槸鍐呴儴閰嶇疆鐨勫湴鍧€,涓嶈?佺洿鎺ヤ慨鏀硅繑鍥炴寚閽堢殑鍐呭??,
 *         搴斾娇鐢? ESP01S_SetConfig/SetWiFi/SetTcpServer/SetNtpServer 鏉ヤ慨鏀?
 */
const ESP01S_Config_t *ESP01S_GetConfig(void);

/**
 * @brief  璁剧疆WiFi鐑?鐐瑰弬鏁?
 * @param  ssid     WiFi鐑?鐐瑰悕绉?(鏈€闀?31瀛楃??,瓒呭嚭鎴?鏂?)
 * @param  password WiFi瀵嗙爜(鏈€闀?63瀛楃??,瓒呭嚭鎴?鏂?)
 * @note   淇?鏀瑰悗闇€璋冪敤ESP01S_Start()閲嶆柊杩炴帴
 */
void ESP01S_SetWiFi(const char *ssid, const char *password);

/**
 * @brief  璁剧疆TCP鏈嶅姟鍣ㄥ弬鏁?
 * @param  ip   TCP鏈嶅姟鍣↖P鍦板潃(鐐瑰垎鍗佽繘鍒?,濡?"192.168.1.100")
 * @param  port TCP鏈嶅姟鍣ㄧ??鍙ｅ彿(1~65535)
 * @note   淇?鏀瑰悗闇€璋冪敤ESP01S_Start()閲嶆柊杩炴帴
 */
void ESP01S_SetTcpServer(const char *ip, uint16_t port);

/**
 * @brief  璁剧疆NTP鎺堟椂鏈嶅姟鍣ㄥ弬鏁?
 * @param  server    NTP鏈嶅姟鍣ㄥ煙鍚?(濡?"ntp.aliyun.com"),浼?""鎴朜ULL鍒欑?佺敤NTP鎺堟椂
 * @param  timezone  鏃跺尯灏忔椂鏁?(涓?鍥戒笢鍏?鍖?=8),鑼冨洿-12~14
 * @note   淇?鏀瑰悗闇€璋冪敤ESP01S_Start()閲嶆柊杩炴帴
 */
void ESP01S_SetNtpServer(const char *server, int8_t timezone);

/* ---- 鏁版嵁鏀跺彂 ---- */

/**
 * @brief  鍙戦€丄T鎸囦护骞剁瓑寰?
 * @param  cmd     AT鎸囦护瀛楃?︿覆(闇€鍖呭惈\r\n缁撳熬,濡?"AT\r\n")
 * @param  waitMs  鍙戦€佸悗绛夊緟鏃堕棿(姣?绉?),鐢ㄤ簬绛夊緟妯″潡鍝嶅簲,0鍒欎笉绛?
 * @note   鏈?鍑芥暟涓洪樆濉炲紡,鍐呴儴璋冪敤 UartDrv_SendStr 鍙戦€? + HAL_Delay 绛夊緟
 *         閫忎紶妯″紡涓嬭?峰嬁璋冪敤姝ゅ嚱鏁?(鏁版嵁浼氳??褰撲綔閫忎紶鏁版嵁鍙戦€?)
 */
void ESP01S_SendATCmd(const char *cmd, uint32_t waitMs);

/**
 * @brief  鍙戦€佸瓧绗︿覆(閫忎紶妯″紡)
 * @param  str 浠?'\0'缁撳熬鐨勫瓧绗︿覆
 * @note   浠呭湪閫忎紶妯″紡涓嬩娇鐢?,鏁版嵁灏嗙洿鎺ュ彂閫佸埌TCP鏈嶅姟鍣?
 */
void ESP01S_SendStr(const char *str);

/**
 * @brief  鍙戦€佹暟鎹?(閫忎紶妯″紡)
 * @param  pData 鏁版嵁鎸囬拡
 * @param  len   鏁版嵁闀垮害(瀛楄妭)
 * @note   浠呭湪閫忎紶妯″紡涓嬩娇鐢?,鍙?鍙戦€佷簩杩涘埗鏁版嵁(鍚玕0鐨勫瓧鑺?)
 */
void ESP01S_SendData(const uint8_t *pData, uint16_t len);

/**
 * @brief  娉ㄥ唽閫忎紶鏁版嵁鎺ユ敹鍥炶皟
 * @param  pCb       鍥炶皟鍑芥暟鎸囬拡(鍑芥暟鍘熷瀷瑙? ESP01S_DataCallback_t)
 * @param  pUserCtx  鐢ㄦ埛涓婁笅鏂囨寚閽?,鍥炶皟鏃跺師鏍蜂紶鍥?,鍙?浼燦ULL
 * @note   閫忎紶妯″紡涓嬫敹鍒癟CP鏈嶅姟鍣ㄦ暟鎹?鏃惰Е鍙戝洖璋?
 *         浣跨敤绀轰緥:
 *         @code
 *         void MyDataCb(const uint8_t *pData, uint16_t len, void *pCtx) {
 *             // 澶勭悊鎺ユ敹鍒扮殑TCP鏁版嵁
 *         }
 *         ESP01S_RegisterDataCb(MyDataCb, NULL);
 *         @endcode
 */
void ESP01S_RegisterDataCb(ESP01S_DataCallback_t pCb, void *pUserCtx);

/* ---- 鐘舵€佹煡璇? ---- */

/**
 * @brief  鑾峰彇褰撳墠杩炴帴鐘舵€?
 * @retval 褰撳墠鐘舵€?(瑙? ESP01S_State_t 鏋氫妇瀹氫箟)
 * @note   鍙?鐢ㄤ簬鍒ゆ柇妯″潡鏄?鍚﹀氨缁?銆佹槸鍚﹀湪閫忎紶妯″紡绛?
 */
ESP01S_State_t ESP01S_GetState(void);

/* ---- NTP鎺堟椂 ---- */

/**
 * @brief  閰嶇疆SNTP鏈嶅姟鍣ㄥ苟鑾峰彇NTP缃戠粶鏃堕棿
 * @note   闇€鍦╓iFi宸茶繛鎺ャ€佹湭杩涘叆閫忎紶妯″紡鏃惰皟鐢ㄣ€?
 *         ESP01S_Start()涓?宸茶嚜鍔ㄨ皟鐢ㄦ?ゅ嚱鏁?,涓€鑸?鏃犻渶鎵嬪姩璋冪敤銆?
 *         宸ヤ綔娴佺▼:
 *         1. 灏濊瘯AT鎸囦护SNTP (AT+CIPSNTPCFG + AT+CIPSNTPTIME)
 *         2. 鑻?AT SNTP澶辫触(鍥轰欢涓嶆敮鎸?),鍥為€€鍒癠DP NTP鍗忚??
 *         3. UDP鏂瑰紡: 鍏抽棴宸叉湁杩炴帴鈫掑缓UDP鈫掑彂NTP璇锋眰鈫掕В鏋愬搷搴斺啋鍏砋DP
 *         濡傛灉config.ntpServer涓虹┖瀛楃?︿覆鍒欒烦杩?
 */
void ESP01S_SyncNtpTime(void);

/**
 * @brief  鑾峰彇鏈€杩戜竴娆?NTP鎺堟椂缁撴灉
 * @param  pTime 杈撳嚭鏃堕棿缁撴瀯鎸囬拡(鐢辫皟鐢ㄨ€呭垎閰?)
 * @retval 0:鏃堕棿鏈夋晥  -1:鏃堕棿鏃犳晥(鏈?鑾峰彇鎴栬幏鍙栧け璐?)
 * @note   杩斿洖鐨勬槸NTP鎺堟椂鏃剁殑鐬?闂存椂闂?,涓嶆槸褰撳墠鏃堕棿銆?
 *         濡傞渶鑾峰彇褰撳墠鏃堕棿璇蜂娇鐢? ESP01S_GetDateTime()
 */
int ESP01S_GetNtpTime(ESP01S_NtpTime_t *pTime);

/**
 * @brief  妫€鏌?NTP鏃堕棿鏄?鍚﹀凡鍚屾?ユ垚鍔?
 * @retval 1:宸插悓姝?  0:鏈?鍚屾??
 */
uint8_t ESP01S_IsNtpSynced(void);

/**
 * @brief  妫€鏌ュ綋鍓嶆槸鍚﹀凡杩炴帴WiFi(鍗矱SP01S鐘舵€佹満宸插埌杈網IFI_CONNECTED鍙婁互涓?)
 * @retval 1:宸茶繛鎺?  0:鏈?杩炴帴
 */
uint8_t ESP01S_IsWiFiConnected(void);

/**
 * @brief  灏哊TP鏃堕棿鍐欏叆STM32鐨凴TC(BCD鏍煎紡)
 * @param  pRtc STM32 RTC鍙ユ焺鎸囬拡,浼燦ULL鍒欒烦杩嘡TC鍐欏叆
 * @retval 0:鎴愬姛  -1:NTP鏃堕棿鏃犳晥  -2:RTC鍐欏叆澶辫触
 * @note   璋冪敤鍓嶉渶纭?淇漀TP鏃堕棿宸叉湁鏁?(鍏堣皟鐢‥SP01S_Start鎴朎SP01S_SyncNtpTime)
 *         鍐呴儴浼氳ˉ鍋夸粠NTP鎺堟椂鍒板綋鍓嶇殑鏃堕棿宸?(閫氳繃HAL_Tick璁＄畻),
 *         娑堥櫎鍚庣画鎿嶄綔(TCP杩炴帴绛?)甯︽潵鐨勫欢杩熻??宸?銆?
 *         鍐欏叆鍚庡彲閫氳繃 HAL_RTC_GetTime/GetDate 璇诲彇RTC鏃堕棿
 */
int ESP01S_SetRtcFromNtp(RTC_HandleTypeDef *pRtc);

/**
 * @brief  鏌ヨ?㈠績鐭ュぉ姘旀棩鎶ュ苟瑙ｆ瀽鍑哄煄甯傘€佺櫧澶?/澶滈棿澶╂皵銆佹俯搴﹀強闄嶉洦閲?
 * @param  apiKey          蹇冪煡澶╂皵API Key
 * @param  location        鍩庡競,濡? "hangzhou"
 * @param  language        璇?瑷€,濡? "zh-Hans"
 * @param  unit            娓╁害鍗曚綅,"c"鎴?"f"
 * @param  outCity         杈撳嚭鍩庡競鍚嶇紦鍐插尯
 * @param  cityBufSize     outCity缂撳啿鍖哄ぇ灏?(寤鸿??>=16)
 * @param  outTextDay      杈撳嚭鐧藉ぉ澶╂皵鐜拌薄鏂囧瓧缂撳啿鍖?
 * @param  textDayBufSize  outTextDay缂撳啿鍖哄ぇ灏?(寤鸿??>=32)
 * @param  outHigh         杈撳嚭鐧藉ぉ鏈€楂樻俯搴﹀瓧绗︿覆缂撳啿鍖?
 * @param  highBufSize     outHigh缂撳啿鍖哄ぇ灏?(寤鸿??>=8)
 * @param  outTextNight    杈撳嚭澶滈棿澶╂皵鐜拌薄鏂囧瓧缂撳啿鍖?
 * @param  textNightBufSize outTextNight缂撳啿鍖哄ぇ灏?(寤鸿??>=32)
 * @param  outLow          杈撳嚭澶滈棿鏈€浣庢俯搴﹀瓧绗︿覆缂撳啿鍖?
 * @param  lowBufSize      outLow缂撳啿鍖哄ぇ灏?(寤鸿??>=8)
 * @param  outPrecip       杈撳嚭闄嶉洦姒傜巼瀛楃?︿覆缂撳啿鍖?
 * @param  precipBufSize   outPrecip缂撳啿鍖哄ぇ灏?(寤鸿??>=8)
 * @retval 0:鎴愬姛  -1:鍙傛暟閿欒??  -2:鏈?杩炴帴WiFi  -3:缃戠粶璇锋眰澶辫触  -4:瑙ｆ瀽澶辫触
 * @note   鏈?鍑芥暟浼氫复鏃堕€€鍑洪€忎紶妯″紡銆佸叧闂?褰撳墠TCP杩炴帴,鏌ヨ?㈢粨鏉熷悗鑷?鍔ㄦ仮澶?
 *         鍒伴厤缃?鐨勪腑缁?TCP鏈嶅姟鍣ㄥ苟閲嶆柊杩涘叆閫忎紶妯″紡(鑻ラ厤缃?浜員CP鏈嶅姟鍣?)銆?
 *         浣跨敤 API: /v3/weather/daily.json?start=0&days=1
 */
int ESP01S_QueryWeather(const char *apiKey, const char *location,
                        const char *language, const char *unit,
                        char *outCity, uint16_t cityBufSize,
                        char *outTextDay, uint16_t textDayBufSize,
                        char *outHigh, uint16_t highBufSize,
                        char *outTextNight, uint16_t textNightBufSize,
                        char *outLow, uint16_t lowBufSize,
                        char *outPrecip, uint16_t precipBufSize);

/* ---- 鏃堕棿鑾峰彇 ---- */

/**
 * @brief  鑾峰彇褰撳墠鏃堕棿瀛楃?︿覆
 * @param  pRtc    STM32 RTC鍙ユ焺鎸囬拡,浼燦ULL鍒欎笉浠嶳TC璇诲彇
 * @param  format  鏃堕棿鏍煎紡: DT_DATE/DT_TIME/DT_ALL
 * @param  pBuf    杈撳嚭瀛楃?︿覆缂撳啿鍖?(鐢辫皟鐢ㄨ€呭垎閰?)
 * @param  bufLen  缂撳啿鍖洪暱搴?(寤鸿??>=24瀛楄妭)
 * @retval 0:鎴愬姛  -1:鏃犲彲鐢ㄦ椂闂存簮(NTP鏃犳晥涓擱TC鏃犳晥鎴栨湭浼?)
 * @note   鏃堕棿鏉ユ簮浼樺厛绾?:
 *         1. RTC(濡傛灉pRtc闈濶ULL涓擱TC骞翠唤>2000) 鈥? 鏈€绮剧‘,鎺夌數鍙?淇濇寔
 *         2. NTP鏃堕棿+HAL_Tick宸?鍊兼帹绠?(濡傛灉NTP宸叉巿鏃?) 鈥? 鏈夌疮绉?璇?宸?
 *         鏍煎紡绀轰緥:
 *         DT_DATE: "2026-04-12"
 *         DT_TIME: "14:30:45"
 *         DT_ALL:  "2026-04-12 14:30:45"
 *         浣跨敤绀轰緥:
 *         @code
 *         char buf[24];
 *         if (ESP01S_GetDateTime(&hrtc, DT_ALL, buf, sizeof(buf)) == 0)
 *             printf("褰撳墠鏃堕棿: %s\r\n", buf);
 *         @endcode
 */
int ESP01S_GetDateTime(RTC_HandleTypeDef *pRtc, DT_Format_t format, char *pBuf, uint16_t bufLen);

/* ---- 天气查询 (wttr.in 免费 API, 无需 Key) ---- */

/**
 * @brief  通过 wttr.in 查询天气 (免费, 无需 API Key)
 * @param  location    城市名 (拼音或英文, 如 "hangzhou")
 * @param  outLine     输出格式化行缓冲 (如 "Hangzhou Sunny 30C 10%")
 * @param  lineBufSize 缓冲大小 (建议 >= 128)
 * @retval 0:成功  -1:参数错  -2:WiFi未连  -3:网络失败  -4:解析失败
 * @note   使用 HTTP GET http://wttr.in/{location}?format="%l|%C|%t|%p"
 *         响应示例: "Hangzhou|Sunny|+28°C|15%"
 *         内部会临时退出透传、关闭TCP、查询后恢复TCP透传。
 */
int ESP01S_QueryWeatherWttr(const char *location,
                            char *outLine, uint16_t lineBufSize);

#ifdef __cplusplus
}
#endif

#ifndef ESP01S_NO_FREERTOS
/**
 * @brief  娉ㄥ唽ESP01S閫忎紶鏁版嵁鎺ユ敹闃熷垪
 * @param  queue CMSIS-RTOS2 娑堟伅闃熷垪鍙ユ焺
 * @note   娉ㄥ唽闃熷垪鍚?,閫忎紶妯″紡涓嬫帴鏀跺埌鐨凾CP鏁版嵁灏嗙洿鎺ュ彂閫佸埌璇ラ槦鍒?,
 *         涓嶅啀閫氳繃 ESP01S_RegisterDataCb 娉ㄥ唽鐨勫洖璋冨?勭悊銆?
 *         AT鎸囦护妯″紡鐨勫搷搴斾粛鐢卞唴閮ㄥ洖璋冨湪ISR涓?澶勭悊銆?
 */
void ESP01S_RegisterRxQueue(osMessageQueueId_t queue);
#endif

/**
 * @brief  鍦ㄤ换鍔′笂涓嬫枃涓?鍒嗗彂閫忎紶鏁版嵁鍒扮敤鎴峰洖璋?
 * @param  pData 鎺ユ敹鍒扮殑TCP鏁版嵁鎸囬拡
 * @param  len   鏁版嵁闀垮害
 * @note   褰撲娇鐢ㄦ秷鎭?闃熷垪鎺ユ敹閫忎紶鏁版嵁鏃?,搴斿湪 NetworkTask 涓?璇诲彇闃熷垪鍚庤皟鐢ㄦ湰鍑芥暟,
 *         鎴栬嚜琛屽?勭悊鏁版嵁銆傛湰鍑芥暟浼氳皟鐢? ESP01S_RegisterDataCb 娉ㄥ唽鐨勭敤鎴峰洖璋冦€?
 */
void ESP01S_DispatchTransparentData(const uint8_t *pData, uint16_t len);
#endif /* __ESP01S_H */

