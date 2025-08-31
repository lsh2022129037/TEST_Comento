#pragma once
#include <stdint.h>
#include <stdbool.h>

/* 공통 DTC 코드 (필요시 추가) */
typedef enum {
  DTC_NONE          = 0x0000,
  DTC_BRAKE_UV      = 0x1234, // PMIC UV
  DTC_GATE_OCP      = 0x1235, // DRV8323 OCP
  DTC_SENSOR_MAG    = 0x1236, // AS5600 Magnet fault
  DTC_ADC_VOUT_ABN  = 0x1237, // ADC Vout abnormal
} dtc_code_t;

/* 퍼시스턴트 저장 포맷(간단 비트마스크) */
typedef struct __attribute__((packed)) {
  uint16_t magic;        // 0xD7C1
  uint16_t active_mask;  // 각 DTC를 비트로 래칭
} dtc_persist_t;

/* API */
void     DTC_InitFromEEPROM(void);     // 부팅 시 1회 호출
void     DTC_Set(dtc_code_t code);     // 고장 세트(래칭)
void     DTC_Clear(dtc_code_t code);   // 고장 클리어
bool     DTC_IsActive(dtc_code_t code);
uint16_t DTC_GetActiveMask(void);      // UDS/표시용
size_t   DTC_GetActiveList(dtc_code_t *out, size_t max);
void     DTC_CommitEEPROM(void);       // EEPROM 반영/검증
