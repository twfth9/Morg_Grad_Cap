#include "proto.h"

static void put_u16_le(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static uint16_t get_u16_le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint16_t proto_crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
      else             crc = (uint16_t)(crc << 1);
    }
  }
  return crc;
}

size_t proto_encode(uint8_t* out, size_t out_max, const proto_msg_t* msg) {
  if (!out || !msg) return 0;
  if (msg->len > PROTO_MAX_PAYLOAD) return 0;

  size_t frame_len = 1 + 1 + 1 + 2 + 2 + 1 + msg->len + 2;
  if (out_max < frame_len) return 0;

  size_t i = 0;
  out[i++] = PROTO_START_BYTE;
  out[i++] = PROTO_VERSION;
  out[i++] = msg->type;
  put_u16_le(&out[i], msg->dst); i += 2;
  put_u16_le(&out[i], msg->src); i += 2;
  out[i++] = msg->len;

  for (uint8_t k = 0; k < msg->len; k++) out[i++] = msg->payload[k];

  // CRC over [VER..end payload]
  uint16_t crc = proto_crc16_ccitt(&out[1], (frame_len - 1 - 2));
  put_u16_le(&out[i], crc); i += 2;

  return i;
}

void proto_parser_init(proto_parser_t* p) {
  if (!p) return;
  p->idx = 0;
  p->needed = 0;
  p->in_frame = 0;
}

static void parser_reset(proto_parser_t* p) {
  p->idx = 0;
  p->needed = 0;
  p->in_frame = 0;
}

int proto_parser_feed(proto_parser_t* p, uint8_t b, proto_msg_t* out_msg) {
  if (!p || !out_msg) return 0;

  if (!p->in_frame) {
    if (b == PROTO_START_BYTE) {
      p->in_frame = 1;
      p->idx = 0;
      p->needed = 0;
      p->buf[p->idx++] = b;
    }
    return 0;
  }

  // store byte
  if (p->idx >= PROTO_MAX_FRAME) {
    parser_reset(p);
    return 0;
  }
  p->buf[p->idx++] = b;

  // Once we have LEN byte, we know total frame length
  // Layout indices: 0 START, 1 VER, 2 TYPE, 3..4 DST, 5..6 SRC, 7 LEN, 8.. payload
  if (p->idx == 8) {
    // next byte will be LEN
    return 0;
  }
  if (p->idx == 9) {
    uint8_t len = p->buf[7];
    if (len > PROTO_MAX_PAYLOAD) {
      parser_reset(p);
      return 0;
    }
    p->needed = (uint16_t)(1 + 1 + 1 + 2 + 2 + 1 + len + 2);
    if (p->needed > PROTO_MAX_FRAME) {
      parser_reset(p);
      return 0;
    }
  }

  if (p->needed != 0 && p->idx >= p->needed) {
    // Validate CRC
    uint16_t rx_crc = get_u16_le(&p->buf[p->needed - 2]);
    uint16_t calc  = proto_crc16_ccitt(&p->buf[1], (size_t)(p->needed - 1 - 2));
    if (rx_crc != calc) {
      parser_reset(p);
      return 0;
    }

    // Unpack
    out_msg->ver  = p->buf[1];
    out_msg->type = p->buf[2];
    out_msg->dst  = get_u16_le(&p->buf[3]);
    out_msg->src  = get_u16_le(&p->buf[5]);
    out_msg->len  = p->buf[7];
    for (uint8_t k = 0; k < out_msg->len; k++) out_msg->payload[k] = p->buf[8 + k];

    parser_reset(p);
    return 1;
  }

  return 0;
}