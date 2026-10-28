/**
 * @file    ssd1306.h
 * @brief   SSD1306/SH1106 OLED鏄剧ず椹卞姩澶存枃浠?
 * @details 鏀?鎸丼SD1306鍜孲H1106涓ょ?峅LED鎺у埗鑺?鐗囩殑椹卞姩
 *          - 鑷?鍔ㄦ?€娴嬭姱鐗囩被鍨?
 *          - 鏀?鎸佺‖浠禝2C鍜岃蒋浠舵ā鎷烮2C
 *          - 鎻愪緵鍩烘湰鍥惧舰缁樺埗鍔熻兘
 * @author  HDU.ZYU
 * @date    2026
 */

#ifndef _SSD1306_H
#define _SSD1306_H

#include <stdlib.h>
#include <string.h>
#include "ssd1306_i2c.h"
#include "gui.h"

/* ==================== 閰嶇疆閫夐」 ==================== */

/**
 * @brief I2C妯″紡閫夋嫨
 * @param 1: 浣跨敤纭?浠禝2C (HAL搴?)
 * @param 0: 浣跨敤杞?浠舵ā鎷烮2C (GPIO)
 */
#define OLED_I2C_HARD

/* ==================== 鍛戒护/鏁版嵁瀹氫箟 ==================== */

/** @brief I2C鍛戒护鎺у埗瀛楄妭 */
#define OLED_CMD            0X00

/** @brief I2C鏁版嵁鎺у埗瀛楄妭 */
#define OLED_DATA           0X40

/** @brief OLED璁惧?嘔2C鍦板潃 (7浣嶅湴鍧€0x3C宸︾Щ1浣?) */
#define SSD1306_I2C_ADDR    0x78

/** @brief OLED鏄剧ず瀹藉害(鍍忕礌) */
#define SSD1306_WIDTH       128

/** @brief OLED鏄剧ず楂樺害(鍍忕礌) */
#define SSD1306_HEIGHT      64

/* ==================== 鍑芥暟澹版槑 ==================== */

/**
 * @brief   濉?鍏呮暣涓?灞忓箷
 * @param   color: 濉?鍏呴?滆壊 (GUI_COLOR_BLACK/GUI_COLOR_WHITE)
 */
void SSD1306_Fill(GUI_COLOR color);

/**
 * @brief   灏嗘樉瀛樼紦鍐插尯鍐呭?规洿鏂板埌灞忓箷
 * @note    璋冪敤姝ゅ嚱鏁板悗锛岀紦鍐插尯鐨勪慨鏀规墠浼氭樉绀哄埌灞忓箷涓?
 */
void SSD1306_UpdateScreen(void);

/**
 * @brief   鍐欏叆鍗曞瓧鑺傛暟鎹?鎴栧懡浠?
 * @param   dat: 瑕佸啓鍏ョ殑鏁版嵁
 * @param   cmd: OLED_CMD(鍛戒护) 鎴? OLED_DATA(鏁版嵁)
 */
void SSD1306_WriteByte(uint8_t dat, uint8_t cmd);

/**
 * @brief   鍐欏叆澶氬瓧鑺傛暟鎹?
 * @param   data: 鏁版嵁鎸囬拡
 * @param   count: 鏁版嵁闀垮害
 * @param   cmd: OLED_CMD(鍛戒护) 鎴? OLED_DATA(鏁版嵁)
 */
void SSD1306_WriteMuliByte(uint8_t* data, uint16_t count, uint8_t cmd);

/**
 * @brief   鍒濆?嬪寲OLED鏄剧ず灞?
 * @note    鑷?鍔ㄦ?€娴嬭姱鐗囩被鍨?(SSD1306/SH1106)骞跺垵濮嬪寲
 */
void SSD1306_init(void);

/**
 * @brief   鍒囨崲鏄剧ず鍙嶈壊鐘舵€?
 */
void SSD1306_ToggleInvert(void);

