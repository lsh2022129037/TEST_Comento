#include "dtc_store.h"
#include "eeprom_25lc256.h"

void dtc_from_pmic(const pmic_status8_t *st, dtc_record_t *rec){
  rec->id = 0xC12345;  // 예시 DTC
  uint8_t sev = 0;
  if (st->f.i_fault) sev = 2;       // 전류 fault 최우선
  else if (st->f.v_fault) sev = 1;  // 전압 fault
  else if (st->f.t_fault) sev = 1;  // 온도 warn
  rec->severity = sev; rec->source=1; rec->ts=0;
}
void dtc_save_and_verify(const dtc_record_t *rec, uint16_t ee_addr){
  dtc_record_t back={0};
  ee_write_dma(ee_addr,(const uint8_t*)rec,sizeof(*rec));
  ee_read_dma (ee_addr,(uint8_t*)&back,sizeof(back));
  // 필요 시 back==*rec 검증/로그
}
