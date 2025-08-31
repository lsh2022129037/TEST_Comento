#pragma once
#include <stdint.h>
void serlcd_init(void);
void serlcd_set_cursor(uint8_t row, uint8_t col);
void serlcd_print(const char *s);
