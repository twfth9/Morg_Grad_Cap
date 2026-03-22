#include "rs485_halfduplex.h"
#include "arduino_shim.h"
#include <string.h>

/*
  NOTE:
  This file is compiled by Arduino as C++ *unless* you force it, but we keep it
  strictly C syntax. We still need to call Arduino functions (pinMode, digitalWrite,
  delayMicroseconds) and HardwareSerial methods. Those calls are done via small
  C-callable wrappers defined in the .ino file.
*/

#ifdef __cplusplus
extern "C" {
#endif

/* Wrappers implemented in .ino (C++), callable from here */
void a_pinMode(int pin, int mode);
void a_digitalWrite(int pin, int val);
void a_delay(int ms);
void a_delayMicroseconds(unsigned int us);

/* HardwareSerial wrappers implemented in .ino */
void a_serial_begin_esp32(void* serial_port, uint32_t baud, int rx_pin, int tx_pin);
size_t a_serial_write(void* serial_port, const uint8_t* data, size_t len);
void a_serial_flush(void* serial_port);
int a_serial_available(void* serial_port);
int a_serial_read(void* serial_port);
int a_serial_peek(void* serial_port);

#ifdef __cplusplus
}
#endif

void rs485_set_timings(rs485_t* ctx, uint16_t pre_tx_us, uint16_t post_tx_us, uint16_t turnaround_us) {
  if (!ctx) return;
  ctx->pre_tx_us = pre_tx_us;
  ctx->post_tx_us = post_tx_us;
  ctx->turnaround_us = turnaround_us;
}

void rs485_init_esp32(rs485_t* ctx,
                      void* serial_port,
                      int de_pin,
                      uint32_t baud,
                      int rx_pin,
                      int tx_pin)
{
  if (!ctx) return;

  ctx->serial = serial_port;
  ctx->de_pin = de_pin;
  ctx->pre_tx_us = 50;
  ctx->post_tx_us = 50;
  ctx->turnaround_us = 50;

  a_pinMode(ctx->de_pin, 1 /*OUTPUT*/);
  a_digitalWrite(ctx->de_pin, 0 /*LOW*/);

  a_serial_begin_esp32(ctx->serial, baud, rx_pin, tx_pin);

  a_delay(10);
}

void rs485_set_receive(rs485_t* ctx) {
  if (!ctx) return;
  a_digitalWrite(ctx->de_pin, 0 /*LOW*/);
  a_delayMicroseconds(ctx->turnaround_us);
}

void rs485_set_transmit(rs485_t* ctx) {
  if (!ctx) return;
  a_digitalWrite(ctx->de_pin, 1 /*HIGH*/);
  a_delayMicroseconds(ctx->pre_tx_us);
}

size_t rs485_write(rs485_t* ctx, const uint8_t* data, size_t len) {
  if (!ctx || !data || !len) return 0;

  rs485_set_transmit(ctx);
  size_t n = a_serial_write(ctx->serial, data, len);
  a_serial_flush(ctx->serial);
  a_delayMicroseconds(ctx->post_tx_us);
  rs485_set_receive(ctx);

  return n;
}

size_t rs485_print(rs485_t* ctx, const char* s) {
  if (!ctx || !s) return 0;
  return rs485_write(ctx, (const uint8_t*)s, strlen(s));
}

size_t rs485_println(rs485_t* ctx, const char* s) {
  size_t n = 0;
  n += rs485_print(ctx, s);
  n += rs485_write(ctx, (const uint8_t*)"\r\n", 2);
  return n;
}

int rs485_available(rs485_t* ctx) {
  if (!ctx) return 0;
  return a_serial_available(ctx->serial);
}

int rs485_read(rs485_t* ctx) {
  if (!ctx) return -1;
  return a_serial_read(ctx->serial);
}

int rs485_peek(rs485_t* ctx) {
  if (!ctx) return -1;
  return a_serial_peek(ctx->serial);
}