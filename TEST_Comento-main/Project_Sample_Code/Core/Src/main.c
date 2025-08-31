/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (BEGINNER / MID / HIGH selectable)
  ******************************************************************************
  * @attention
  * © 2025 STMicroelectronics. BSD 3-Clause license
  ******************************************************************************
  */
/* 공통 흐름
====================================================================================
① Boot → HAL_Init → SystemClock_Config
② MX_GPIO/DMA/ADC/CAN/I2C/SPI/UART 초기화
③ DTC_InitFromEEPROM()으로 기존 DTC 래칭 복구
④ HAL_CAN_Start() / HAL_CAN_ActivateNotification() (수신 인터럽트)
⑤ (BEGINNER) while: I2C→SPI→CAN→UART 순환
   (MID) 기능별 태스크: I2C/EEPROM/CAN/UART
   (HIGH) 1ms/5ms 태스크: 1ms=I2C/SPI/CAN, 5ms=ADC/UART & 전압 DTC
⑥ DTC는 EEPROM 저장, CAN(UDS/OBD) 응답, UART heartbeat
====================================================================================*/
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include <string.h>

/* === 프로젝트 설정 & 모듈 === */
#include "build_config.h"         /* 모드 스위치/주기 상수 */
#include "pmic_map.h"
#include "pmic_mp5475.h"
#include "eeprom_25lc256.h"
#include "dtc_store.h"
#include "can_if.h"
#include "testcases.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_UART4_Init(void);
void StartDefaultTask(void *argument);

/* CAN 상태 프레임 송신 헬퍼 */
static void CAN_SendStatus_Snapshot(const uint8_t *payload8);
/* DTC 전체 클리어 헬퍼 */
static void DTC_ClearAll(void);

/* === (공통) DMA 완료 플래그 & 콜백 === */
volatile uint8_t g_i2c_done = 0;
volatile uint8_t g_spi_done = 0;

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c){ (void)hi2c; g_i2c_done = 1; }
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c){ (void)hi2c; g_i2c_done = 1; }
void HAL_SPI_TxRxCpltCallback (SPI_HandleTypeDef *hspi){ (void)hspi; g_spi_done = 1; }
void HAL_SPI_TxCpltCallback   (SPI_HandleTypeDef *hspi){ (void)hspi; g_spi_done = 1; }
void HAL_SPI_RxCpltCallback   (SPI_HandleTypeDef *hspi){ (void)hspi; g_spi_done = 1; }

/* === (MID 전용) 기능별 태스크 선언 === */
#if MID_LEVEL_RTOS
void StartI2CTask(void *argument);
void StartSPITask(void *argument);
void StartCANTask(void *argument);
void StartUARTTask(void *argument);
#endif

/* === (HIGH 전용) 1ms/5ms 태스크 선언 & 헬퍼 === */
#if HIGH_LEVEL_RTOS
void StartTask1ms(void *argument);
void StartTask5ms(void *argument);
/* ADC → 분압 환산 → mV */
static uint16_t adc_read_mv(void)
{
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) return 0;
  uint32_t raw = HAL_ADC_GetValue(&hadc1);
  uint32_t vadc_mv = (raw * (uint32_t)VREF_mV) / (uint32_t)ADC_FULL_SCALE;
  uint32_t vout_mv = (vadc_mv * (R_TOP_OHM + R_BOT_OHM)) / R_BOT_OHM; /* 분압 복원 */
  return (uint16_t)(vout_mv & 0xFFFF);
}
#endif

/* === (공통) CAN 수신 인터럽트/UDS 처리 === */
#if !BEGINNER_MODE
void Process_CAN_Response(uint8_t *data); /* forward */
#endif

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
CAN_HandleTypeDef hcan1;
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;
DMA_HandleTypeDef hdma_i2c2_rx;
DMA_HandleTypeDef hdma_i2c2_tx;
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;
DMA_HandleTypeDef hdma_spi2_rx;
DMA_HandleTypeDef hdma_spi2_tx;
UART_HandleTypeDef huart4;

/* RTOS objects --------------------------------------------------------------*/
/* Default task */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal,
};

/* (MID) 기능별 태스크 */
#if MID_LEVEL_RTOS
osThreadId_t I2CTaskHandle;
const osThreadAttr_t I2CTask_attributes = {
  .name = "I2CTask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal,
};
osThreadId_t SPITaskHandle;
const osThreadAttr_t SPITask_attributes = {
  .name = "SPITask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal,
};
osThreadId_t CANTaskHandle;
const osThreadAttr_t CANTask_attributes = {
  .name = "CANTask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal,
};
osThreadId_t UARTTaskHandle;
const osThreadAttr_t UARTTask_attributes = {
  .name = "UARTTask", .stack_size = 128 * 4, .priority = (osPriority_t) osPriorityNormal,
};
#endif

