#pragma once
#include <stdint.h>
#define AS5600_I2C_ADDR (0x36<<1)
typedef struct { uint16_t raw_angle; uint8_t status; } as5600_data_t;
int as5600_read(as5600_data_t *out); // RAW_ANGLE(0x0C/0x0D), STATUS(0x0B)
