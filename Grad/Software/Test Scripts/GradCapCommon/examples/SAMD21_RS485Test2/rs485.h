#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void*    serial;        /* HardwareSerial* */
  int      de_pin;
  uint16_t pre_tx_us;
  uint16_t post_tx_us;
  uint16_t turnaround_us;
} rs485_t;

void rs485_init(rs485_t* ctx, void* serial_port, int de_pin, uint32_t baud);
void rs485_set_timings(rs485_t* ctx, uint16_t pre_tx_us, uint16_t post_tx_us, uint16_t turnaround_us);
void rs485_set_receive(rs485_t* ctx);
void rs485_set_transmit(rs485_t* ctx);

size_t rs485_write(rs485_t* ctx, const uint8_t* data, size_t len);
int    rs485_available(rs485_t* ctx);
int    rs485_read(rs485_t* ctx);
int    rs485_peek(rs485_t* ctx);

#ifdef __cplusplus
}
#endif