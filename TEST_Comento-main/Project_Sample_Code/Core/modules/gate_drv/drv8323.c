#include "drv8323.h"
#include "stm32f4xx_hal.h"
extern SPI_HandleTypeDef hspi2;
#define DRV8323_CS_GPIO GPIOB
#define DRV8323_CS_PIN  GPIO_PIN_12

static inline void cs_low(void){ HAL_GPIO_WritePin(DRV8323_CS_GPIO,DRV8323_CS_PIN,GPIO_PIN_RESET);}
static inline void cs_high(void){HAL_GPIO_WritePin(DRV8323_CS_GPIO,DRV8323_CS_PIN,GPIO_PIN_SET);}

void drv8323_init(void){
  cs_high();
  // 필요 시 초기 설정 레지스터 쓰기…
}

int drv8323_read_status(drv8323_stat_t *out){
  uint16_t tx=0x0000, rx=0;
  cs_low();
  HAL_SPI_TransmitReceive(&hspi2,(uint8_t*)&tx,(uint8_t*)&rx,1,10);
  cs_high();
  out->u16 = rx;
  return 0;
}
