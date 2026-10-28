#include "ds18B20.h"
#include "delay_us.h"   /* 鍏?鍏卞井绉掑欢鏃舵湇鍔? */
#include "FreeRTOS.h"   /* FreeRTOS 鍩虹?€澶存枃浠讹紝椤诲湪 task.h 涔嬪墠 */
#include "task.h"       /* taskENTER_CRITICAL / taskEXIT_CRITICAL */

/* DS18B20 数据引脚定义 (PE0 / TEMP) */
#define DATA_GPIO_Port   GPIOE
#define DATA_Pin         TEMP_Pin

#define EnableINT()
#define DisableINT()

#define DS_PRECISION 		0x7f   //绮惧害閰嶇疆瀵勫瓨鍣? 1f=9浣?; 3f=10浣?; 5f=11浣?; 7f=12浣?;
#define DS_AlarmTH  		0x64
#define DS_AlarmTL  		0x8a
#define DS_CONVERT_TICK 1000

#define ResetDQ() HAL_GPIO_WritePin(DATA_GPIO_Port, DATA_Pin, GPIO_PIN_RESET)
#define SetDQ()  	HAL_GPIO_WritePin(DATA_GPIO_Port, DATA_Pin, GPIO_PIN_SET)
#define GetDQ()  	HAL_GPIO_ReadPin(DATA_GPIO_Port, DATA_Pin) 

unsigned char ResetDS18B20(void)
{
	unsigned char resport;
	SetDQ();
	delay_us(50);

	ResetDQ();
	delay_us(500);  //500us 锛堣?ユ椂闂寸殑鏃堕棿鑼冨洿鍙?浠ヤ粠480鍒?960寰?绉掞級
	SetDQ();

	/* 涓寸晫鍖猴細閲囨牱搴旂瓟鑴夊啿鐨勭獥鍙ｄ粎 15 碌s锛屼笉鑳借??鎶㈠崰 */
	taskENTER_CRITICAL();

	delay_us(40);
	uint16_t cnt = 0;
	while(GetDQ() && cnt < 500)
	{
		++cnt;
		delay_us(1);
	}
	if (cnt >= 500)
		resport = 1;
	else
		resport = 0;

	taskEXIT_CRITICAL();

	delay_us(500);  //500us
	SetDQ();
	return resport;
}

void DS18B20WriteByte(unsigned char Dat)
{
	unsigned char i;
	for(i = 8; i > 0; i--)
	{
		/* 涓寸晫鍖猴細闃叉?㈠湪鍐? '1' 鏃朵綆鐢靛钩琚?鎷夐暱瓒呰繃 15 碌s
		 * 淇濇姢鏃堕棿绾? 5-7 碌s/bit锛?8 bit 鍚堣?＄害 48 碌s */
		taskENTER_CRITICAL();

		ResetDQ();
		delay_us(5);
		if(Dat & 0x01)
			SetDQ();
		else
			ResetDQ();

		taskEXIT_CRITICAL();

		delay_us(65);
		SetDQ();
		delay_us(2);
		Dat >>= 1;
	}
}


unsigned char DS18B20ReadByte(void)
{
	unsigned char i, Dat = 0;
	SetDQ();
	delay_us(5);
	for(i = 8; i > 0; i--)
	{
		Dat >>= 1;

		/* 涓寸晫鍖猴細浠庢媺浣庡埌閲囨牱蹇呴』鍦? 15 碌s 鍐呭畬鎴?
		 * 淇濇姢鏃堕棿绾? 15 碌s/bit锛?8 bit 鍚堣?＄害 120 碌s */
		taskENTER_CRITICAL();

		ResetDQ();
		delay_us(5);
		SetDQ();
		delay_us(5);
		if(GetDQ())
			Dat |= 0x80;
		else
			Dat &= 0x7f;

		taskEXIT_CRITICAL();

		delay_us(65);
		SetDQ();
	}
	return Dat;
}

void ReadRom(unsigned char *Read_Addr)
{
	unsigned char i;

	DS18B20WriteByte(ReadROM);
	for(i = 8; i > 0; i--)
	{
		*Read_Addr = DS18B20ReadByte();
		Read_Addr++;
	}
}

void DS18B20Init(unsigned char Precision,unsigned char AlarmTH,unsigned char AlarmTL)
{
	DisableINT();
	ResetDS18B20();
	
	DS18B20WriteByte(SkipROM); 
	DS18B20WriteByte(WriteScratchpad);
	DS18B20WriteByte(AlarmTL);
	DS18B20WriteByte(AlarmTH);
	DS18B20WriteByte(Precision);

	ResetDS18B20();
	DS18B20WriteByte(SkipROM); 
	DS18B20WriteByte(CopyScratchpad);
	EnableINT();

	while(!GetDQ());  //绛夊緟澶嶅埗瀹屾垚 ///////////
}

void DS18B20StartConvert(void)
{
	DisableINT();
	ResetDS18B20();
	DS18B20WriteByte(SkipROM); 
	DS18B20WriteByte(StartConvert); 
	EnableINT();
}

void DS18B20_Configuration(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitStruct.Pin = DATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}


void ds18b20_init(void)
{
	delay_us_init();
	
	DS18B20_Configuration();
	ResetDS18B20();
	DS18B20Init(DS_PRECISION, DS_AlarmTH, DS_AlarmTL);
	DS18B20StartConvert();
}


float ds18b20_read(void)
{
	unsigned char DL, DH;
	unsigned short TemperatureData;
	float Temperature;

	DisableINT();
	DS18B20StartConvert();
	ResetDS18B20();
	DS18B20WriteByte(SkipROM); 
	DS18B20WriteByte(ReadScratchpad);
	DL = DS18B20ReadByte();
	DH = DS18B20ReadByte(); 
	EnableINT();

	TemperatureData = DH;
	TemperatureData <<= 8;
	TemperatureData |= DL;

	Temperature = (float)((float)TemperatureData * 0.0625); //鍒嗚鲸鐜囦负0.0625搴?

	return  Temperature;
}
