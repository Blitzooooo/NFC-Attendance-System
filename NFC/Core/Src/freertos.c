/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "uart_drv.h"
#include "ds18B20.h"
#include "led.h"
#include "key.h"
#include "ssd1306.h"
#include "midi.h"
#include "esp01s.h"
#include "w25qxx.h"
#include "rc522.h"
#include "delay_us.h"
#include "bsp_rtc.h"
#include "display.h"
#include "tim.h"
#include "NFC.h"
#include "esp01s.h"
#include "rtc.h"
#include <stdio.h>
#include <string.h>

/* 锟斤拷知锟斤拷锟斤拷 API Key (锟斤拷锟阶?锟斤拷: https://www.seniverse.com) */
#define WEATHER_API_KEY  "SX5O_Nv09Ap1egenj"
#define WEATHER_CITY     "hangzhou"
#define WEATHER_LANG     "zh-Hans"
#define WEATHER_UNIT     "c"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HEARTBEAT_INTERVAL_MS      5000U    /* 锟斤拷锟斤拷锟斤拷锟酵硷拷锟? (ms) */
#define WEATHER_UPDATE_INTERVAL_MS 3600000U /* 锟斤拷锟斤拷刷锟铰硷拷锟? (ms, 1h) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
UartDrv_t g_uart1Drv;
UartDrv_t g_uart6Drv;

/* LED 锟斤拷锟斤拷: L1~L7 (PE8~PE14), 锟斤拷锟斤拷锟斤拷, 锟酵碉拷平锟斤拷锟斤拷 */
static const Led_Config_t s_ledConfigs[] = {
    {GPIOE, L1_Pin, LED_ON_LOW},
    {GPIOE, L2_Pin, LED_ON_LOW},
    {GPIOE, L3_Pin, LED_ON_LOW},
    {GPIOE, L4_Pin, LED_ON_LOW},
    {GPIOE, L5_Pin, LED_ON_LOW},
    {GPIOE, L6_Pin, LED_ON_LOW},
    {GPIOE, L7_Pin, LED_ON_LOW},
};

/* 锟斤拷锟斤拷锟斤拷锟斤拷: K1~K4 锟斤拷锟斤拷锟斤拷锟斤拷(锟酵碉拷平锟斤拷效), K5~K6 锟斤拷锟斤拷锟斤拷锟斤拷(锟竭碉拷平锟斤拷效) */
static const Key_Config_t s_keyConfigs[] = {
    {GPIOE, K1_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K2_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K3_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K4_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K5_Pin, KEY_ACTIVE_HIGH},
    {GPIOE, K6_Pin, KEY_ACTIVE_HIGH},
};

/* USART1 锟斤拷锟斤拷双锟斤拷锟斤拷 (锟截碉拷写锟斤拷 锟斤拷 uartTask 锟斤拷锟斤拷, 锟劫斤拷锟斤拷锟斤拷锟斤拷) */
static uint8_t  s_uart1_rx_buf[512];
static uint8_t  s_uart1_proc_buf[512];
static volatile uint16_t s_uart1_rx_len = 0;
static volatile uint8_t  s_uart1_data_ready = 0;

