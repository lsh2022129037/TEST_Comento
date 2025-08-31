#pragma once
#include <stdint.h>
typedef union {
  uint16_t u16;
  struct {
    uint16_t VDS_OCP:1;
    uint16_t GDF:1;
    uint16_t UVLO:1;
    uint16_t OTW:1;
    uint16_t OCP:1;
    uint16_t RSVD:11;
  } __attribute__((packed)) b;
} drv8323_stat_t;

void drv8323_init(void);                // SPI2 + CS 핀 설정
int  drv8323_read_status(drv8323_stat_t *out);