/* (HIGH) 1ms/5ms 태스크 */
#if HIGH_LEVEL_RTOS
osThreadId_t Task1msHandle;
const osThreadAttr_t Task1ms_attributes = {
  .name = "Task1ms", .stack_size = 256 * 4, .priority = (osPriority_t)osPriorityHigh
};
osThreadId_t Task5msHandle;
const osThreadAttr_t Task5ms_attributes = {
  .name = "Task5ms", .stack_size = 256 * 4, .priority = (osPriority_t)osPriorityNormal
};
#endif

/* Queue & Mutex */
osMessageQueueId_t CanQueueHandle;
const osMessageQueueAttr_t CanQueue_attributes = { .name = "CanQueue" };
osMutexId_t CommMutexHandleHandle;
const osMutexAttr_t CommMutexHandle_attributes = { .name = "CommMutexHandle" };

/* CAN 수신 버퍼 */
CAN_RxHeaderTypeDef RxHeader;

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Application entry ---------------------------------------------------------*/
int main(void)
{
  /* HAL/Clock/Peripherals */
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_CAN1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_UART4_Init();

  /* 부팅 시 DTC 래칭 복구 */
  DTC_InitFromEEPROM();

  /* CAN 시작 + RX 인터럽트 */
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

#if BEGINNER_MODE
  /* ========== [BEGINNER: 무RTOS 메인 루프] ========== */
  pmic_set_voutA_mv(VOUTA_DEF_mV);  /* 요구사항 5 예시 */

  while (1)
  {
    /* 1) I2C(DMA): PMIC 8바이트 스냅샷 + UV 확인 */
    pmic_status8_t st = {0};
    pmic_read_status8(&st);
    /* 필요 시 단일 바이트 폴링으로 UV 확인 (예: reg 0x07, bit0) */
    uint8_t fault1 = 0;
    HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_FAULT_STATUS1_REG,
                     I2C_MEMADD_SIZE_8BIT, &fault1, 1, HAL_MAX_DELAY);
    if ((fault1 & 0x01) && !DTC_IsActive(DTC_BRAKE_UV)) {
      DTC_Set(DTC_BRAKE_UV);
      DTC_CommitEEPROM();
    }

    /* 2) SPI(DMA): 스냅샷 EEPROM 백업 & 재읽기 검증 */
    uint8_t back[8] = {0};
    ee_write_dma(0x0000, st.raw, sizeof(st.raw));
    ee_read_dma (0x0000, back,   sizeof(back));

    /* 3) CAN: 상태 프레임 송신 */
    CAN_SendStatus_Snapshot(st.raw);

    /* 4) UART: Heartbeat (Polling) */
    const char ok[] = "System OK\r\n";
    HAL_UART_Transmit(&huart4, (uint8_t*)ok, sizeof(ok)-1, HAL_MAX_DELAY);

    HAL_Delay(10);
  }

#else  /* !BEGINNER_MODE => RTOS 경로 */

  /* Init scheduler & sync objects */
  osKernelInitialize();
  CommMutexHandleHandle = osMutexNew(&CommMutexHandle_attributes);
  CanQueueHandle = osMessageQueueNew(CAN_QUEUE_DEPTH, 8, &CanQueue_attributes);

  /* 공통 Default task */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

#if MID_LEVEL_RTOS
  /* ===== 중급: 기능별 태스크 ===== */
  I2CTaskHandle  = osThreadNew(StartI2CTask,  NULL, &I2CTask_attributes);
  SPITaskHandle  = osThreadNew(StartSPITask,  NULL, &SPITask_attributes);
  CANTaskHandle  = osThreadNew(StartCANTask,  NULL, &CANTask_attributes);
  UARTTaskHandle = osThreadNew(StartUARTTask, NULL, &UARTTask_attributes);

#elif HIGH_LEVEL_RTOS
  /* ===== 고급: 1ms / 5ms 주기 태스크 ===== */
  Task1msHandle = osThreadNew(StartTask1ms, NULL, &Task1ms_attributes);
  Task5msHandle = osThreadNew(StartTask5ms, NULL, &Task5ms_attributes);
#endif

  /* Start scheduler */
  osKernelStart();
  while (1) { /* never reached */ }

#endif /* BEGINNER_MODE */
}

/* ====================== System Peripherals Init ====================== */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) { Error_Handler(); }
}

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) { Error_Handler(); }

  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) { Error_Handler(); }
}

static void MX_CAN1_Init(void)
{
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 16;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_1TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK) { Error_Handler(); }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) { Error_Handler(); }
}

