#pragma once
#include <stdint.h>
int uds_handle_request(const uint8_t* rx, uint8_t len, uint8_t* tx); // returns tx_len
