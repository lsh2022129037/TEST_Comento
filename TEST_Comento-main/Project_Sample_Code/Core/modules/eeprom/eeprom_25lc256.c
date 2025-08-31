// eeprom_25lc256.c (필요 핀: EE_CS_GPIO_Port/Pin)
#include "spi.h"
#include "gpio.h"
#include "eeprom_25lc256.h"

#ifndef EE_CS_GPIO_Port
#define EE_CS_GPIO_Port  GPIOB
#define EE_CS_Pin        GPIO_PIN_2
#endif
static inline void CS_L(){ HAL_GPIO_WritePin(EE_CS_GPIO_Port,EE_CS_Pin,GPIO_PIN_RESET); }
static inline void CS_H(){ HAL_GPIO_WritePin(EE_CS_GPIO_Port,EE_CS_Pin,GPIO_PIN_SET); }

int ee_wren(void){ uint8_t c=0x06; CS_L(); HAL_SPI_Transmit(&hspi1,&c,1,100); CS_H(); return 0; }
int ee_rdsr(uint8_t* sr){ uint8_t c=0x05; CS_L(); HAL_SPI_Transmit(&hspi1,&c,1,100); HAL_SPI_Receive(&hspi1,sr,1,100); CS_H(); return 0; }
static void ee_wait_wip_clear(void){ uint8_t sr; do{ ee_rdsr(&sr);}while(sr&0x01); }

int ee_write(uint16_t a,const uint8_t* p,uint16_t n){
  ee_wren(); CS_L(); uint8_t cmd[3]={0x02,(uint8_t)(a>>8),(uint8_t)a};
  HAL_SPI_Transmit(&hspi1,cmd,3,100); HAL_SPI_Transmit(&hspi1,(uint8_t*)p,n,100); CS_H(); ee_wait_wip_clear(); return 0;
}
int ee_read(uint16_t a,uint8_t* p,uint16_t n){
  CS_L(); uint8_t cmd[3]={0x03,(uint8_t)(a>>8),(uint8_t)a};
  HAL_SPI_Transmit(&hspi1,cmd,3,100); HAL_SPI_Receive(&hspi1,p,n,100); CS_H(); return 0;
}
