#pragma once
#include <stdint.h>
#include "rs485_halfduplex.h"
#include "proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t node_id;
  uint8_t  mac[6];
  uint32_t last_hello_ms;
} esp_node_t;

void esp_node_init(esp_node_t* n, uint16_t node_id, const uint8_t mac[6]);
void esp_node_send_hello(rs485_t* bus, const esp_node_t* n);
void esp_node_handle_msg(rs485_t* bus, const esp_node_t* n, const proto_msg_t* msg);

#ifdef __cplusplus
}
#endif