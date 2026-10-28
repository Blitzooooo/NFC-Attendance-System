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

/* ��֪���� API Key (����?��: https://www.seniverse.com) */
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
#define HEARTBEAT_INTERVAL_MS      5000U    /* �������ͼ��? (ms) */
#define WEATHER_UPDATE_INTERVAL_MS 3600000U /* ����ˢ�¼��? (ms, 1h) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
UartDrv_t g_uart1Drv;
UartDrv_t g_uart6Drv;

/* LED ����: L1~L7 (PE8~PE14), ������, �͵�ƽ���� */
static const Led_Config_t s_ledConfigs[] = {
    {GPIOE, L1_Pin, LED_ON_LOW},
    {GPIOE, L2_Pin, LED_ON_LOW},
    {GPIOE, L3_Pin, LED_ON_LOW},
    {GPIOE, L4_Pin, LED_ON_LOW},
    {GPIOE, L5_Pin, LED_ON_LOW},
    {GPIOE, L6_Pin, LED_ON_LOW},
    {GPIOE, L7_Pin, LED_ON_LOW},
};

/* ��������: K1~K4 ��������(�͵�ƽ��Ч), K5~K6 ��������(�ߵ�ƽ��Ч) */
static const Key_Config_t s_keyConfigs[] = {
    {GPIOE, K1_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K2_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K3_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K4_Pin, KEY_ACTIVE_LOW},
    {GPIOE, K5_Pin, KEY_ACTIVE_HIGH},
    {GPIOE, K6_Pin, KEY_ACTIVE_HIGH},
};

/* USART1 ����˫���� (�ص�д�� �� uartTask ����, �ٽ�������) */
static uint8_t  s_uart1_rx_buf[512];
static uint8_t  s_uart1_proc_buf[512];
static volatile uint16_t s_uart1_rx_len = 0;
static volatile uint8_t  s_uart1_data_ready = 0;

