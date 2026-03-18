#include "samd_master.h"
#include "bus_proto.h"

static void copy_mac(uint8_t* dst, const uint8_t* src) {
  for (int i = 0; i < 6; i++) dst[i] = src[i];
}

void node_table_init(node_table_t* t) {
  t->count = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    t->nodes[i].id = 0;
    t->nodes[i].has_mac = 0;
    t->nodes[i].last_seen_ms = 0;
  }
}

node_entry_t* node_table_find(node_table_t* t, uint16_t id) {
  for (int i = 0; i < MAX_NODES; i++) {
    if (t->nodes[i].has_mac || t->nodes[i].last_seen_ms != 0) {
      if (t->nodes[i].id == id) return &t->nodes[i];
    }
  }
  return 0;
}

int node_table_upsert(node_table_t* t, uint16_t id, const uint8_t* mac6, uint32_t now_ms) {
  node_entry_t* e = node_table_find(t, id);
  if (!e) {
    // find empty slot
    for (int i = 0; i < MAX_NODES; i++) {
      if (t->nodes[i].last_seen_ms == 0 && t->nodes[i].has_mac == 0) {
        t->nodes[i].id = id;
        t->nodes[i].last_seen_ms = now_ms;
        if (mac6) { copy_mac(t->nodes[i].mac, mac6); t->nodes[i].has_mac = 1; }
        if (t->count < MAX_NODES) t->count++;
        return 1;
      }
    }
    return 0;
  } else {
    e->last_seen_ms = now_ms;
    if (mac6) { copy_mac(e->mac, mac6); e->has_mac = 1; }
    return 1;
  }
}

int master_send_hello_ack(rs485_t* bus, uint16_t dst_id) {
  proto_msg_t m;
  m.ver = PROTO_VERSION;
  m.type = MSG_HELLO_ACK;
  m.dst = dst_id;
  m.src = MASTER_ID;
  m.len = 0;
  return bus_send_msg(bus, &m);
}

int master_send_ping(rs485_t* bus, uint16_t dst_id) {
  proto_msg_t m;
  m.ver = PROTO_VERSION;
  m.type = MSG_PING;
  m.dst = dst_id;
  m.src = MASTER_ID;
  m.len = 0;
  return bus_send_msg(bus, &m);
}

void master_handle_msg(rs485_t* bus, node_table_t* t, const proto_msg_t* msg, uint32_t now_ms) {
  if (!bus || !t || !msg) return;
  if (msg->ver != PROTO_VERSION) return;
  if (msg->dst != MASTER_ID) return; // only process messages to master

  if (msg->type == MSG_HELLO) {
    const uint8_t* mac = (msg->len == 6) ? msg->payload : 0;
    (void)node_table_upsert(t, msg->src, mac, now_ms);
    (void)master_send_hello_ack(bus, msg->src);
  } else if (msg->type == MSG_PONG) {
    (void)node_table_upsert(t, msg->src, 0, now_ms);
  }
}