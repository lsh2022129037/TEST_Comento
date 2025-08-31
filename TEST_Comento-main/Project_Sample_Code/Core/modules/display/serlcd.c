#include "serlcd.h"
#include "stm32f4xx_hal.h"
extern UART_HandleTypeDef huart4;
void serlcd_init(void){ /* 필요시 초기 명령 송신 */ }
void serlcd_set_cursor(uint8_t r,uint8_t c){ (void)r; (void)c; /* 확장 시 명령 송신 */ }
void serlcd_print(const char *s){
  HAL_UART_Transmit(&huart4,(uint8_t*)s,strlen(s),HAL_MAX_DELAY);
  HAL_UART_Transmit(&huart4,(uint8_t*)"\r\n",2,HAL_MAX_DELAY);
}
