#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_START_BYTE   (0x7E)
#define PROTO_VERSION      (1)

#define PROTO_MAX_PAYLOAD  (32)
#define PROTO_MAX_FRAME    (1 + 1 + 1 + 2 + 2 + 1 + PROTO_MAX_PAYLOAD + 2)

typedef enum {
  MSG_HELLO     = 0x01,
  MSG_HELLO_ACK = 0x02,
  MSG_PING      = 0x03,
  MSG_PONG      = 0x04
} msg_type_t;

typedef struct {
  uint8_t  ver;
  uint8_t  type;
  uint16_t dst;
  uint16_t src;
  uint8_t  len;
  uint8_t  payload[PROTO_MAX_PAYLOAD];
} proto_msg_t;

// Encode proto_msg_t -> bytes (frame). Returns frame length or 0 on error.
size_t proto_encode(uint8_t* out, size_t out_max, const proto_msg_t* msg);

// Streaming parser: feed bytes one-at-a-time; when a full valid message arrives returns 1 and fills *out_msg.
typedef struct {
  uint8_t  buf[PROTO_MAX_FRAME];
  uint16_t idx;
  uint16_t needed; // total bytes needed once len is known, else 0
  uint8_t  in_frame;
} proto_parser_t;

void proto_parser_init(proto_parser_t* p);
int  proto_parser_feed(proto_parser_t* p, uint8_t byte_in, proto_msg_t* out_msg);

// CRC16-CCITT (0x1021), init 0xFFFF
uint16_t proto_crc16_ccitt(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif