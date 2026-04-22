#include <Arduino.h>
#include "HatPins.h"
#include "Debug485.h"
#include "Bus_Node.h"
#include "Bus_Protocol.h"

Bus_Node busNode;

static uint32_t g_heartbeatCounter = 0;
static uint32_t g_lastHeartbeatMs = 0;

enum : uint8_t {
  CMD_SET_ERROR_CODE = 0x01
};

void handleBusCommand(uint8_t commandId, const uint8_t* data, uint8_t len) {
  switch (commandId) {
    case CMD_SET_ERROR_CODE:
      if (len >= 1) {
        busNode.setErrorCode(data[0]);
        busNode.debugPrint("[ESP32] CMD_SET_ERROR_CODE received, new error code = ");
        char msg[16];
        snprintf(msg, sizeof(msg), "%u\r\n", data[0]);
        busNode.debugPrint(msg);
      } else {
        busNode.debugPrint("[ESP32] CMD_SET_ERROR_CODE missing payload\r\n");
      }
      break;

    default:
      busNode.debugPrint("[ESP32] Unknown command received\r\n");
      break;
  }
}

void setup() {
  Debug485.begin(460800);
  //Debug485.setPrefix("[ESP32 RAW] ");

  busNode.begin(&Debug485);  // address derived from MAC
  busNode.setFirmwareVersion(1, 0);
  busNode.setCapabilities(BUS_CAP_DEBUG_TEXT | BUS_CAP_STATUS_REPORT);
  busNode.setDisplayReady(true);
  busNode.setCommandHandler(handleBusCommand);

  busNode.debugPrint("\r\n[ESP32] Bus node booted\r\n");
  busNode.debugPrint("[ESP32] Waiting for master enumeration/status/debug polls\r\n");
}

void loop() {
  busNode.service();

  uint32_t now = millis();
  if ((now - g_lastHeartbeatMs) >= 3000) {
    g_lastHeartbeatMs = now;
    g_heartbeatCounter++;

    char msg[96];
    snprintf(msg,
             sizeof(msg),
             "[ESP32] heartbeat %lu, addr=0x%02X, debug_pending=%u\r\n",
             (unsigned long)g_heartbeatCounter,
             busNode.getAddress(),
             (unsigned int)busNode.getDebugBytesPending());
    busNode.debugPrint(msg);
  }
}