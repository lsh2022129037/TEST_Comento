#include "as5600.h"
#include "stm32f4xx_hal.h"
extern I2C_HandleTypeDef hi2c2;
int as5600_read(as5600_data_t *o){
  uint8_t st=0, hi=0, lo=0;
  HAL_I2C_Mem_Read(&hi2c2,AS5600_I2C_ADDR,0x0B,1,&st,1,10);
  HAL_I2C_Mem_Read(&hi2c2,AS5600_I2C_ADDR,0x0C,1,&hi,1,10);
  HAL_I2C_Mem_Read(&hi2c2,AS5600_I2C_ADDR,0x0D,1,&lo,1,10);
  o->status = st;
  o->raw_angle = ((uint16_t)hi<<8)|lo;
  return 0;
}
