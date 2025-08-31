#include "can_if.h"
#include "dtc_store.h"

#define UDS_SVC_READ_DTC  0x19
#define UDS_SVC_CLEAR_DTC 0x14

int uds_handle_request(const uint8_t* rx, uint8_t len, uint8_t* tx){
  if (len<1) return 0;
  uint8_t sid=rx[0];
  if (sid==UDS_SVC_CLEAR_DTC){
    dtc_record_t clr={0}; dtc_save_and_verify(&clr,0x0000);
    tx[0]=0x54; /* POS(0x40|0x14) */ return 1;
  } else if (sid==UDS_SVC_READ_DTC){
    tx[0]=0x59; tx[1]=0x02; // subf 예시
    tx[2]=0x12; tx[3]=0x34; tx[4]=0x00; // DTC 예시(0x1234)
    tx[5]=0x01; // status 예시
    return 6;
  }
  return 0;
}
