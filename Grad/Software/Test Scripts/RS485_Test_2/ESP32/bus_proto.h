#pragma once
#include <stdint.h>
#include "rs485_halfduplex.h"   // your C-style RS485 wrapper
#include "proto.h"

#ifdef __cplusplus
extern "C" {
#endif

// Send a protocol message over RS485
int bus_send_msg(rs485_t* bus, const proto_msg_t* msg);

// Pump RX bytes from RS485 into parser; returns 1 if a full message was received
int bus_poll_msg(rs485_t* bus, proto_parser_t* parser, proto_msg_t* out_msg);

#ifdef __cplusplus
}
#endif