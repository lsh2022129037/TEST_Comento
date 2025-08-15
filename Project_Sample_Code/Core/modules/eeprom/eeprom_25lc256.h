// eeprom_25lc256.h
#pragma once
#include <stdint.h>
int ee_wren(void);                             // 0x06
int ee_write(uint16_t addr, const uint8_t* p, uint16_t len); // 0x02
int ee_read (uint16_t addr, uint8_t* p, uint16_t len);      // 0x03
int ee_rdsr(uint8_t* sr);                      // 0x05