/**
 * @brief   缁樺埗鍗曚釜鍍忕礌鐐?
 * @param   x: X鍧愭爣 (0 ~ SSD1306_WIDTH-1)
 * @param   y: Y鍧愭爣 (0 ~ SSD1306_HEIGHT-1)
 * @param   color: 鍍忕礌棰滆壊
 */
void SSD1306_DrawPixel(uint16_t x, uint16_t y, GUI_COLOR color);

/**
 * @brief   璁剧疆缁樺浘鍏夋爣浣嶇疆
 * @param   x: X鍧愭爣
 * @param   y: Y鍧愭爣
 */
void SSD1306_GotoXY(uint16_t x, uint16_t y);

/**
 * @brief   获取显存缓冲区指针 (用于像素级滚动等高级操作)
 * @return  显存缓冲区首地址 (1024 字节, 8 页 × 128 列)
 */
uint8_t* SSD1306_GetBuffer(void);

/**
 * @brief   缁樺埗鐩寸嚎
 * @param   x0: 璧风偣X鍧愭爣
 * @param   y0: 璧风偣Y鍧愭爣
 * @param   x1: 缁堢偣X鍧愭爣
 * @param   y1: 缁堢偣Y鍧愭爣
 * @param   c: 绾挎潯棰滆壊
 */
void SSD1306_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, GUI_COLOR c);

/**
 * @brief   缁樺埗鐭╁舰杈规??
 * @param   x: 宸︿笂瑙扻鍧愭爣
 * @param   y: 宸︿笂瑙扽鍧愭爣
 * @param   w: 瀹藉害
 * @param   h: 楂樺害
 * @param   c: 杈规?嗛?滆壊
 */
void SSD1306_DrawRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, GUI_COLOR c);

/**
 * @brief   缁樺埗濉?鍏呯煩褰?
 * @param   x: 宸︿笂瑙扻鍧愭爣
 * @param   y: 宸︿笂瑙扽鍧愭爣
 * @param   w: 瀹藉害
 * @param   h: 楂樺害
 * @param   c: 濉?鍏呴?滆壊
 */
void SSD1306_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, GUI_COLOR c);

/**
 * @brief   缁樺埗涓夎?掑舰杈规??
 * @param   x1, y1: 绗?涓€涓?椤剁偣鍧愭爣
 * @param   x2, y2: 绗?浜屼釜椤剁偣鍧愭爣
 * @param   x3, y3: 绗?涓変釜椤剁偣鍧愭爣
 * @param   color: 杈规?嗛?滆壊
 */
void SSD1306_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, GUI_COLOR color);

/**
 * @brief   缁樺埗濉?鍏呬笁瑙掑舰
 * @param   x1, y1: 绗?涓€涓?椤剁偣鍧愭爣
 * @param   x2, y2: 绗?浜屼釜椤剁偣鍧愭爣
 * @param   x3, y3: 绗?涓変釜椤剁偣鍧愭爣
 * @param   color: 濉?鍏呴?滆壊
 */
void SSD1306_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, GUI_COLOR color);

/**
 * @brief   缁樺埗鍦嗗舰杈规??
 * @param   x0: 鍦嗗績X鍧愭爣
 * @param   y0: 鍦嗗績Y鍧愭爣
 * @param   r: 鍗婂緞
 * @param   c: 杈规?嗛?滆壊
 */
void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r, GUI_COLOR c);

/**
 * @brief   缁樺埗濉?鍏呭渾褰?
 * @param   x0: 鍦嗗績X鍧愭爣
 * @param   y0: 鍦嗗績Y鍧愭爣
 * @param   r: 鍗婂緞
 * @param   c: 濉?鍏呴?滆壊
 */
void SSD1306_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, GUI_COLOR c);

/**
 * @brief   寮€鍚疧LED鏄剧ず
 */
void SSD1306_ON(void);

/**
 * @brief   鍏抽棴OLED鏄剧ず
 */
void SSD1306_OFF(void);

#endif /* _SSD1306_H */
