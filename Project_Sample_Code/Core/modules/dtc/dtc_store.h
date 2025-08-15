#pragma once
#include <stdint.h>
#include "pmic_map.h"

typedef struct __attribute__((packed)){
  uint32_t id;       // 예: 브레이크 관련 코드
  uint8_t  severity; // 0=none,1=warn,2=fault
  uint8_t  source;   // 1=PMIC
  uint16_t ts;       // 간단 타임스탬프(옵션)
} dtc_record_t;

void dtc_from_pmic(const pmic_status8_t *st, dtc_record_t *rec);
void dtc_save_and_verify(const dtc_record_t *rec, uint16_t ee_addr);
