#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ESP32_RS485_TX_PIN   (11)  /* UART TX -> THVD1400D DI */
#define ESP32_RS485_RX_PIN   (12)  /* THVD1400D RO -> UART RX */
#define ESP32_RS485_DE_PIN   (21)  /* THVD1400D DE (HIGH=TX, LOW=RX) */

#ifdef __cplusplus
}
#endif