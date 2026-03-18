#include <Arduino.h>
#include "samd21_pinmap.h"
#include "rs485.h"
#include "arduino_shim.h"          // your SAMD shim prototypes (like on ESP32)
#include "proto.h"
#include "bus_proto.h"
#include "samd_master.h"

static rs485_t g_bus;
static proto_parser_t g_parser;
static node_table_t g_table;

static uint32_t g_last_poll_ms = 0;
static uint16_t g_poll_index = 0;

static void print_node(const node_entry_t* e) {
  Serial.print("Node 0x");
  Serial.print(e->id, HEX);
  if (e->has_mac) {
    Serial.print(" MAC=");
    for (int i = 0; i < 6; i++) {
      if (i) Serial.print(":");
      if (e->mac[i] < 16) Serial.print("0");
      Serial.print(e->mac[i], HEX);
    }
  }
  Serial.print(" last_seen_ms=");
  Serial.println(e->last_seen_ms);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("SAMD21 RS-485 master starting...");

  rs485_init(&g_bus, (void*)&Serial1, SAMD_RS485_DE_PIN, 9600);
  rs485_set_receive(&g_bus);

  proto_parser_init(&g_parser);
  node_table_init(&g_table);

  Serial.println("Listening for HELLO...");
}

void loop() {
  uint32_t now = millis();

  // 1) Pump incoming bus bytes
  proto_msg_t msg;
  while (bus_poll_msg(&g_bus, &g_parser, &msg)) {
    master_handle_msg(&g_bus, &g_table, &msg, now);

    // Print interesting events
    if (msg.type == MSG_HELLO) {
      Serial.print("RX HELLO from 0x");
      Serial.println(msg.src, HEX);
      node_entry_t* e = node_table_find(&g_table, msg.src);
      if (e) print_node(e);
    } else if (msg.type == MSG_PONG) {
      Serial.print("RX PONG from 0x");
      Serial.println(msg.src, HEX);
    }
  }

  // 2) Periodically poll nodes to verify bidirectional comms
  if (now - g_last_poll_ms >= 500) { // poll every 500ms (round-robin)
    g_last_poll_ms = now;

    // find next valid node slot
    for (int tries = 0; tries < MAX_NODES; tries++) {
      uint16_t idx = (g_poll_index++) % MAX_NODES;
      node_entry_t* e = &g_table.nodes[idx];
      if (e->last_seen_ms != 0) {
        // Send PING
        Serial.print("TX PING -> 0x");
        Serial.println(e->id, HEX);
        master_send_ping(&g_bus, e->id);

        // wait briefly for matching PONG
        uint32_t start = millis();
        int got_pong = 0;
        while (millis() - start < 50) {
          proto_msg_t r;
          if (bus_poll_msg(&g_bus, &g_parser, &r)) {
            master_handle_msg(&g_bus, &g_table, &r, millis());
            if (r.type == MSG_PONG && r.src == e->id) {
              got_pong = 1;
              break;
            }
          }
        }

        if (got_pong) {
          Serial.print("OK  PONG <- 0x");
          Serial.println(e->id, HEX);
        } else {
          Serial.print("MISS (no PONG) from 0x");
          Serial.println(e->id, HEX);
        }
        break;
      }
    }
  }
}