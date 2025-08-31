#pragma once

/* ========= 빌드 모드 스위치 =========
 * 한 번에 하나만 1로! (BEGINNER / MID / HIGH)
 */
#define BEGINNER_MODE    0   /* 초급: while( ) 루프  */
#define MID_LEVEL_RTOS   1   /* 중급: 기능별 RTOS 태스크 */
#define HIGH_LEVEL_RTOS  0   /* 고급: 1ms/5ms 주기 태스크 */

/* ========= 공통 동작 파라미터 ========= */
#define UART_HEARTBEAT_MS     1000     /* UART 주기(중급/기본) */
#define SPI_BACKUP_MS         5000     /* EEPROM 백업 주기     */
#define I2C_FAULT_POLL_MS      500     /* PMIC fault 폴링 주기  */
#define CAN_QUEUE_DEPTH          8

/* ========= 고급(1ms/5ms) 설정 ========= */
#define TASK_1MS_PERIOD_MS        1
#define TASK_5MS_PERIOD_MS        5

/* ADC 스케일(보드 분압 저항값에 맞게 조정) */
#define VREF_mV                3300u
#define ADC_FULL_SCALE         4095u
#define R_TOP_OHM            100000u
#define R_BOT_OHM             10000u
#define VOUTA_DEF_mV           1200u
#define VOUT_TOLERANCE_mV       100u

/* ========= 테스트 케이스 스위치 =========
 * 1로 켜면 해당 TC가 컴파일되어 자동 수행됨
 */
#define TESTCASE_REG_PARSE   0  /* WhiteBox-1: 레지스터 파서 */
#define TESTCASE_VOUT_SET    0  /* WhiteBox-2: VOUT 설정 */
#define TESTCASE_E2E         0  /* BlackBox-1: E2E(UV→EEPROM→UDS) */
#define TESTCASE_NOFAULT     0  /* BlackBox-2: 무이상 경로 */

/* ========= 빌드 안전장치(동시 활성화 방지) ========= */
#if ((BEGINNER_MODE + MID_LEVEL_RTOS + HIGH_LEVEL_RTOS) != 1)
#error "빌드 모드 스위치는 BEGINNER_MODE, MID_LEVEL_RTOS, HIGH_LEVEL_RTOS 중 하나만 1이어야 합니다."
#endif
