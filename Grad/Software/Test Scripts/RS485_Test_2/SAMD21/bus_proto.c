#include "bus_proto.h"

int bus_send_msg(rs485_t* bus, const proto_msg_t* msg) {
  uint8_t frame[PROTO_MAX_FRAME];
  size_t n = proto_encode(frame, sizeof(frame), msg);
  if (n == 0) return 0;
  return (rs485_write(bus, frame, n) == n) ? 1 : 0;
}

int bus_poll_msg(rs485_t* bus, proto_parser_t* parser, proto_msg_t* out_msg) {
  while (rs485_available(bus) > 0) {
    int c = rs485_read(bus);
    if (c < 0) break;
    if (proto_parser_feed(parser, (uint8_t)c, out_msg)) {
      return 1;
    }
  }
  return 0;
}