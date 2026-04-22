#include <Arduino.h>

#include "SAMD21_Pinmap.h"
#include "SAMD21_BusPhy.h"
#include "Bus_Protocol.h"
#include "Bus_Master.h"

SAMD21_BusPhy busPhy(Serial1);
BusMaster master;

static constexpr uint8_t NODE_IDS[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static constexpr size_t NODE_COUNT = sizeof(NODE_IDS) / sizeof(NODE_IDS[0]);

static bool testStarted = false;
static bool testFinished = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println();
  Serial.println(F("SAMD21 9-node RS485 harness test starting"));

  busPhy.begin();
  master.begin();

  master.setStartupPhase(true);
  master.setTxOwner(0);
  master.setSdOwner(0);

  // Turn verbose logging on or off here.
  master.setVerbose(true);

  Serial.println(F("SAMD21 ready."));
  delay(3000);
}

void loop() {
  if (testFinished) {
    delay(1000);
    return;
  }

  if (!testStarted) {
    testStarted = true;

    bus_ping_test_options_t options{};
    options.verbose = true;                 // show RX frames, node messages, summary table
    options.request_enum = true;           // request ENUM before each ping test
    options.max_attempts_per_node = 5;     // retry disconnected/problem nodes up to 5 times
    options.data_flags = BUS_PING_DATA_ALL;  // collect RTT + status/error + memory usage
    options.pong_timeout_ms = 1000;        // currently not heavily used internally, but keep set
    options.tx_report_timeout_ms = 3000;

    bus_ping_test_summary_t summary{};

    master.setVerbose(options.verbose);

    const bool ok = master.runPingTestForNodes(
        busPhy,
        NODE_IDS,
        NODE_COUNT,
        options,
        &summary);

    Serial.println();
    if (ok) {
      Serial.println(F("Harness ping test PASSED."));
    } else {
      Serial.println(F("Harness ping test FAILED."));
    }

    // If verbose is false and you still want a one-line outcome:
    if (!options.verbose) {
      Serial.print(F("Pass count: "));
      Serial.println(summary.pass_count);
      Serial.print(F("Fail count: "));
      Serial.println(summary.fail_count);
    }

    testFinished = true;
  }
}