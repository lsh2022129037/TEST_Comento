#include "dtc_store.h"
#include "eeprom_25lc256.h"   // ee_write_dma, ee_read_dma 선언부
#include <string.h>

#define DTC_EE_ADDR   0x0000
#define DTC_MAGIC     0xD7C1

static uint16_t g_mask = 0;

static inline uint16_t code_to_bit(dtc_code_t c) {
  // 간단 매핑: 코드별 비트 위치(원하는 규칙으로 바꿔도 됨)
  switch (c) {
    case DTC_BRAKE_UV:     return (1u << 0);
    case DTC_GATE_OCP:     return (1u << 1);
    case DTC_SENSOR_MAG:   return (1u << 2);
    case DTC_ADC_VOUT_ABN: return (1u << 3);
    default: return 0;
  }
}

void DTC_InitFromEEPROM(void) {
  dtc_persist_t p = {0};
  ee_read_dma(DTC_EE_ADDR, (uint8_t*)&p, sizeof(p));
  if (p.magic == DTC_MAGIC) g_mask = p.active_mask;
  else g_mask = 0;
}

void DTC_Set(dtc_code_t code) {
  g_mask |= code_to_bit(code);
}

void DTC_Clear(dtc_code_t code) {
  g_mask &= ~code_to_bit(code);
}

bool DTC_IsActive(dtc_code_t code) {
  return (g_mask & code_to_bit(code)) != 0;
}

uint16_t DTC_GetActiveMask(void) {
  return g_mask;
}

size_t DTC_GetActiveList(dtc_code_t *out, size_t max) {
  size_t n = 0;
  if (!out || max == 0) return 0;
  if (DTC_IsActive(DTC_BRAKE_UV)     && n < max) out[n++] = DTC_BRAKE_UV;
  if (DTC_IsActive(DTC_GATE_OCP)     && n < max) out[n++] = DTC_GATE_OCP;
  if (DTC_IsActive(DTC_SENSOR_MAG)   && n < max) out[n++] = DTC_SENSOR_MAG;
  if (DTC_IsActive(DTC_ADC_VOUT_ABN) && n < max) out[n++] = DTC_ADC_VOUT_ABN;
  return n;
}

void DTC_CommitEEPROM(void) {
  dtc_persist_t w = { .magic = DTC_MAGIC, .active_mask = g_mask };
  ee_write_dma(DTC_EE_ADDR, (const uint8_t*)&w, sizeof(w));

  // 간단 검증(선택)
  dtc_persist_t r = {0};
  ee_read_dma(DTC_EE_ADDR, (uint8_t*)&r, sizeof(r));
  // 필요하면 r 검사 결과로 오류 처리
}