/* USART1 ���ջص����� */
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

  /* --- BSP ������ʼ�� --- */

  /* 1. OLED + GUI ���ȳ�ʼ��, �Ա���ʾ��ʼ������ */
  GUI_Init();
  GUI_Clear();
  GUI_Update();

  /* 2. DS18B20 �¶ȴ����� (�ڲ����� delay_us_init) */
  DISP_ShowInitMsg("Init DS18B20...");
  ds18b20_init();

  /* 3. �������� (USART1 �� �������?, USART6 �� ESP01S) */
  UartDrv_Init(&g_uart1Drv, &huart1);
  UartDrv_SetDebugPort(&g_uart1Drv);
  UartDrv_RegisterRxCb(&g_uart1Drv, UART1_RxCallback, NULL);
  UartDrv_StartRecv(&g_uart1Drv);
  UartDrv_Init(&g_uart6Drv, &huart6);
  UartDrv_StartRecv(&g_uart6Drv);

  /* 4. LED ���� (L1~L7) */
  LED_Init(s_ledConfigs, GUI_COUNTOF(s_ledConfigs));

  /* 5. �������� (K1~K6) */
  Key_Init(s_keyConfigs, GUI_COUNTOF(s_keyConfigs));

  DISP_ShowInitMsg("Init MIDI...");
  /* 6. MIDI ������ (TIM3 CH1 �� PB4) */
  MIDI_Init(&htim3, TIM_CHANNEL_1);

  DISP_ShowInitMsg("Init Flash...");
  /* 7. W25Qxx SPI Flash (SPI1) */
  W25QXX_Init();
  /* ����ϵͳ���� + ��ʼ����¼�� + ������ */
  Config_Load();
  Record_Init();
  Blacklist_Init();

  DISP_ShowInitMsg("Init NFC...");
  /* 8. RC522 NFC (GPIO ģ�� SPI, �ڲ����� delay_us_init �ݵȰ�ȫ) */
  RC522_Platform_Init();
  RC522_ConfigISOType('A');  /* ���� ISO14443A ���������� */

  DISP_ShowInitMsg("Init WiFi...");
  /* 9. ESP01S WiFi ģ�� (�ڲ��Զ�ע�� UartDrv �ص�) */
  ESP01S_Init(&g_uart6Drv);

  DISP_ShowInitMsg("Init RTC...");
  /* 10. RTC �״��ϵ��ʼ����? MX_RTC_Init() ͨ�����ݼĴ����ж����? */

  DISP_ShowInitMsg("Init Done");
  osDelay(300);

  /* --- LOGO 2s --- */
  DISP_ShowLogo();

  /* --- ��Ϣҳ 2s --- */
  DISP_ShowInfoPage();

  /* ֪ͨ guiTask �� networkTask �������? */
  osThreadFlagsSet(guiTaskHandle, 0x01);
  osThreadFlagsSet(otherTaskHandle, 0x01);

  /* Infinite loop */
  for(;;)
  {
    DISP_UpdateTempCache();   /* �첽�����¶Ȼ���, guiTask ���������� OneWire */

    /* OLED ���������? (������ guiTask, ȷ����������ʱҲ�ָܻ�) */
    if (DISP_CheckStuck()) {
      DISP_ResetOLED();
    }

    /* ��������: ÿ 5s ����λ���ϱ��豸״̬ */
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

  /* �ȴ� defaultTask �����������? */
  osThreadFlagsWait(0x01, osFlagsWaitAll, osWaitForever);
  DISP_SetState(DISP_STATE_CLOCK);

  /* Infinite loop */
  for (;;)
  {
    DISP_FeedWatchdog();  /* ÿ��ѭ��ι��, defaultTask �����⿨������λ */
    DispState_t state = DISP_GetState();
    now = osKernelGetTickCount();

    switch (state) {

    /* ===== ʱ�Ӵ��� ===== */
    case DISP_STATE_CLOCK:
      {
        /* ������������������ (���� networkTask �Ķ�����Ϣ) */
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

      /* K5 ����: ����ʱ������ */
      if (Key_IsLongPressed(4)) {
        DISP_SetState(DISP_STATE_TIME_SET);
        stateEnterTick = osKernelGetTickCount();
      }
      break;

    /* ===== ˢ�����? ===== */
    case DISP_STATE_RESULT:
      pCard = DISP_GetCardInfo();
      {
        int countdown = 0;
        int timeoutMs = 3000;
        if (pCard->event == DISP_EVT_CARD_DUP) {
          timeoutMs = 2000;
          int elapsed = (int)(now - stateEnterTick);
          countdown = (elapsed < 2000) ? (2 - elapsed / 1000) : 0;
        }
        DISP_ShowCardResult(pCard, countdown);
      }

      /* �Ȱ����ͳ�ʱ */
      osThreadFlagsWait(DISP_EVT_KEY_PRESSED, osFlagsWaitAny, DISP_FRAME_INTERVAL_MS);

      /* ���ⰴ�� �� ����ʱ�� */
      if (Key_AnyPressed()) {
        Key_ClearAllEvents();
        DISP_SetState(DISP_STATE_CLOCK);
        break;
      }

      /* ��ʱ �� ����ʱ�� (�ظ�ˢ�� 2s, ���� 3s) */
      {
        int timeoutMs = (pCard->event == DISP_EVT_CARD_DUP) ? 2000 : 3000;
        if (now - stateEnterTick >= (uint32_t)timeoutMs) {
          DISP_SetState(DISP_STATE_CLOCK);
        }
      }
      break;

    /* ===== ʱ������ ===== */
    case DISP_STATE_TIME_SET:
      DISP_ShowTimeSet();

      /* ������ */
      for (i = 0; i < 6; i++) {
        if (Key_IsShortPressed(i))
          DISP_TimeSetKey(i, 1, 0);
        if (Key_IsLongPressed(i))
          DISP_TimeSetKey(i, 0, 1);
      }

      /* ��һ֡����Ƿ����˳�? */
      osThreadFlagsWait(DISP_EVT_KEY_PRESSED, osFlagsWaitAny, DISP_FRAME_INTERVAL_MS);
      osThreadFlagsClear(DISP_EVT_KEY_PRESSED);

      /* 60 �볬ʱ�Զ��˳� */
      if (now - stateEnterTick >= 60000) {
        DISP_SetState(DISP_STATE_CLOCK);
      }
      break;

    /* ===== ����Ա���� ===== */
    case DISP_STATE_ADMIN_SET:
      DISP_ShowAdminSet();

      /* ������ */
      for (i = 0; i < 6; i++) {
        if (Key_IsShortPressed(i))
          DISP_AdminSetKey(i, 1, 0);
        if (Key_IsLongPressed(i))
          DISP_AdminSetKey(i, 0, 1);
      }

      /* ��һ֡����Ƿ����˳�? */
      osThreadFlagsWait(DISP_EVT_KEY_PRESSED, osFlagsWaitAny, DISP_FRAME_INTERVAL_MS);
      osThreadFlagsClear(DISP_EVT_KEY_PRESSED);

      /* 120 �볬ʱ�Զ��˳� */
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
    Key_Scan();         /* ����ɨ��, ��ÿ 10ms ����һ�� */
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
    /* ���? USART1 �Ƿ��������� (˫����: �ص�д�� �� �ٽ������� �� ����) */
    if (s_uart1_data_ready) {
      uint16_t len;
      taskENTER_CRITICAL();
      len = s_uart1_rx_len;
      memcpy(s_uart1_proc_buf, s_uart1_rx_buf, len);
      s_uart1_rx_len = 0;
      s_uart1_data_ready = 0;
      taskEXIT_CRITICAL();

      /* ��������׷�ӵ��л��� (��ʱ�ص���ͬʱд�� s_uart1_rx_buf) */
      for (uint16_t i = 0; i < len && lineLen < (int)sizeof(lineBuf) - 1; i++) {
        char ch = (char)s_uart1_proc_buf[i];
        lineBuf[lineLen++] = ch;
        /* �������� �� ִ������ */
        if (ch == '\n' || lineLen >= (int)sizeof(lineBuf) - 1) {
          lineBuf[lineLen] = '\0';
          /* ʱ��ͬ���ڼ�ܾ���������? */
          if (DISP_IsTimeSyncBusy()) {
            snprintf(respBuf, sizeof(respBuf), "ERR:SYNC\r\n");
          } else {
            NFC_ProcessCommand(lineBuf, respBuf, sizeof(respBuf));
          }
          UartDrv_SendStr(&g_uart1Drv, respBuf);
          NFC_FlushCmdNotify();  /* �ȷ���Ӧ��֪ͨ nfcTask, ��֤�����������? */
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
/* ������ѯ��������: ������֪���� API ��������ʾ���� */
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

  /* �ȴ� defaultTask ���Ӳ����ʼ�� */
  osThreadFlagsWait(0x01, osFlagsWaitAll, osWaitForever);

  /* ===== �״� WiFi ���� + NTP ��ʱ + ���� ===== */
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

  /* ===== �״�������ѯ ===== */
  {
    UpdateWeather();
  }

  /* ===== ��ѭ��: �������� + ��ʱ����ˢ�� ===== */
  for(;;)
  {
    static uint32_t s_lastWeatherUpdate = 0;
    static uint32_t s_lastReconnectTry  = 0;
    uint32_t nowTick = osKernelGetTickCount();

    /* --- �����Զ����� (ÿ 120s) --- */
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

    /* --- ÿ 1h ˢ������ --- */
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

  #define CARD_MISS_THRESHOLD    2     /* ���� 2 ��δ���? �� ���뿨 (2��200ms=400ms) */
  #define CARD_SCAN_INTERVAL_MS  200   /* Ѱ�����? */
  #define CARD_DEDUP_TIMEOUT_MS  3000  /* ���ظ���ʱ, �� OLED ����? 3s ͬ�� */
  #define CARD_STABLE_DELAY_MS   300   /* ��⵽�����ȶ��ӳ�?, ȷ����Ƭ��ȫ������Ƶ�� */

  /* Infinite loop */
  for (;;)
  {
    /* ʱ��ͬ���ڼ���ͣˢ�� */
    if (DISP_IsTimeSyncBusy()) {
      osDelay(CARD_SCAN_INTERVAL_MS);
      continue;
    }

    /* ===== �п�: �ȴ���λ������ + �����Լ����? (������) ===== */
    if (cardPresent) {
      /* ��λ���Ƿ������һ������? (����/����/����)? */
      if (NFC_IsCmdDone()) {
        NFC_ClearCmdDone();

        /* ����Ƿ����?ȫ���ض� (UPDATEIMG ��λͼ�ѱ�) */
        if (NFC_IsFullRereadNeeded()) {
          NFC_ClearFullReread();

          /* ȫ���ض�: ��ͼ������, ˢ��λͼ���� */
          if (!NFC_IsReaderLocked()) {
            while (!NFC_LockReader()) { osDelay(10); }
            nfcEvent = NFC_ReadCard(cardID, &cardData);
            NFC_UnlockReader();

            if (nfcEvent != NFC_EVT_INVALID) {
              /* λͼ��ˢ��, ֪ͨ GUI ������ʾ */
              DISP_SendNfcEvent(myQueue04Handle,  DISP_EVT_CARD_VALID, cardID, 4,
                                   cardData.cardId, cardData.sid, cardData.points,
                                   cardData.cardType, cardData.statusFlag,
                                   cardData.swipeDT.hour,
                                   cardData.swipeDT.minute,
                                   cardData.swipeDT.second,
                                   0, 0, 0);
            }
            /* �ó� CPU �� guiTask ˢ�� OLED */
            osDelay(50);
            NFC_Recover();
          }
        } else {
          /* �����ض�: ���� Block1 �˻�ͷ (λͼδ��) */
          if (!NFC_IsReaderLocked()) {
            while (!NFC_LockReader()) { osDelay(10); }
            nfcEvent = NFC_ReadCardHeader(cardID, &cardData);
            NFC_UnlockReader();

            if (nfcEvent != NFC_EVT_INVALID) {
              /* ֪ͨ GUI ��ʾд�����? */
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
        /* �Ѵ����� CmdDone, ��������������Ѱ��, �� GUI ��ʱ����Ⱦ */
        osDelay(50);
        continue;
      }

      /* �����Լ��?�Ƿ��� (������: ���� 3 ��ʧ�ܲ����뿨) */
      if (NFC_IsReaderLocked()) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;  /* uartTask ���ڲ���, �������ò� */
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

    /* ===== �޿�: ����Ѱ�� ===== */
    {
      char status;
      /* �����λ�����ڲ���?, ֱ������������ */
      if (NFC_IsReaderLocked()) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;
      }
      /* ��������������, ����λ��ռ�������������� */
      if (!NFC_LockReader()) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;
      }
      status = NFC_ScanCard(cardID);
      NFC_UnlockReader();  /* Ѱ����������ͷ���? */
      if (status != RC522_OK) {
        osDelay(CARD_SCAN_INTERVAL_MS);
        continue;
      }
    }

    /* �����?: ����Ƿ��?�մ�������?һ�ſ� (��λ������������) */
    {
      uint32_t uid32 = ((uint32_t)cardID[0] << 24) |
                       ((uint32_t)cardID[1] << 16) |
                       ((uint32_t)cardID[2] << 8)  |
                        (uint32_t)cardID[3];
      if (uid32 == lastUID && lastUID != 0) {
        /* ͬһ�ſ�����, ����ͷ(��ˢ��λͼ, ���⸲�� GUI ������ʾ��ͼ��) */
        cardPresent = 1;
        missCount   = 0;
        LED_On(0);

        /* �ȶ��ӳٲ�����; while ѭ���Զ��ȴ���λ���ͷ��� */
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

    /* �¿��״�ˢ��: �������� */
    cardPresent = 1;
    missCount   = 0;
    LED_On(0);

    /* ===== ȥ��: ͬһ�ſ��ڽ��ҳ��ʾ�ڼ�? (3s) �ڲ��ظ����� ===== */
    {
      uint32_t uid32 = ((uint32_t)cardID[0] << 24) |
                       ((uint32_t)cardID[1] << 16) |
                       ((uint32_t)cardID[2] << 8)  |
                        (uint32_t)cardID[3];
      if (uid32 == lastUID && lastUID != 0) {
        uint32_t elapsed = osKernelGetTickCount() - lastNotifyTick;
        if (elapsed < CARD_DEDUP_TIMEOUT_MS) {
          /* 3s ��ͬһ�ſ�: �ظ�ˢ�� (�� WaitCardOff ����) */
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
        /* ��ʱ, �����ٴ�ˢ��, ������������ */
      }
      lastUID = uid32;
    }

    /* Ѱ����: �ϸ������� */
    MIDI_Beep(6, 50);

    /* �ȴ���Ƭ����Ƶ�����ȶ� (�������?) */
    osDelay(CARD_STABLE_DELAY_MS);

    /* ���¼�����ȡ���� (����λ��ռ���� while �Զ��ȴ�) */
    while (!NFC_LockReader()) { osDelay(50); }

    /* ===== ����: ��ȡ������ =====
     * GUI ������ʾ���ʱ�����?, ���⸲��λͼ���»���/���� */
    if (DISP_GetState() == DISP_STATE_RESULT) {
      nfcEvent = NFC_ReadCardHeader(cardID, &cardData);
    } else {
      nfcEvent = NFC_ReadCard(cardID, &cardData);
    }

    /* �������?, �ͷ� RC522 �� */
    NFC_UnlockReader();

    {
      uint8_t  evtType   = 0;
      uint8_t  recStatus = 0;
      uint32_t durSec    = 0;
      uint32_t uidNfc    = ((uint32_t)cardID[0] << 24) | ((uint32_t)cardID[1] << 16) |
                            ((uint32_t)cardID[2] << 8)  |  (uint32_t)cardID[3];

    /* ===== ����: LED + ���� + �¼����� ===== */
    switch (nfcEvent) {
    case NFC_EVT_ADMIN:
      /* ����Ա��: L1+L3 ��, �ϸ�������, ���������ж� */
      dispEvent = DISP_EVT_CARD_ADMIN;
      LED_On(2);
      MIDI_Beep(6, 150);
      break;

    case NFC_EVT_VALID:
    {
      /* ===== ���������?: ��ʧ���ܾ� ===== */
      if (Blacklist_IsBlocked(uidNfc)) {
        printf("[BL] BLOCKED UID=%08lX, count=%u\r\n",
               (unsigned long)uidNfc, (unsigned)Blacklist_GetCount());
        dispEvent = DISP_EVT_CARD_INVALID;
        cardData.statusFlag = 0xFF;  /* ����?��ʧ, OLED ��ʾ"�ѹ�ʧ" */
        MIDI_Beep(3, 100);
        osDelay(100);
        MIDI_Beep(3, 100);
        /* �ϱ���ʧ����¼ */
        printf("REC:UID=%08lX|SID=%lu|LOST\r\n",
               (unsigned long)uidNfc, (unsigned long)cardData.sid);
        /* дһ�� LOST ��¼�� Flash */
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

      /* ===== ����Աģʽ�¾ܾ���ͨ�� ===== */
      if (DISP_GetState() == DISP_STATE_ADMIN_SET) {
        /* ����������ʾ�ܾ�, ��д��¼, ����LED, ��֪ͨGUI */
        dispEvent = DISP_EVT_NONE;
        MIDI_Beep(3, 60);
        osDelay(60);
        MIDI_Beep(3, 60);
        break;
      }

      /* ===== �����ж� ===== */
      const DispAdminCfg_t *pCfg = DISP_GetAdminCfg();
      uint8_t  mode      = pCfg->attendMode;
      uint8_t  ledMask   = 0;

      /* ��ѯ���� (������ UID) */
      uint32_t enterSecs = 0;
      uint8_t cacheState = Cache_Find(uidNfc, &enterSecs);

      /* ����δ������ɨ Flash */
      if (cacheState == CACHE_MISS) {
        SwipeRecord_t lastRec;
        if (Record_QueryLast(uidNfc, &lastRec)) {
          cacheState = lastRec.eventType;  /* 0=�볡 1=�볡 */
          if (cacheState == 0) {
            /* �Ӽ�¼�ؽ� enterSecs: ��ˢ��ʱ�̹��� (�뼶���ȿɽ���) */
            BSP_RTC_DateTime_t dt;
            dt.year = 2000 + lastRec.year;
            dt.month = lastRec.month; dt.day = lastRec.day;
            dt.hour = lastRec.hour; dt.minute = lastRec.minute; dt.second = lastRec.second;
            enterSecs = DISP_DateTimeToSeconds(&dt);
          }
        }
      }

      /* ��ģʽ�ж� */
      switch (mode) {
      case 0: /* ���ģ�? */
        if (cacheState == 0) {
          /* ���볡 �� ��Ч�ظ� */
          evtType = 1; recStatus = 1;
        } else {
          /* �볡 */
          evtType = 0; recStatus = 0; ledMask = 0x08;  /* L4 */
        }
        break;

      case 1: /* ����ģʽ */
        if (cacheState == 0) {
          /* ���볡 �� �볡 + ��ʱ�� (RTC ���찲ȫ) */
          evtType = 1; recStatus = 0; ledMask = 0x10;  /* L5 */
          /* �� RTC ��ǰʱ�����ʱ��?, ��Ȼ֧�ֿ��� */
          {
            BSP_RTC_DateTime_t nowDT;
            BSP_RTC_GetDateTime(&nowDT);
            uint32_t nowSecs = DISP_DateTimeToSeconds(&nowDT);
            durSec = (nowSecs > enterSecs) ? (nowSecs - enterSecs) : 0;
          }
        } else {
          /* ���볡��¼ */
          evtType = 1; recStatus = 2;
        }
        break;

      case 2: /* �����ģ�? */
      default:
        if (cacheState == 0) {
          /* ���볡 �� �볡 + ��ʱ�� (RTC ���찲ȫ) */
          evtType = 1; recStatus = 0; ledMask = 0x40;  /* L7 */
          {
            BSP_RTC_DateTime_t nowDT;
            BSP_RTC_GetDateTime(&nowDT);
            uint32_t nowSecs = DISP_DateTimeToSeconds(&nowDT);
            durSec = (nowSecs > enterSecs) ? (nowSecs - enterSecs) : 0;
          }
        } else {
          /* �볡 */
          evtType = 0; recStatus = 0; ledMask = 0x20;  /* L6 */
        }
        break;
      }

      /* ���»��� (�볡ʱ��¼ RTC ��ʱ���?) */
      if (recStatus == 0) {
        uint32_t secs = 0;
        if (evtType == 0) {
          /* �볡: ��¼��ǰ RTC ��ʱ���������?, ���볡ʱ�������ʱ��? */
          BSP_RTC_DateTime_t nowDT;
          BSP_RTC_GetDateTime(&nowDT);
          secs = DISP_DateTimeToSeconds(&nowDT);
        }
        Cache_Update(uidNfc, evtType, secs);
      }

      /* д SPI Flash ��¼ */
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

      /* LED + ���� */
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
      /* ��Ч��: ����Ӳ���ָ� RC522, ��ֹоƬ״̬����Ӱ�����Ѱ��? */
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

    /* ===== ֪ͨ GUI ===== */
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
    /* �ó� CPU �� guiTask ˢ�� OLED */
    osDelay(50);
    /* cardPresent=1, �ص�ѭ������: �ȴ���λ������ + �������뿨���? */
  }
  /* USER CODE END StartTaskNFC */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
 * @brief  USART1 ���ջص����� (�ж�������, �����ٿ�����˫������)
 */
static void UART1_RxCallback(UartDrv_RxData_t *pData, void *pUserCtx)
{
  (void)pUserCtx;

  if (!pData || pData->rx_len == 0) return;

  /* �ۼ�д��˫������, ���ʱ�������������¿��? */
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

