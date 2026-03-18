#pragma once
#include <stdint.h>
#include "proto.h"
#include "rs485.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MASTER_ID (0)
#define MAX_NODES (16)

typedef struct {
  uint16_t id;
  uint8_t  mac[6];
  uint8_t  has_mac;
  uint32_t last_seen_ms;
} node_entry_t;

typedef struct {
  node_entry_t nodes[MAX_NODES];
  uint8_t count;
} node_table_t;

void node_table_init(node_table_t* t);
int  node_table_upsert(node_table_t* t, uint16_t id, const uint8_t* mac6, uint32_t now_ms);
node_entry_t* node_table_find(node_table_t* t, uint16_t id);

void master_handle_msg(rs485_t* bus, node_table_t* t, const proto_msg_t* msg, uint32_t now_ms);
int  master_send_ping(rs485_t* bus, uint16_t dst_id);
int  master_send_hello_ack(rs485_t* bus, uint16_t dst_id);

#ifdef __cplusplus
}
#endif