static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK) { Error_Handler(); }
}

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) { Error_Handler(); }
}

static void MX_SPI2_Init(void)
{
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) { Error_Handler(); }
}

static void MX_UART4_Init(void)
{
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK) { Error_Handler(); }
}

static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
  HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /* 기본 출력 High */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_SET);

  /* PB2 (EEPROM CS) : Push-Pull */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   /* Push-Pull */
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET); /* CS idle High */
}

/* ====================== Tasks & Callbacks ====================== */

static void CAN_SendStatus_Snapshot(const uint8_t *payload8)
{
  CAN_TxHeaderTypeDef th = {0}; uint32_t mb = 0;
  th.StdId = 0x7E8; th.IDE = CAN_ID_STD; th.RTR = CAN_RTR_DATA; th.DLC = 8;
  HAL_CAN_AddTxMessage(&hcan1, &th, (uint8_t*)payload8, &mb);
}

void StartDefaultTask(void *argument)
{
  for(;;) { osDelay(1); }
}

/* --- CAN RX 인터럽트: 큐에 투입 (RTOS 모드 공통) --- */
#if !BEGINNER_MODE
uint8_t RxData[8];
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData);
  osMessageQueuePut(CanQueueHandle, RxData, 0, 0);
}

/* UDS/OBD 응답 처리: 새 DTC API 사용 */
void Process_CAN_Response(uint8_t *data)
{
  CAN_TxHeaderTypeDef TxHeader;
  uint32_t TxMailbox;
  uint8_t TxData[8] = {0};

  TxHeader.StdId = 0x7E8;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.DLC = 8;

  dtc_code_t list[4]; size_t n = DTC_GetActiveList(list, 4);

  /* OBD2 0x43: Read DTCs (예시: 1개만 응답) */
  if (data[1] == 0x43) {
    if (n > 0) {
      TxData[0] = 0x03; TxData[1] = 0x43;
      TxData[2] = (list[0] >> 8) & 0xFF;
      TxData[3] = (list[0]     ) & 0xFF;
    } else {
      TxData[0] = 0x01; TxData[1] = 0x43; TxData[2] = 0x00;
    }
  }
  /* OBD2 0x04: Clear DTCs */
  else if (data[1] == 0x04) {
    DTC_ClearAll();
    DTC_CommitEEPROM();
    TxData[0] = 0x01; TxData[1] = 0x44;
  }
  /* UDS 0x19: Read DTCs (간단 응답) */
  else if (data[1] == 0x19) {
    if (n > 0) {
      TxData[0] = 0x03; TxData[1] = 0x59; TxData[2] = 0x02;
      TxData[3] = (list[0] >> 8) & 0xFF;
      TxData[4] = (list[0]     ) & 0xFF;
    } else {
      TxData[0] = 0x01; TxData[1] = 0x59; TxData[2] = 0x00;
    }
  }
  /* UDS 0x14: Clear DTCs */
  else if (data[1] == 0x14) {
    DTC_ClearAll();
    DTC_CommitEEPROM();
    TxData[0] = 0x02; TxData[1] = 0x54;
  }
  else {
    /* MISRA placeholder */
  }

  HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox);
}
#endif /* !BEGINNER_MODE */

/* ====================== (MID) 기능별 Tasks ====================== */
#if MID_LEVEL_RTOS
void StartI2CTask(void *argument)
{
  uint8_t faultReg = 0;
  for(;;) {
    osMutexAcquire(CommMutexHandleHandle, osWaitForever);
    HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_FAULT_STATUS1_REG,
                     I2C_MEMADD_SIZE_8BIT, &faultReg, 1, HAL_MAX_DELAY);
    osMutexRelease(CommMutexHandleHandle);

    if ((faultReg & 0x01) && !DTC_IsActive(DTC_BRAKE_UV)) {
      DTC_Set(DTC_BRAKE_UV);
      DTC_CommitEEPROM();
    }
    osDelay(I2C_FAULT_POLL_MS);
  }
}

void StartSPITask(void *argument)
{
  /* 단순 주기 백업 (원하면 상태 스냅샷도 함께 저장 가능) */
  for(;;) {
    osMutexAcquire(CommMutexHandleHandle, osWaitForever);
    DTC_CommitEEPROM();
    osMutexRelease(CommMutexHandleHandle);
    osDelay(SPI_BACKUP_MS);
  }
}

void StartCANTask(void *argument)
{
  uint8_t rxBuf[8];
  for(;;) {
    if (osMessageQueueGet(CanQueueHandle, rxBuf, NULL, osWaitForever) == osOK) {
      osMutexAcquire(CommMutexHandleHandle, osWaitForever);
      Process_CAN_Response(rxBuf);
      osMutexRelease(CommMutexHandleHandle);
    }
    osDelay(1);
  }
}