/* USART1 锟斤拷锟秸回碉拷锟斤拷锟斤拷 */
static void UART1_RxCallback(UartDrv_RxData_t *pData, void *pUserCtx);
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for guiTask */
osThreadId_t guiTaskHandle;
const osThreadAttr_t guiTask_attributes = {
  .name = "guiTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for keyTask */
osThreadId_t keyTaskHandle;
const osThreadAttr_t keyTask_attributes = {
  .name = "keyTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for uartTask */
osThreadId_t uartTaskHandle;
const osThreadAttr_t uartTask_attributes = {
  .name = "uartTask",
  .stack_size = 1536 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for otherTask */
osThreadId_t otherTaskHandle;
const osThreadAttr_t otherTask_attributes = {
  .name = "otherTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for nfcTask */
osThreadId_t nfcTaskHandle;
const osThreadAttr_t nfcTask_attributes = {
  .name = "nfcTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for myQueue01 */
osMessageQueueId_t myQueue01Handle;
const osMessageQueueAttr_t myQueue01_attributes = {
  .name = "myQueue01"
};
/* Definitions for myQueue04 */
osMessageQueueId_t myQueue04Handle;
const osMessageQueueAttr_t myQueue04_attributes = {
  .name = "myQueue04"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTaskGui(void *argument);
void StartTaskKey(void *argument);
void StartTaskUart(void *argument);
void StartTaskOther(void *argument);
void StartTaskNFC(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of myQueue01 */
  myQueue01Handle = osMessageQueueNew (8, DISP_WEATHER_LINE_LEN, &myQueue01_attributes);

  /* creation of myQueue04 */
  myQueue04Handle = osMessageQueueNew (4, 1024, &myQueue04_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of guiTask */
  guiTaskHandle = osThreadNew(StartTaskGui, NULL, &guiTask_attributes);

  /* creation of keyTask */
  keyTaskHandle = osThreadNew(StartTaskKey, NULL, &keyTask_attributes);

  /* creation of uartTask */
  uartTaskHandle = osThreadNew(StartTaskUart, NULL, &uartTask_attributes);

  /* creation of otherTask */
  otherTaskHandle = osThreadNew(StartTaskOther, NULL, &otherTask_attributes);

  /* creation of nfcTask */
  nfcTaskHandle = osThreadNew(StartTaskNFC, NULL, &nfcTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

  /* Apply I2C clock config to match DISP_I2C_TARGET_HZ */
  DISP_ApplyI2CConfig();

  /* --- BSP 锟斤拷锟斤拷锟斤拷始锟斤拷 --- */

  /* 1. OLED + GUI 锟斤拷锟饺筹拷始锟斤拷, 锟皆憋拷锟斤拷示锟斤拷始锟斤拷锟斤拷锟斤拷 */
  GUI_Init();
  GUI_Clear();
  GUI_Update();

  /* 2. DS18B20 锟铰度达拷锟斤拷锟斤拷 (锟节诧拷锟斤拷锟斤拷 delay_us_init) */
  DISP_ShowInitMsg("Init DS18B20...");
  ds18b20_init();

  /* 3. 锟斤拷锟斤拷锟斤拷锟斤拷 (USART1 锟斤拷 锟斤拷锟斤拷锟斤拷锟?, USART6 锟斤拷 ESP01S) */
  UartDrv_Init(&g_uart1Drv, &huart1);
  UartDrv_SetDebugPort(&g_uart1Drv);
  UartDrv_RegisterRxCb(&g_uart1Drv, UART1_RxCallback, NULL);
  UartDrv_StartRecv(&g_uart1Drv);
  UartDrv_Init(&g_uart6Drv, &huart6);
  UartDrv_StartRecv(&g_uart6Drv);

  /* 4. LED 锟斤拷锟斤拷 (L1~L7) */
  LED_Init(s_ledConfigs, GUI_COUNTOF(s_ledConfigs));

  /* 5. 锟斤拷锟斤拷锟斤拷锟斤拷 (K1~K6) */
  Key_Init(s_keyConfigs, GUI_COUNTOF(s_keyConfigs));

  DISP_ShowInitMsg("Init MIDI...");
  /* 6. MIDI 锟斤拷锟斤拷锟斤拷 (TIM3 CH1 锟斤拷 PB4) */
  MIDI_Init(&htim3, TIM_CHANNEL_1);

  DISP_ShowInitMsg("Init Flash...");
  /* 7. W25Qxx SPI Flash (SPI1) */
  W25QXX_Init();
  /* 锟斤拷锟斤拷系统锟斤拷锟斤拷 + 锟斤拷始锟斤拷锟斤拷录锟斤拷 + 锟斤拷锟斤拷锟斤拷 */
  Config_Load();
  Record_Init();
  Blacklist_Init();

  DISP_ShowInitMsg("Init NFC...");
  /* 8. RC522 NFC (GPIO 模锟斤拷 SPI, 锟节诧拷锟斤拷锟斤拷 delay_us_init 锟捷等帮拷全) */
  RC522_Platform_Init();
  RC522_ConfigISOType('A');  /* 锟斤拷锟斤拷 ISO14443A 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 */

  DISP_ShowInitMsg("Init WiFi...");
  /* 9. ESP01S WiFi 模锟斤拷 (锟节诧拷锟皆讹拷注锟斤拷 UartDrv 锟截碉拷) */
  ESP01S_Init(&g_uart6Drv);

  DISP_ShowInitMsg("Init RTC...");
  /* 10. RTC 锟阶达拷锟较碉拷锟绞硷拷锟斤拷锟? MX_RTC_Init() 通锟斤拷锟斤拷锟捷寄达拷锟斤拷锟叫讹拷锟斤拷锟? */

  DISP_ShowInitMsg("Init Done");
  osDelay(300);

  /* --- LOGO 2s --- */
  DISP_ShowLogo();

  /* --- 锟斤拷息页 2s --- */
  DISP_ShowInfoPage();

  /* 通知 guiTask 锟斤拷 networkTask 锟斤拷锟斤拷锟斤拷锟? */
  osThreadFlagsSet(guiTaskHandle, 0x01);
  osThreadFlagsSet(otherTaskHandle, 0x01);

  /* Infinite loop */
  for(;;)
  {
    DISP_UpdateTempCache();   /* 锟届步锟斤拷锟斤拷锟铰度伙拷锟斤拷, guiTask 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 OneWire */

    /* OLED 锟斤拷锟斤拷锟斤拷锟斤拷锟? (锟斤拷锟斤拷锟斤拷 guiTask, 确锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷时也锟杰恢革拷) */
    if (DISP_CheckStuck()) {
      DISP_ResetOLED();
    }

    /* 锟斤拷锟斤拷锟斤拷锟斤拷: 每 5s 锟斤拷锟斤拷位锟斤拷锟较憋拷锟借备状态 */
    {
      static uint32_t s_lastHeartbeat = 0;
      uint32_t now = osKernelGetTickCount();
      if (now - s_lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        s_lastHeartbeat = now;
        char hb[128];
        snprintf(hb, sizeof(hb),
                 "HEART:DEV=%u|MODE=%u|TEMP=%.1f|WIFI=%u|PEND=%lu\r\n",
                 Config_GetDeviceId(),
                 Config_GetAttendMode(),
                 (double)DISP_GetCachedTemp(),
                 ESP01S_IsWiFiConnected() ? 1U : 0U,
                 (unsigned long)Record_GetUnuploadCount());
        UartDrv_SendStr(&g_uart1Drv, hb);
      }
    }

    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTaskGui */
/**
* @brief Function implementing the guiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskGui */
void StartTaskGui(void *argument)
{
  /* USER CODE BEGIN StartTaskGui */
  const DispCardInfo_t *pCard;
  uint32_t stateEnterTick = 0;
  uint32_t now;
  int i;

  DISP_Init();

  /* 锟饺达拷 defaultTask 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟? */
  osThreadFlagsWait(0x01, osFlagsWaitAll, osWaitForever);
  DISP_SetState(DISP_STATE_CLOCK);

  /* Infinite loop */
  for (;;)
  {
    DISP_FeedWatchdog();  /* 每锟斤拷循锟斤拷喂锟斤拷, defaultTask 锟斤拷锟斤拷锟解卡锟斤拷锟斤拷锟斤拷位 */
    DispState_t state = DISP_GetState();
    now = osKernelGetTickCount();

    switch (state) {

    /* ===== 时锟接达拷锟斤拷 ===== */
    case DISP_STATE_CLOCK:
      {
        /* 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 (锟斤拷锟斤拷 networkTask 锟侥讹拷锟斤拷锟斤拷息) */
        char weatherMsg[DISP_WEATHER_LINE_LEN];
        if (osMessageQueueGet(myQueue01Handle, weatherMsg, NULL, 0) == osOK) {
          DISP_SetWeatherCache(weatherMsg);
        }
      }
      DISP_ShowClock();
      {
        uint32_t nfcEvt = DISP_CheckNfcEvent(myQueue04Handle);   /* poll NFC queue */
        if (nfcEvt != DISP_EVT_NONE) {
          pCard = DISP_GetCardInfo();
          if (pCard->event == DISP_EVT_CARD_ADMIN) {
            DISP_SetState(DISP_STATE_ADMIN_SET);
          } else {
            DISP_SetState(DISP_STATE_RESULT);
          }
          stateEnterTick = osKernelGetTickCount();
        } else {
          osDelay(DISP_FRAME_INTERVAL_MS);   /* keep frame pacing */
        }
      }

      /* K5 锟斤拷锟斤拷: 锟斤拷锟斤拷时锟斤拷锟斤拷锟斤拷 */
      if (Key_IsLongPressed(4)) {
        DISP_SetState(DISP_STATE_TIME_SET);
        stateEnterTick = osKernelGetTickCount();
      }
      break;

    /* ===== 刷锟斤拷锟斤拷锟? ===== */
    case DISP_STATE_RESULT:
      pCard = DISP_GetCardInfo();
      {
        int countdown = 0;
        if (pCard->event == DISP_EVT_CARD_DUP) {
          int elapsed = (int)(now - stateEnterTick);
          countdown = (elapsed < 2000) ? (2 - elapsed / 1000) : 0;
        }
        DISP_ShowCardResult(pCard, countdown);
      }

      /* 锟饺帮拷锟斤拷锟酵筹拷时 */
      osThreadFlagsWait(DISP_EVT_KEY_PRESSED, osFlagsWaitAny, DISP_FRAME_INTERVAL_MS);

      /* 锟斤拷锟解按锟斤拷 锟斤拷 锟斤拷锟斤拷时锟斤拷 */
      if (Key_AnyPressed()) {
        Key_ClearAllEvents();
        DISP_SetState(DISP_STATE_CLOCK);
        break;
      }

      /* 锟斤拷时 锟斤拷 锟斤拷锟斤拷时锟斤拷 (锟截革拷刷锟斤拷 2s, 锟斤拷锟斤拷 3s) */
      {
        int timeoutMs = (pCard->event == DISP_EVT_CARD_DUP) ? 2000 : 3000;
        if (now - stateEnterTick >= (uint32_t)timeoutMs) {
          DISP_SetState(DISP_STATE_CLOCK);
        }
      }
      break;

    /* ===== 时锟斤拷锟斤拷锟斤拷 ===== */
    case DISP_STATE_TIME_SET:
      DISP_ShowTimeSet();

      /* 锟斤拷锟斤拷锟斤拷 */
      for (i = 0; i < 6; i++) {
        if (Key_IsShortPressed(i))
          DISP_TimeSetKey(i, 1, 0);
        if (Key_IsLongPressed(i))
          DISP_TimeSetKey(i, 0, 1);
      }

      /* 锟斤拷一帧锟斤拷锟斤拷欠锟斤拷锟斤拷顺锟? */
      osThreadFlagsWait(DISP_EVT_KEY_PRESSED, osFlagsWaitAny, DISP_FRAME_INTERVAL_MS);
      osThreadFlagsClear(DISP_EVT_KEY_PRESSED);

      /* 60 锟诫超时锟皆讹拷锟剿筹拷 */
      if (now - stateEnterTick >= 60000) {
        DISP_SetState(DISP_STATE_CLOCK);
      }
      break;

    /* ===== 锟斤拷锟斤拷员锟斤拷锟斤拷 ===== */
    case DISP_STATE_ADMIN_SET:
      DISP_ShowAdminSet();

      /* 锟斤拷锟斤拷锟斤拷 */
      for (i = 0; i < 6; i++) {
        if (Key_IsShortPressed(i))
          DISP_AdminSetKey(i, 1, 0);
        if (Key_IsLongPressed(i))
          DISP_AdminSetKey(i, 0, 1);
      }

      /* 锟斤拷一帧锟斤拷锟斤拷欠锟斤拷锟斤拷顺锟? */
      osThreadFlagsWait(DISP_EVT_KEY_PRESSED, osFlagsWaitAny, DISP_FRAME_INTERVAL_MS);
      osThreadFlagsClear(DISP_EVT_KEY_PRESSED);

      /* 120 锟诫超时锟皆讹拷锟剿筹拷 */
      if (now - stateEnterTick >= 120000) {
        DISP_SetState(DISP_STATE_CLOCK);
      }
      break;
    }
  }
  /* USER CODE END StartTaskGui */
}

/* USER CODE BEGIN Header_StartTaskKey */
/**
* @brief Function implementing the keyTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskKey */
void StartTaskKey(void *argument)
{
  /* USER CODE BEGIN StartTaskKey */
  /* Infinite loop */
  for(;;)
  {
    Key_Scan();         /* 锟斤拷锟斤拷扫锟斤拷, 锟斤拷每 10ms 锟斤拷锟斤拷一锟斤拷 */
    osDelay(10);
  }
  /* USER CODE END StartTaskKey */
}

/* USER CODE BEGIN Header_StartTaskUart */
/**
* @brief Function implementing the uartTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskUart */
void StartTaskUart(void *argument)
{
  /* USER CODE BEGIN StartTaskUart */
  char  lineBuf[256];
  int   lineLen = 0;
  char  respBuf[256];

  /* Infinite loop */
  for(;;)
  {
    /* 锟斤拷锟? USART1 锟角凤拷锟斤拷锟斤拷锟斤拷锟斤拷 (双锟斤拷锟斤拷: 锟截碉拷写锟斤拷 锟斤拷 锟劫斤拷锟斤拷锟斤拷锟斤拷 锟斤拷 锟斤拷锟斤拷) */
    if (s_uart1_data_ready) {
      uint16_t len;
      taskENTER_CRITICAL();
      len = s_uart1_rx_len;
      memcpy(s_uart1_proc_buf, s_uart1_rx_buf, len);
      s_uart1_rx_len = 0;
      s_uart1_data_ready = 0;
      taskEXIT_CRITICAL();

      /* 锟斤拷锟斤拷锟斤拷锟斤拷追锟接碉拷锟叫伙拷锟斤拷 (锟斤拷时锟截碉拷锟斤拷同时写锟斤拷 s_uart1_rx_buf) */
      for (uint16_t i = 0; i < len && lineLen < (int)sizeof(lineBuf) - 1; i++) {
        char ch = (char)s_uart1_proc_buf[i];
        lineBuf[lineLen++] = ch;
        /* 锟斤拷锟斤拷锟斤拷锟斤拷 锟斤拷 执锟斤拷锟斤拷锟斤拷 */
        if (ch == '\n' || lineLen >= (int)sizeof(lineBuf) - 1) {
          lineBuf[lineLen] = '\0';
          /* 时锟斤拷同锟斤拷锟节硷拷芫锟斤拷锟斤拷锟斤拷锟斤拷锟? */
          if (DISP_IsTimeSyncBusy()) {
            snprintf(respBuf, sizeof(respBuf), "ERR:SYNC\r\n");
          } else {
            NFC_ProcessCommand(lineBuf, respBuf, sizeof(respBuf));
          }
          UartDrv_SendStr(&g_uart1Drv, respBuf);
          NFC_FlushCmdNotify();  /* 锟饺凤拷锟斤拷应锟斤拷通知 nfcTask, 锟斤拷证锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟? */
          lineLen = 0;
        }
      }
    }
    osDelay(1);
  }
  /* USER CODE END StartTaskUart */
}

/* USER CODE BEGIN Header_StartTaskOther */
/**
* @brief Function implementing the otherTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskOther */
/* 天气查询辅助函数: 调用心知天气 API 并更新显示缓存 */
static void UpdateWeather(void)
{
    char city[32]    = {0};
    char day[32]     = {0};
    char high[8]     = {0};
    char night[32]   = {0};
    char low[8]      = {0};
    char precip[8]   = {0};
    char weatherLine[DISP_WEATHER_LINE_LEN] = {0};

    if (ESP01S_QueryWeather(WEATHER_API_KEY, WEATHER_CITY,
                            WEATHER_LANG, WEATHER_UNIT,
                            city, sizeof(city),
                            day, sizeof(day),
                            high, sizeof(high),
                            night, sizeof(night),
                            low, sizeof(low),
                            precip, sizeof(precip)) == 0) {
        snprintf(weatherLine, sizeof(weatherLine),
                 "%s %s %s~%s\xe2\x84\x83",
                 city, day, low, high);
        osMessageQueuePut(myQueue01Handle, weatherLine, 0, 0);
    }
}

void StartTaskOther(void *argument)
{
  /* USER CODE BEGIN StartTaskOther */

  /* 等待 defaultTask 完成硬件初始化 */
  osThreadFlagsWait(0x01, osFlagsWaitAll, osWaitForever);

  /* ===== 首次 WiFi 联网 + NTP 授时 + 天气 ===== */
  {
    int espRet = ESP01S_Start();

    if (espRet == 0 || espRet == -3) {
      if (ESP01S_IsNtpSynced()) {
        BSP_RTC_DateTime_t rtcBefore;
        BSP_RTC_GetDateTime(&rtcBefore);
        uint32_t beforeSecs = DISP_DateTimeToSeconds(&rtcBefore);

        ESP01S_SetRtcFromNtp(&hrtc);

        BSP_RTC_DateTime_t rtcAfter;
        BSP_RTC_GetDateTime(&rtcAfter);
        uint32_t afterSecs = DISP_DateTimeToSeconds(&rtcAfter);
        int32_t delta = (int32_t)(afterSecs - beforeSecs);

        DISP_EnterTimeSync();
        DISP_ShowInitMsg("Time Sync...");
        Record_FixAllTimeOffsets(delta);
        DISP_ExitTimeSync();

        DISP_SetTimeOffset(0);
      }
    }
  }

  /* ===== 首次天气查询 ===== */
  {
    UpdateWeather();
  }

  /* ===== 主循环: 断网重连 + 定时天气刷新 ===== */
  for(;;)
  {
    static uint32_t s_lastWeatherUpdate = 0;
    static uint32_t s_lastReconnectTry  = 0;
    uint32_t nowTick = osKernelGetTickCount();

    /* --- 断网自动重连 (每 120s) --- */
    if (!ESP01S_IsWiFiConnected() && (nowTick - s_lastReconnectTry >= 120000U)) {
      s_lastReconnectTry = nowTick;
      printf("[ESP01S] WiFi lost, retry...\r\n");

      int espRet = ESP01S_Start();
      if ((espRet == 0 || espRet == -3) && ESP01S_IsNtpSynced()) {
        BSP_RTC_DateTime_t rtcBefore, rtcAfter;
        BSP_RTC_GetDateTime(&rtcBefore);
        uint32_t beforeSecs = DISP_DateTimeToSeconds(&rtcBefore);
        ESP01S_SetRtcFromNtp(&hrtc);
        BSP_RTC_GetDateTime(&rtcAfter);
        uint32_t afterSecs = DISP_DateTimeToSeconds(&rtcAfter);
        DISP_SetTimeOffset((int32_t)(beforeSecs - afterSecs));
      }
    }

    /* --- 每 1h 刷新天气 --- */
    if (ESP01S_IsWiFiConnected() && (nowTick - s_lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL_MS)) {
      s_lastWeatherUpdate = nowTick;
      UpdateWeather();
    }

    osDelay(1000);
  }
  /* USER CODE END StartTaskOther */
}

/* USER CODE BEGIN Header_StartTaskNFC */
/**
* @brief Function implementing the nfcTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskNFC */
void StartTaskNFC(void *argument)
{
  /* USER CODE BEGIN StartTaskNFC */
  uint8_t  cardID[4];
  NFC_CardData_t cardData;
  NFC_Event_t    nfcEvent;
  uint32_t       lastUID        = 0;
  uint32_t       lastNotifyTick = 0;
  int            cardPresent    = 0;
  int            missCount      = 0;
  uint32_t       dispEvent;

  #define CARD_MISS_THRESHOLD    2     /* 锟斤拷锟斤拷 2 锟斤拷未锟斤拷獾? 锟斤拷 锟斤拷锟诫卡 (2锟斤拷200ms=400ms) */
  #define CARD_SCAN_INTERVAL_MS  200   /* 寻锟斤拷锟斤拷锟? */
  #define CARD_DEDUP_TIMEOUT_MS  3000  /* 锟斤拷锟截革拷锟斤拷时, 锟斤拷 OLED 锟斤拷锟揭? 3s 同锟斤拷 */
  #define CARD_STABLE_DELAY_MS   300   /* 锟斤拷獾斤拷锟斤拷锟斤拷榷锟斤拷映锟?, 确锟斤拷锟斤拷片锟斤拷全锟斤拷锟斤拷锟斤拷频锟斤拷 */

  /* Infinite loop */
  for (;;)
  {
    /* 时锟斤拷同锟斤拷锟节硷拷锟斤拷停刷锟斤拷 */
    if (DISP_IsTimeSyncBusy()) {
      osDelay(CARD_SCAN_INTERVAL_MS);
      continue;
    }

    /* ===== 锟叫匡拷: 锟饺达拷锟斤拷位锟斤拷锟斤拷锟斤拷 + 锟斤拷锟斤拷锟皆硷拷锟斤拷肟? (锟斤拷锟斤拷锟斤拷) ===== */
    if (cardPresent) {
      /* 锟斤拷位锟斤拷锟角凤拷锟斤拷锟斤拷锟揭伙拷锟斤拷锟斤拷锟? (锟斤拷锟斤拷/锟斤拷锟斤拷/锟斤拷锟斤拷)? */
      if (NFC_IsCmdDone()) {
        NFC_ClearCmdDone();

        /* 锟斤拷锟斤拷欠锟斤拷锟揭?全锟斤拷锟截讹拷 (UPDATEIMG 锟斤拷位图锟窖憋拷) */
        if (NFC_IsFullRereadNeeded()) {
          NFC_ClearFullReread();

          /* 全锟斤拷锟截讹拷: 锟斤拷图锟斤拷锟斤拷锟斤拷, 刷锟斤拷位图锟斤拷锟斤拷 */
          if (!NFC_IsReaderLocked()) {
            while (!NFC_LockReader()) { osDelay(10); }
            nfcEvent = NFC_ReadCard(cardID, &cardData);
            NFC_UnlockReader();

            if (nfcEvent != NFC_EVT_INVALID) {
              /* 位图锟斤拷刷锟斤拷, 通知 GUI 锟斤拷锟斤拷锟斤拷示 */
              DISP_SendNfcEvent(myQueue04Handle,  DISP_EVT_CARD_VALID, cardID, 4,
                                   cardData.cardId, cardData.sid, cardData.points,
                                   cardData.cardType, cardData.statusFlag,
                                   cardData.swipeDT.hour,
                                   cardData.swipeDT.minute,
                                   cardData.swipeDT.second,
                                   0, 0, 0);
            }
            /* 锟矫筹拷 CPU 锟斤拷 guiTask 刷锟斤拷 OLED */
            osDelay(50);
            NFC_Recover();
          }
        } else {
          /* 锟斤拷锟斤拷锟截讹拷: 锟斤拷锟斤拷 Block1 锟剿伙拷头 (位图未锟斤拷) */
          if (!NFC_IsReaderLocked()) {
            while (!NFC_LockReader()) { osDelay(10); }
            nfcEvent = NFC_ReadCardHeader(cardID, &cardData);
            NFC_UnlockReader();

            if (nfcEvent != NFC_EVT_INVALID) {
              /* 通知 GUI 锟斤拷示写锟斤拷锟斤拷锟? */
              DISP_SendNfcEvent(myQueue04Handle,  DISP_EVT_CARD_VALID, cardID, 4,
                                   cardData.cardId, cardData.sid, cardData.points,
                                   cardData.cardType, cardData.statusFlag,
                                   cardData.swipeDT.hour,
                                   cardData.swipeDT.minute,
                                   cardData.swipeDT.second,
                                   0, 0, 0);
            }
            osDelay(50);
          }
        }
        /* 锟窖达拷锟斤拷锟斤拷 CmdDone, 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷寻锟斤拷, 锟斤拷 GUI 锟斤拷时锟斤拷锟斤拷染 */
        osDelay(50);
        continue;
      }

      /* 锟斤拷锟斤拷锟皆硷拷饪?锟角凤拷锟斤拷 (锟斤拷锟斤拷锟斤拷: 锟斤拷锟斤拷 3 锟斤拷失锟杰诧拷锟斤拷锟诫卡) */
      if (NFC_IsReaderLocked()) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;  /* uartTask 锟斤拷锟节诧拷锟斤拷, 锟斤拷锟斤拷锟斤拷锟矫诧拷 */
      }
      if (!NFC_LockReader()) {
        osDelay(100);
        continue;
      }
      RC522_Halt();
      osDelay(2);
      {
        char st = NFC_ScanCard(cardID);
        if (st == RC522_OK) {
          RC522_Halt();
          missCount = 0;
        } else {
          missCount++;
        }
        NFC_UnlockReader();
        if (missCount >= 3) {
          cardPresent = 0;
          missCount   = 0;
          lastUID     = 0;
          LED_SetLeds(0x00);
        }
      }
      osDelay(200);
      continue;
    }

    /* ===== 锟睫匡拷: 锟斤拷锟斤拷寻锟斤拷 ===== */
    {
      char status;
      /* 锟斤拷锟斤拷锟轿伙拷锟斤拷锟斤拷诓锟斤拷锟?, 直锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 */
      if (NFC_IsReaderLocked()) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;
      }
      /* 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷, 锟斤拷锟斤拷位锟斤拷占锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 */
      if (!NFC_LockReader()) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;
      }
      status = NFC_ScanCard(cardID);
      NFC_UnlockReader();  /* 寻锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷头锟斤拷锟? */
      if (status != RC522_OK) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;
      }
    }

    /* 锟斤拷锟斤拷獾?: 锟斤拷锟斤拷欠锟轿?锟秸达拷锟斤拷锟斤拷锟酵?一锟脚匡拷 (锟斤拷位锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷) */
    {
      uint32_t uid32 = ((uint32_t)cardID[0] << 24) |
                       ((uint32_t)cardID[1] << 16) |
                       ((uint32_t)cardID[2] << 8)  |
                        (uint32_t)cardID[3];
      if (uid32 == lastUID && lastUID != 0) {
        /* 同一锟脚匡拷锟斤拷锟斤拷, 锟斤拷锟斤拷头(锟斤拷刷锟斤拷位图, 锟斤拷锟解覆锟斤拷 GUI 锟斤拷锟斤拷锟斤拷示锟斤拷图锟斤拷) */
        cardPresent = 1;
        missCount   = 0;
        LED_On(0);

        /* 锟饺讹拷锟接迟诧拷锟斤拷锟斤拷; while 循锟斤拷锟皆讹拷锟饺达拷锟斤拷位锟斤拷锟酵凤拷锟斤拷 */
        osDelay(CARD_STABLE_DELAY_MS);
        while (!NFC_LockReader()) { osDelay(10); }
        nfcEvent = NFC_ReadCardHeader(cardID, &cardData);
        NFC_UnlockReader();

        if (nfcEvent != NFC_EVT_INVALID) {
          DISP_SendNfcEvent(myQueue04Handle, DISP_EVT_CARD_VALID, cardID, 4,
                               cardData.cardId, cardData.sid, cardData.points,
                               cardData.cardType, cardData.statusFlag,
                               cardData.swipeDT.hour,
                               cardData.swipeDT.minute,
                               cardData.swipeDT.second,
                               0, 0, 0);
        }
        lastNotifyTick = osKernelGetTickCount();
        osDelay(200);
        continue;
      }
    }

    /* 锟铰匡拷锟阶达拷刷锟斤拷: 锟斤拷锟斤拷锟斤拷锟斤拷 */
    cardPresent = 1;
    missCount   = 0;
    LED_On(0);

    /* ===== 去锟斤拷: 同一锟脚匡拷锟节斤拷锟揭筹拷锟绞撅拷诩锟? (3s) 锟节诧拷锟截革拷锟斤拷锟斤拷 ===== */
    {
      uint32_t uid32 = ((uint32_t)cardID[0] << 24) |
                       ((uint32_t)cardID[1] << 16) |
                       ((uint32_t)cardID[2] << 8)  |
                        (uint32_t)cardID[3];
      if (uid32 == lastUID && lastUID != 0) {
        uint32_t elapsed = osKernelGetTickCount() - lastNotifyTick;
        if (elapsed < CARD_DEDUP_TIMEOUT_MS) {
          /* 3s 锟斤拷同一锟脚匡拷: 锟截革拷刷锟斤拷 (锟斤拷 WaitCardOff 锟斤拷锟斤拷) */
          MIDI_Beep(1, 50);
          DISP_SendNfcEvent(myQueue04Handle, DISP_EVT_CARD_DUP, cardID, 4,
                               0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0);
          while (!NFC_LockReader()) { osDelay(50); }
          NFC_WaitCardOff();
          NFC_UnlockReader();
          cardPresent = 0;
          LED_SetLeds(0x00);
          osDelay(200);
          continue;
        }
        /* 锟斤拷时, 锟斤拷锟斤拷锟劫达拷刷锟斤拷, 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 */
      }
      lastUID = uid32;
    }

    /* 寻锟斤拷锟斤拷: 锟较革拷锟斤拷锟斤拷锟斤拷 */
    MIDI_Beep(6, 50);

    /* 锟饺达拷锟斤拷片锟斤拷锟斤拷频锟斤拷锟斤拷锟饺讹拷 (锟斤拷锟斤拷锟斤拷锟?) */
    osDelay(CARD_STABLE_DELAY_MS);

    /* 锟斤拷锟铰硷拷锟斤拷锟斤拷取锟斤拷锟斤拷 (锟斤拷锟斤拷位锟斤拷占锟斤拷锟斤拷 while 锟皆讹拷锟饺达拷) */
    while (!NFC_LockReader()) { osDelay(50); }

    /* ===== 锟斤拷锟斤拷: 锟斤拷取锟斤拷锟斤拷锟斤拷 =====
     * GUI 锟斤拷锟斤拷锟斤拷示锟斤拷锟绞憋拷锟斤拷锟酵?, 锟斤拷锟解覆锟斤拷位图锟斤拷锟铰伙拷锟斤拷/锟斤拷锟斤拷 */
    if (DISP_GetState() == DISP_STATE_RESULT) {
      nfcEvent = NFC_ReadCardHeader(cardID, &cardData);
    } else {
      nfcEvent = NFC_ReadCard(cardID, &cardData);
    }

    /* 锟斤拷锟斤拷锟斤拷锟?, 锟酵凤拷 RC522 锟斤拷 */
    NFC_UnlockReader();

    {
      uint8_t  evtType   = 0;
      uint8_t  recStatus = 0;
      uint32_t durSec    = 0;
      uint32_t uidNfc    = ((uint32_t)cardID[0] << 24) | ((uint32_t)cardID[1] << 16) |
                            ((uint32_t)cardID[2] << 8)  |  (uint32_t)cardID[3];

    /* ===== 锟斤拷锟斤拷: LED + 锟斤拷锟斤拷 + 锟铰硷拷锟斤拷锟斤拷 ===== */
    switch (nfcEvent) {
    case NFC_EVT_ADMIN:
      /* 锟斤拷锟斤拷员锟斤拷: L1+L3 锟斤拷, 锟较革拷锟斤拷锟斤拷锟斤拷, 锟斤拷锟斤拷锟斤拷锟斤拷锟叫讹拷 */
      dispEvent = DISP_EVT_CARD_ADMIN;
      LED_On(2);
      MIDI_Beep(6, 150);
      break;

    case NFC_EVT_VALID:
    {
      /* ===== 锟斤拷锟斤拷锟斤拷锟斤拷锟?: 锟斤拷失锟斤拷锟杰撅拷 ===== */
      if (Blacklist_IsBlocked(uidNfc)) {
        printf("[BL] BLOCKED UID=%08lX, count=%u\r\n",
               (unsigned long)uidNfc, (unsigned)Blacklist_GetCount());
        dispEvent = DISP_EVT_CARD_INVALID;
        cardData.statusFlag = 0xFF;  /* 锟斤拷锟轿?锟斤拷失, OLED 锟斤拷示"锟窖癸拷失" */
        MIDI_Beep(3, 100);
        osDelay(100);
        MIDI_Beep(3, 100);
        /* 锟较憋拷锟斤拷失锟斤拷锟斤拷录 */
        printf("REC:UID=%08lX|SID=%lu|LOST\r\n",
               (unsigned long)uidNfc, (unsigned long)cardData.sid);
        /* 写一锟斤拷 LOST 锟斤拷录锟斤拷 Flash */
        {
          SwipeRecord_t rec;
          BSP_RTC_DateTime_t nowDT;
          BSP_RTC_GetDateTime(&nowDT);
          memset(&rec, 0, sizeof(rec));
          rec.deviceId   = DISP_GetAdminCfg()->deviceId;
          rec.uid        = uidNfc;
          rec.sid        = cardData.sid;
          rec.year       = nowDT.year % 100;
          rec.month      = nowDT.month;
          rec.day        = nowDT.day;
          rec.hour       = nowDT.hour;
          rec.minute     = nowDT.minute;
          rec.second     = nowDT.second;
          rec.eventType  = 1;
          rec.status     = 4;  /* 4 = LOST */
          rec.cardType   = cardData.cardType;
          rec.timeOffset = (uint16_t)DISP_GetTimeOffset();
          Record_Add(&rec);
        }
        break;
      }

      /* ===== 锟斤拷锟斤拷员模式锟铰拒撅拷锟斤拷通锟斤拷 ===== */
      if (DISP_GetState() == DISP_STATE_ADMIN_SET) {
        /* 锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷示锟杰撅拷, 锟斤拷写锟斤拷录, 锟斤拷锟斤拷LED, 锟斤拷通知GUI */
        dispEvent = DISP_EVT_NONE;
        MIDI_Beep(3, 60);
        osDelay(60);
        MIDI_Beep(3, 60);
        break;
      }

      /* ===== 锟斤拷锟斤拷锟叫讹拷 ===== */
      const DispAdminCfg_t *pCfg = DISP_GetAdminCfg();
      uint8_t  mode      = pCfg->attendMode;
      uint8_t  ledMask   = 0;

      /* 锟斤拷询锟斤拷锟斤拷 (锟斤拷锟斤拷锟斤拷 UID) */
      uint32_t enterSecs = 0;
      uint8_t cacheState = Cache_Find(uidNfc, &enterSecs);

      /* 锟斤拷锟斤拷未锟斤拷锟斤拷锟斤拷扫 Flash */
      if (cacheState == CACHE_MISS) {
        SwipeRecord_t lastRec;
        if (Record_QueryLast(uidNfc, &lastRec)) {
          cacheState = lastRec.eventType;  /* 0=锟诫场 1=锟诫场 */
          if (cacheState == 0) {
            /* 锟接硷拷录锟截斤拷 enterSecs: 锟斤拷刷锟斤拷时锟教癸拷锟斤拷 (锟诫级锟斤拷锟饺可斤拷锟斤拷) */
            BSP_RTC_DateTime_t dt;
            dt.year = 2000 + lastRec.year;
            dt.month = lastRec.month; dt.day = lastRec.day;
            dt.hour = lastRec.hour; dt.minute = lastRec.minute; dt.second = lastRec.second;
            enterSecs = DISP_DateTimeToSeconds(&dt);
          }
        }
      }

      /* 锟斤拷模式锟叫讹拷 */
      switch (mode) {
      case 0: /* 锟斤拷锟侥Ｊ? */
        if (cacheState == 0) {
          /* 锟斤拷锟诫场 锟斤拷 锟斤拷效锟截革拷 */
          evtType = 1; recStatus = 1;
        } else {
          /* 锟诫场 */
          evtType = 0; recStatus = 0; ledMask = 0x08;  /* L4 */
        }
        break;

      case 1: /* 锟斤拷锟斤拷模式 */
        if (cacheState == 0) {
          /* 锟斤拷锟诫场 锟斤拷 锟诫场 + 锟斤拷时锟斤拷 (RTC 锟斤拷锟届安全) */
          evtType = 1; recStatus = 0; ledMask = 0x10;  /* L5 */
          /* 锟斤拷 RTC 锟斤拷前时锟斤拷锟斤拷锟绞憋拷锟?, 锟斤拷然支锟街匡拷锟斤拷 */
          {
            BSP_RTC_DateTime_t nowDT;
            BSP_RTC_GetDateTime(&nowDT);
            uint32_t nowSecs = DISP_DateTimeToSeconds(&nowDT);
            durSec = (nowSecs > enterSecs) ? (nowSecs - enterSecs) : 0;
          }
        } else {
          /* 锟斤拷锟诫场锟斤拷录 */
          evtType = 1; recStatus = 2;
        }
        break;

      case 2: /* 锟斤拷锟斤拷锟侥Ｊ? */
      default:
        if (cacheState == 0) {
          /* 锟斤拷锟诫场 锟斤拷 锟诫场 + 锟斤拷时锟斤拷 (RTC 锟斤拷锟届安全) */
          evtType = 1; recStatus = 0; ledMask = 0x40;  /* L7 */
          {
            BSP_RTC_DateTime_t nowDT;
            BSP_RTC_GetDateTime(&nowDT);
            uint32_t nowSecs = DISP_DateTimeToSeconds(&nowDT);
            durSec = (nowSecs > enterSecs) ? (nowSecs - enterSecs) : 0;
          }
        } else {
          /* 锟诫场 */
          evtType = 0; recStatus = 0; ledMask = 0x20;  /* L6 */
        }
        break;
      }

      /* 锟斤拷锟铰伙拷锟斤拷 (锟诫场时锟斤拷录 RTC 锟斤拷时锟斤拷锟?) */
      if (recStatus == 0) {
        uint32_t secs = 0;
        if (evtType == 0) {
          /* 锟诫场: 锟斤拷录锟斤拷前 RTC 锟斤拷时锟斤拷锟斤拷锟斤拷锟斤拷锟?, 锟斤拷锟诫场时锟斤拷锟斤拷锟斤拷锟绞憋拷锟? */
          BSP_RTC_DateTime_t nowDT;
          BSP_RTC_GetDateTime(&nowDT);
          secs = DISP_DateTimeToSeconds(&nowDT);
        }
        Cache_Update(uidNfc, evtType, secs);
      }

      /* 写 SPI Flash 锟斤拷录 */
      {
        SwipeRecord_t rec;
        BSP_RTC_DateTime_t nowDT;
        BSP_RTC_GetDateTime(&nowDT);
        memset(&rec, 0, sizeof(rec));
        rec.deviceId   = pCfg->deviceId;
        rec.uid        = uidNfc;
        rec.sid        = cardData.sid;
        rec.year       = nowDT.year % 100;
        rec.month      = nowDT.month;
        rec.day        = nowDT.day;
        rec.hour       = nowDT.hour;
        rec.minute     = nowDT.minute;
        rec.second     = nowDT.second;
        rec.eventType  = evtType;
        rec.status     = recStatus;
        rec.cardType   = cardData.cardType;
        rec.durationSec = durSec;
        rec.timeOffset = (uint16_t)DISP_GetTimeOffset();
        Record_Add(&rec);
      }

      /* LED + 锟斤拷锟斤拷 */
      if (recStatus == 0) {
        LED_SetLeds(ledMask | 0x01);
        MIDI_Beep(6, 100);
        dispEvent = DISP_EVT_CARD_VALID;
      } else if (recStatus == 1) {
        dispEvent = DISP_EVT_CARD_DUP;
        MIDI_Beep(3, 80);
      } else {
        dispEvent = DISP_EVT_CARD_VALID;
        MIDI_Beep(3, 80);
        osDelay(80);
        MIDI_Beep(3, 80);
      }
      break;
    }

    case NFC_EVT_INVALID:
    default:
      /* 锟斤拷效锟斤拷: 锟斤拷锟斤拷硬锟斤拷锟街革拷 RC522, 锟斤拷止芯片状态锟斤拷锟斤拷影锟斤拷锟斤拷锟窖帮拷锟? */
      NFC_Recover();
      dispEvent = DISP_EVT_CARD_INVALID;
      MIDI_Beep(3, 100);
      osDelay(100);
      MIDI_Beep(3, 100);
      {
        SwipeRecord_t rec;
        BSP_RTC_DateTime_t nowDT;
        BSP_RTC_GetDateTime(&nowDT);
        memset(&rec, 0, sizeof(rec));
        rec.deviceId   = DISP_GetAdminCfg()->deviceId;
        rec.uid        = uidNfc;
        rec.sid        = 0;
        rec.year       = nowDT.year % 100;
        rec.month      = nowDT.month;
        rec.day        = nowDT.day;
        rec.hour       = nowDT.hour;
        rec.minute     = nowDT.minute;
        rec.second     = nowDT.second;
        rec.eventType  = 1;
        rec.status     = 3;
        rec.cardType   = 0;
        rec.timeOffset = (uint16_t)DISP_GetTimeOffset();
        Record_Add(&rec);
      }
      break;
    }

    /* ===== 通知 GUI ===== */
    DISP_SendNfcEvent(myQueue04Handle, dispEvent, cardID, 4,
                         cardData.cardId, cardData.sid, cardData.points,
                         cardData.cardType, cardData.statusFlag,
                         cardData.swipeDT.hour,
                         cardData.swipeDT.minute,
                         cardData.swipeDT.second,
                         evtType, recStatus, durSec);
    }
    lastNotifyTick = osKernelGetTickCount();
    NFC_ClearCmdDone();
    /* 锟矫筹拷 CPU 锟斤拷 guiTask 刷锟斤拷 OLED */
    osDelay(50);
    /* cardPresent=1, 锟截碉拷循锟斤拷锟斤拷锟斤拷: 锟饺达拷锟斤拷位锟斤拷锟斤拷锟斤拷 + 锟斤拷锟斤拷锟斤拷锟诫卡锟斤拷锟? */
  }
  /* USER CODE END StartTaskNFC */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief  USART1 锟斤拷锟秸回碉拷锟斤拷锟斤拷 (锟叫讹拷锟斤拷锟斤拷锟斤拷, 锟斤拷锟斤拷锟劫匡拷锟斤拷锟斤拷双锟斤拷锟斤拷锟斤拷)
 */
static void UART1_RxCallback(UartDrv_RxData_t *pData, void *pUserCtx)
{
  (void)pUserCtx;

  if (!pData || pData->rx_len == 0) return;

  /* 锟桔硷拷写锟斤拷双锟斤拷锟斤拷锟斤拷, 锟斤拷锟绞憋拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷驴锟绞? */
  if (s_uart1_rx_len + pData->rx_len <= sizeof(s_uart1_rx_buf)) {
    memcpy(s_uart1_rx_buf + s_uart1_rx_len, pData->rx_buf, pData->rx_len);
    s_uart1_rx_len += pData->rx_len;
  } else {
    s_uart1_rx_len = 0;
    memcpy(s_uart1_rx_buf, pData->rx_buf, pData->rx_len);
    s_uart1_rx_len = pData->rx_len;
  }

  s_uart1_data_ready = 1;
}

/* USER CODE END Application */

