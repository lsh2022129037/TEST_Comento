#pragma once
#include <stdint.h>

#define PMIC_I2C_ADDR7   (0x60u)
#define PMIC_I2C_ADDR    (PMIC_I2C_ADDR7 << 1)  // HAL은 8-bit 주소 사용

typedef enum {
  PMIC_REG_STATUS  = 0x05,
  PMIC_REG_PG      = 0x06,  // Power Good
  PMIC_REG_V_FAULT = 0x07,  // 전압: UV/OV
  PMIC_REG_I_FAULT = 0x08,  // 전류: OC/OC_WARN
  PMIC_REG_T_FAULT = 0x09,  // 온도/시스템 요약
  PMIC_REG_VOUTA_H = 0x13,
  PMIC_REG_VOUTA_L = 0x14,
} pmic_reg_t;

typedef union {
  struct {
    uint8_t v_fault;  // 0x07
    uint8_t i_fault;  // 0x08
    uint8_t t_fault;  // 0x09
    uint8_t pg;       // 0x06
    uint8_t vout_hi;  // 0x13
    uint8_t vout_lo;  // 0x14
    uint8_t rsv1, rsv2;
  } f;
  uint8_t raw[8];
} pmic_status8_t;

// 비트필드(과제용 단순화)
typedef struct { uint8_t uv:4, ov:4; } __attribute__((packed)) pmic_v_bits_t;  // 0x07
typedef struct { uint8_t oc:4, oc_warn:4; } __attribute__((packed)) pmic_i_bits_t; // 0x08
typedef union  {
  uint8_t byte;
  struct { uint8_t ot_warn:1, ot_shdn:1, rsv:6; } b; // 0x09
} __attribute__((packed)) pmic_t_bits_u;

static inline void pmic_build_vref_2mV(uint16_t mv, uint8_t *hi, uint8_t *lo){
  uint16_t step = (uint16_t)(mv/2u);
  *hi = (uint8_t)(step >> 8);
  *lo = (uint8_t)(step & 0xFF);
}
