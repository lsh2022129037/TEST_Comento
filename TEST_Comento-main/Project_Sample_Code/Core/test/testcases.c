#include "build_config.h"
#include "pmic_map.h"
#include "pmic_mp5475.h"
#include "dtc_store.h"
#include "eeprom_25lc256.h"

extern volatile uint32_t g_eeprom_write_count;
static uint8_t g_last_hi=0,g_last_lo=0;

void run_testcases_startup(void){
#if TESTCASE_REG_PARSE
  // WB-1: 레지스터 파서
  pmic_status8_t st={0}; st.f.v_fault=0x01; // UV_A만 1
  dtc_record_t rec; dtc_from_pmic(&st,&rec);
  // 기대: severity>=1
#endif
#if TESTCASE_VOUT_SET
  // WB-2: 1200mV → 2mV step(600) 인코딩 확인
  pmic_build_vref_2mV(1200,&g_last_hi,&g_last_lo);
  pmic_set_voutA_mv(1200);
#endif
}

void run_testcases_periodic(void){
#if TESTCASE_E2E
  // BB-1: Fault→EEPROM 저장 확인
  static uint8_t once=0;
  if(!once){
    dtc_record_t rec={.id=0xC12345,.severity=2,.source=1,.ts=0};
    dtc_save_and_verify(&rec,0x0000);
    once=1;
  }
#endif
#if TESTCASE_NOFAULT
  // BB-2: 정상 주행 → EEPROM write 증가 없음
  static uint32_t base=0, tick=0;
  if(base==0) base=g_eeprom_write_count;
  if(++tick>1000){ /* 기대: g_eeprom_write_count==base */ tick=0; }
#endif
}
