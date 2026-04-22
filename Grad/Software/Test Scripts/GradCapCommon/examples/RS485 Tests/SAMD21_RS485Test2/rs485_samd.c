#include "rs485.h"
#include "arduino_shim.h"

void rs485_set_timings(rs485_t* ctx, uint16_t pre_tx_us, uint16_t post_tx_us, uint16_t turnaround_us) {
  if (!ctx) return;
  ctx->pre_tx_us = pre_tx_us;
  ctx->post_tx_us = post_tx_us;
  ctx->turnaround_us = turnaround_us;
}

void rs485_init(rs485_t* ctx, void* serial_port, int de_pin, uint32_t baud) {
  if (!ctx) return;

  ctx->serial = serial_port;
  ctx->de_pin = de_pin;
  ctx->pre_tx_us = 50;
  ctx->post_tx_us = 50;
  ctx->turnaround_us = 50;

  a_pinMode(ctx->de_pin, 1 /*OUTPUT*/);
  a_digitalWrite(ctx->de_pin, 0 /*LOW*/);

  a_serial_begin(ctx->serial, baud);
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