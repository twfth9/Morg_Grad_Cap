#include <Arduino.h>
#include "esp32_pinmap.h"
#include "rs485_halfduplex.h"
#include "arduino_shim.h"   // your wrapper prototypes
#include "proto.h"
#include "bus_proto.h"
#include "esp32_node.h"

static rs485_t g_bus;
static proto_parser_t g_parser;
static esp_node_t g_node;

static void get_mac_and_id(uint8_t mac[6], uint16_t* node_id) {
  // Arduino-ESP32: get efuse mac as 64-bit, low 48 bits used
  uint64_t m = ESP.getEfuseMac();
  mac[0] = (uint8_t)(m >> 40);
  mac[1] = (uint8_t)(m >> 32);
  mac[2] = (uint8_t)(m >> 24);
  mac[3] = (uint8_t)(m >> 16);
  mac[4] = (uint8_t)(m >>  8);
  mac[5] = (uint8_t)(m >>  0);

  *node_id = (uint16_t)((mac[4] << 8) | mac[5]);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  rs485_init_esp32(&g_bus, (void*)&Serial1,
                   ESP32_RS485_DE_PIN, 9600,
                   ESP32_RS485_RX_PIN, ESP32_RS485_TX_PIN);
  rs485_set_receive(&g_bus);
  proto_parser_init(&g_parser);

  uint8_t mac[6];
  uint16_t node_id;
  get_mac_and_id(mac, &node_id);
  esp_node_init(&g_node, node_id, mac);

  // Random backoff so 9 nodes don't collide
  uint32_t seed = ((uint32_t)mac[2] << 24) ^ ((uint32_t)mac[3] << 16) ^ ((uint32_t)mac[4] << 8) ^ mac[5];
  randomSeed(seed);
  delay((uint32_t)random(0, 1000));

  Serial.printf("ESP node_id=0x%04X MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                g_node.node_id, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);

  // Send HELLO once at boot
  esp_node_send_hello(&g_bus, &g_node);
}

void loop() {
  proto_msg_t msg;
  if (bus_poll_msg(&g_bus, &g_parser, &msg)) {
    esp_node_handle_msg(&g_bus, &g_node, &msg);
  }
}