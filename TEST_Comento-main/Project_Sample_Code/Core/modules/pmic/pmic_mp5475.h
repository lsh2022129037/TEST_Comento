// pmic_mp5475.h
#pragma once
#include "pmic_map.h"
int pmic_read_status8(pmic_status8_t *out);   // 0x06/07/08/09/13/14 읽기
int pmic_set_voutA_mv(uint16_t mv);           // 2mV step → 0x13/0x14 씀