void StartUARTTask(void *argument)
{
  const char msg[] = "ECU System Running\r\n";
  for(;;) {
    osMutexAcquire(CommMutexHandleHandle, osWaitForever);
    HAL_UART_Transmit(&huart4, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    osMutexRelease(CommMutexHandleHandle);
    osDelay(UART_HEARTBEAT_MS);
  }
}
#endif /* MID_LEVEL_RTOS */

/* ====================== (HIGH) 1ms/5ms Tasks ====================== */
#if HIGH_LEVEL_RTOS
void StartTask1ms(void *argument)
{
  uint32_t next = osKernelGetTickCount();
  uint32_t ms_counter = 0;
  uint8_t  rxBuf[8];

  for(;;) {
    next += TASK_1MS_PERIOD_MS;

    /* 1) I2C: PMIC fault 빠른 폴링 */
    uint8_t fault1 = 0;
    osMutexAcquire(CommMutexHandleHandle, osWaitForever);
    HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_FAULT_STATUS1_REG,
                     I2C_MEMADD_SIZE_8BIT, &fault1, 1, HAL_MAX_DELAY);
    osMutexRelease(CommMutexHandleHandle);
    if ((fault1 & 0x01) && !DTC_IsActive(DTC_BRAKE_UV)) {
      DTC_Set(DTC_BRAKE_UV);
      DTC_CommitEEPROM();
    }

    /* 2) SPI: 10ms마다 간단 백업 */
    if ((ms_counter % 10) == 0) {
      osMutexAcquire(CommMutexHandleHandle, osWaitForever);
      DTC_CommitEEPROM();
      osMutexRelease(CommMutexHandleHandle);
    }

    /* 3) CAN: 10ms마다 상태 송신 (예: PMIC 8바이트 스냅샷) */
    if ((ms_counter % 10) == 0) {
      pmic_status8_t st = {0};
      pmic_read_status8(&st);
      CAN_SendStatus_Snapshot(st.raw);
    }

    /* 4) CAN 수신 큐 드레인(논블로킹) */
    for (int i = 0; i < 2; ++i) {
      if (osMessageQueueGet(CanQueueHandle, rxBuf, NULL, 0) == osOK) {
        osMutexAcquire(CommMutexHandleHandle, osWaitForever);
        Process_CAN_Response(rxBuf);
        osMutexRelease(CommMutexHandleHandle);
      } else { break; }
    }

    ms_counter++;
    osDelayUntil(next);
  }
}

void StartTask5ms(void *argument)
{
  uint32_t next = osKernelGetTickCount();
  uint32_t acc_ms = 0;
  const uint16_t low_th  = (uint16_t)(VOUTA_DEF_mV - VOUT_TOLERANCE_mV);
  const uint16_t high_th = (uint16_t)(VOUTA_DEF_mV + VOUT_TOLERANCE_mV);

  for(;;) {
    next += TASK_5MS_PERIOD_MS;

    /* ADC: PMIC Vout 모니터링 → 스펙 밖이면 DTC 셋 */
    uint16_t vout_mv = adc_read_mv();
    if ((vout_mv < low_th) || (vout_mv > high_th)) {
      if (!DTC_IsActive(DTC_ADC_VOUT_ABN)) {
        DTC_Set(DTC_ADC_VOUT_ABN);
        osMutexAcquire(CommMutexHandleHandle, osWaitForever);
        DTC_CommitEEPROM();
        osMutexRelease(CommMutexHandleHandle);
      }
    }

    /* UART heartbeat (1s 분주) */
    acc_ms += TASK_5MS_PERIOD_MS;
    if (acc_ms >= UART_HEARTBEAT_MS) {
      acc_ms = 0;
      const char msg[] = "ECU OK (5ms task)\r\n";
      osMutexAcquire(CommMutexHandleHandle, osWaitForever);
      HAL_UART_Transmit(&huart4, (uint8_t*)msg, sizeof(msg)-1, HAL_MAX_DELAY);
      osMutexRelease(CommMutexHandleHandle);
    }

    osDelayUntil(next);
  }
}
#endif /* HIGH_LEVEL_RTOS */

/* ====================== DTC Helper ====================== */
static void DTC_ClearAll(void)
{
  DTC_Clear(DTC_BRAKE_UV);
  DTC_Clear(DTC_ADC_VOUT_ABN);
  DTC_Clear(DTC_GATE_OCP);
  DTC_Clear(DTC_SENSOR_MAG);
}

/* ====================== Error / Assert ====================== */
void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file; (void)line;
}
#endif /* USE_FULL_ASSERT */
