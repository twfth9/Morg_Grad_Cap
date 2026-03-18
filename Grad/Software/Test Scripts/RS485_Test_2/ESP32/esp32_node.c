#include "esp32_node.h"
#include "bus_proto.h"

#define MASTER_ID (0)

void esp_node_init(esp_node_t* n, uint16_t node_id, const uint8_t mac[6]) {
  if (!n) return;
  n->node_id = node_id;
  for (int i = 0; i < 6; i++) n->mac[i] = mac[i];
  n->last_hello_ms = 0;
}

void esp_node_send_hello(rs485_t* bus, const esp_node_t* n) {
  proto_msg_t m;
  m.ver  = PROTO_VERSION;
  m.type = MSG_HELLO;
  m.dst  = MASTER_ID;
  m.src  = n->node_id;
  m.len  = 6;
  for (int i = 0; i < 6; i++) m.payload[i] = n->mac[i];

  (void)bus_send_msg(bus, &m);
}

void esp_node_handle_msg(rs485_t* bus, const esp_node_t* n, const proto_msg_t* msg) {
  if (!bus || !n || !msg) return;
  if (msg->dst != n->node_id) return;         // not for us
  if (msg->ver != PROTO_VERSION) return;

  if (msg->type == MSG_PING) {
    // Reply PONG
    proto_msg_t r;
    r.ver  = PROTO_VERSION;
    r.type = MSG_PONG;
    r.dst  = MASTER_ID;
    r.src  = n->node_id;
    r.len  = 0;
    (void)bus_send_msg(bus, &r);
  }
  // optional: MSG_HELLO_ACK handling (not required)
}