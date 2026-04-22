#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void*    serial;        /* opaque pointer to HardwareSerial (C++), passed in from .ino */
  int      de_pin;
  uint16_t pre_tx_us;
  uint16_t post_tx_us;
  uint16_t turnaround_us;
} rs485_t;

/* Initialize wrapper + underlying UART (ESP32 lets us pick RX/TX pins) */
void rs485_init_esp32(rs485_t* ctx,
                      void* serial_port,
                      int de_pin,
                      uint32_t baud,
                      int rx_pin,
                      int tx_pin);

/* Optional timing tuning */
void rs485_set_timings(rs485_t* ctx, uint16_t pre_tx_us, uint16_t post_tx_us, uint16_t turnaround_us);

/* Direction control */
void rs485_set_receive(rs485_t* ctx);
void rs485_set_transmit(rs485_t* ctx);

/* TX */
size_t rs485_write(rs485_t* ctx, const uint8_t* data, size_t len);
size_t rs485_print(rs485_t* ctx, const char* s);
size_t rs485_println(rs485_t* ctx, const char* s); /* appends \r\n */

/* RX */
int rs485_available(rs485_t* ctx);
int rs485_read(rs485_t* ctx);
int rs485_peek(rs485_t* ctx);

#ifdef __cplusplus
}
#endif