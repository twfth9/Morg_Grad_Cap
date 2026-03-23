#include <Arduino.h>
#include <SPI.h>

#include "HatPins.h"
#include "HatSpiMux.h"
#include "Debug485.h"
#include "HatSD.h"

static const uint32_t kSweepFreqs[] = {
  400000,    // safe init-ish speed
  1000000,
  4000000,
  8000000,
  12000000,
  16000000,
  20000000,
  26000000,
  40000000,
  80000000
};

void setup() {
  Debug485.begin(115200);
  delay(300);

  Debug485.println();
  Debug485.println("[TEST] ==================================");
  Debug485.println("[TEST] microSD write/read + speed sweep");
  Debug485.println("[TEST] ==================================");

  if (!HatSD.begin(&Debug485, SPI, 400000)) {
    Debug485.println("[TEST] Initial SD mount failed.");
    return;
  }

  Debug485.println("[TEST] Initial SD mount succeeded.");
  HatSD.printCardInfo(Debug485);
  Debug485.println();

  // Small conservative functional test first
  Debug485.println("[TEST] Running small write/read/verify test...");
  if (!HatSD.writeReadVerifyTest(Debug485, "/GPTTEST.BIN", 4096, 512, true)) {
    Debug485.println("[TEST] Functional test FAILED.");
    return;
  }
  Debug485.println("[TEST] Functional test PASSED.");
  Debug485.println();

  // Then run the speed sweep
  Debug485.println("[TEST] Running throughput sweep...");
  bool any_ok = HatSD.throughputSweep(
      Debug485,
      kSweepFreqs,
      sizeof(kSweepFreqs) / sizeof(kSweepFreqs[0]),
      262144,   // 256 KiB total
      4096,     // 4 KiB chunk
      true      // delete test files
  );

  Debug485.println();
  Debug485.print("[TEST] Sweep overall result: ");
  Debug485.println(any_ok ? "at least one speed passed" : "all speeds failed");
  Debug485.println("[TEST] Test complete.");
}

void loop() {
  delay(1000);
}