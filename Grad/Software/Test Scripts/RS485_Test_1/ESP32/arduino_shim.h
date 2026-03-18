#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void a_pinMode(int pin, int mode);
void a_digitalWrite(int pin, int val);
void a_delay(int ms);
void a_delayMicroseconds(unsigned int us);

void   a_serial_begin_esp32(void* serial_port, uint32_t baud, int rx_pin, int tx_pin);
size_t a_serial_write(void* serial_port, const uint8_t* data, size_t len);
void   a_serial_flush(void* serial_port);
int    a_serial_available(void* serial_port);
int    a_serial_read(void* serial_port);
int    a_serial_peek(void* serial_port);

#ifdef __cplusplus
}
#endif