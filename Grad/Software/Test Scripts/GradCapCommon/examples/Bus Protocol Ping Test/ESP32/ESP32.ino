#include <Arduino.h>

#include "ESP32_Pinmap.h"
#include "ESP32_BusPhy.h"
#include "Bus_Protocol.h"
#include "Bus_Node.h"
#include "Bus_Debug.h"

static constexpr uint8_t THIS_NODE_ID = 9;  // change per board

ESP32_BusPhy busPhy(Serial1);
BusNode node;
BusDebug DebugBus;

static uint8_t txBuffer[BUS_MAX_FRAME_SIZE];

uint16_t lastHandledStateSeq = 0;
bool transmittedForCurrentGrant = false;

void serviceBusRx() {
  while (busPhy.available() > 0) {
    int value = busPhy.read();
    if (value < 0) {
      break;
    }
    node.processIncomingByte(static_cast<uint8_t>(value), millis());
  }
}

void sendCurrentBundle() {
  while (node.transmitBundleHasMoreFrames()) {
    const size_t len = node.getNextEncodedTransmitFrame(txBuffer, sizeof(txBuffer));
    if (len == 0) {
      break;
    }
    busPhy.write(txBuffer, len);
  }
}

void prepareNodeStatus() {
  node.setLcdStatus(BUS_LCD_STATUS_READY);
  node.setAssetStatus(BUS_ASSET_STATUS_NONE);

  // Distinct values per node make it easier to verify the SAMD is parsing
  // the correct TX_REPORT from the correct node.
  node.setPsramUsage(1000UL * THIS_NODE_ID, 8UL * 1024UL * 1024UL);
  node.setFlashUsage(2000UL * THIS_NODE_ID, 16UL * 1024UL * 1024UL);

  node.setDisplayReady(false);
  node.setWorkPending(false);
  node.setMemoryWarning(false);
  node.setSdActive(false);
  node.setErrorCode(0);
}

void queueNodeMessagesForTurn() {
  DebugBus.print("Hello World from node ");
  DebugBus.println(static_cast<unsigned int>(THIS_NODE_ID));

  DebugBus.print("Node ");
  DebugBus.print(static_cast<unsigned int>(THIS_NODE_ID));
  DebugBus.println(" responding to TX grant");

  DebugBus.print("Long debug line from node ");
  DebugBus.print(static_cast<unsigned int>(THIS_NODE_ID));
  DebugBus.println(": ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 abcdefghijklmnopqrstuvwxyz");

  // You can add one more long-ish line if desired.
  DebugBus.print("Node ");
  DebugBus.print(static_cast<unsigned int>(THIS_NODE_ID));
  DebugBus.println(" report payload prepared successfully");
}

void setup() {
  Serial.begin(115200);
  delay(250);

  busPhy.begin();

  node.begin(THIS_NODE_ID);
  prepareNodeStatus();

  DebugBus.begin(&node);
  DebugBus.setEnabled(true);
  DebugBus.setLevel(BUS_DEBUG_LEVEL_INFO);

  Serial.print("ESP32 node ready, ID=");
  Serial.println(THIS_NODE_ID);
}

void loop() {
  serviceBusRx();

  const bus_bus_state_t& state = node.getBusState();

  if (!node.hasTxOwnership()) {
    transmittedForCurrentGrant = false;
    lastHandledStateSeq = state.state_seq;
    return;
  }

  if (node.hasTxOwnership() && node.retransmitRequested()) {
    if (node.hasRetainedBundle() && !transmittedForCurrentGrant) {
      node.retransmitLastBundle();
      sendCurrentBundle();
      transmittedForCurrentGrant = true;
    }
    return;
  }

  if (node.hasTxOwnership() &&
      state.state_seq != lastHandledStateSeq &&
      !transmittedForCurrentGrant) {

    lastHandledStateSeq = state.state_seq;

    if (node.hasSdOwnership()) {
      node.setSdActive(true);
    } else {
      node.setSdActive(false);
    }

    queueNodeMessagesForTurn();

    if (!node.hasRetainedBundle()) {
      node.buildTransmitBundle();
    }

    sendCurrentBundle();
    transmittedForCurrentGrant = true;
  }
}