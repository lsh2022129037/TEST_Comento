// pmic_mp5475.c (초급: 블로킹 버전; 3단계에서 *_DMA로 교체)
#include "i2c.h"
#include "pmic_mp5475.h"

int pmic_read_status8(pmic_status8_t *o){
  if (!o) return -1;
  HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_V_FAULT, I2C_MEMADD_SIZE_8BIT, &o->f.v_fault, 1, 100);
  HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_I_FAULT, I2C_MEMADD_SIZE_8BIT, &o->f.i_fault, 1, 100);
  HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_T_FAULT, I2C_MEMADD_SIZE_8BIT, &o->f.t_fault, 1, 100);
  HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_PG,      I2C_MEMADD_SIZE_8BIT, &o->f.pg,      1, 100);
  HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_VOUTA_H, I2C_MEMADD_SIZE_8BIT, &o->f.vout_hi, 1, 100);
  HAL_I2C_Mem_Read(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_VOUTA_L, I2C_MEMADD_SIZE_8BIT, &o->f.vout_lo, 1, 100);
  return 0;
}
int pmic_set_voutA_mv(uint16_t mv){
  uint8_t hi,lo; pmic_build_vref_2mV(mv,&hi,&lo);
  HAL_I2C_Mem_Write(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_VOUTA_H, I2C_MEMADD_SIZE_8BIT, &hi, 1, 100);
  HAL_I2C_Mem_Write(&hi2c1, PMIC_I2C_ADDR, PMIC_REG_VOUTA_L, I2C_MEMADD_SIZE_8BIT, &lo, 1, 100);
  return 0;
}
