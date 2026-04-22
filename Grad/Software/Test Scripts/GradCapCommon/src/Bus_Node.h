#pragma once

#include <Arduino.h>
#include "Bus_Protocol.h"
#include "Debug485.h"

// Size of local buffered debug text on each ESP32 node.
// Keep modest for now; can be tuned later.
#ifndef BUS_NODE_DEBUG_BUFFER_SIZE
#define BUS_NODE_DEBUG_BUFFER_SIZE 256
#endif

class Bus_Node {
public:
  typedef void (*CommandHandler)(uint8_t commandId,
                                 const uint8_t* data,
                                 uint8_t len);

  Bus_Node();

  // Initialize the node using the existing Debug485 transport.
  // nodeAddress may be supplied explicitly, or left as 0 to derive from MAC.
  // Bus_Node expects a binary-safe transport.
  bool begin(Debug485Class* transport, uint8_t nodeAddress = 0);

  // Main service routine. Call frequently from loop().
  void service();

  // ---------------------------------------------------------------------------
  // Identity / status
  // ---------------------------------------------------------------------------

  uint8_t getAddress() const;
  const uint8_t* getMac() const;

  void setFirmwareVersion(uint8_t majorVersion, uint8_t minorVersion);
  void setCapabilities(uint8_t capabilityFlags);

  uint8_t getStatusFlags() const;
  void setStatusFlag(uint8_t flag, bool enabled);
  void setErrorCode(uint8_t errorCode);
  uint8_t getErrorCode() const;

  // ---------------------------------------------------------------------------
  // Debug text buffering
  // ---------------------------------------------------------------------------

  size_t debugPrint(const char* text);
  size_t debugWrite(const uint8_t* data, size_t len);

  uint16_t getDebugBytesPending() const;
  bool hasDebugPending() const;
  bool didDebugOverflow() const;
  void clearDebugOverflow();

  // ---------------------------------------------------------------------------
  // Application command handling
  // ---------------------------------------------------------------------------

  void setCommandHandler(CommandHandler handler);

  // ---------------------------------------------------------------------------
  // Optional application helpers
  // ---------------------------------------------------------------------------

  void setDisplayReady(bool ready);

private:
  Debug485Class* _transport;
  uint8_t _address;
  uint8_t _mac[6];

  uint8_t _fwMajor;
  uint8_t _fwMinor;
  uint8_t _capabilities;

  uint8_t _statusFlags;
  uint8_t _errorCode;

  uint32_t _bootMillis;

  bus_parser_t _parser;

  // Duplicate request detection
  uint8_t _lastRequestSeq;
  uint8_t _lastRequestType;
  uint8_t _lastRequestSrc;
  bool _hasLastRequest;

  // Cached last response for duplicate retries
  bus_frame_t _lastResponseFrame;
  bool _hasLastResponse;

  // Debug ring buffer
  uint8_t _debugBuffer[BUS_NODE_DEBUG_BUFFER_SIZE];
  uint16_t _debugHead;
  uint16_t _debugTail;
  uint16_t _debugCount;
  bool _debugOverflow;

  CommandHandler _commandHandler;

private:
  // ---------------------------------------------------------------------------
  // Internal helpers
  // ---------------------------------------------------------------------------

  void readMacAddress();
  uint8_t deriveAddressFromMac() const;

  void parserReset();
  void processIncomingByte(uint8_t byteIn, uint32_t nowMs);
  void handleFrame(const bus_frame_t& frame);

  bool isFrameForMe(const bus_frame_t& frame) const;
  bool isDuplicateRequest(const bus_frame_t& frame) const;
  void rememberRequest(const bus_frame_t& frame);

  bool sendFrame(const bus_frame_t& frame);
  bool sendSimpleFrame(uint8_t dst,
                       uint8_t type,
                       uint8_t seq,
                       const uint8_t* payload,
                       uint8_t len);

  bool sendEnumResponse(uint8_t dst, uint8_t seq);
  bool sendStatusResponse(uint8_t dst, uint8_t seq);
  bool sendPong(uint8_t dst, uint8_t seq);
  bool sendCommandAck(uint8_t dst, uint8_t seq, uint8_t commandId, uint8_t resultCode);
  bool sendCommandNack(uint8_t dst, uint8_t seq, uint8_t commandId, uint8_t errorCode);
  bool sendDebugData(uint8_t dst, uint8_t seq, uint8_t maxBytesRequested);

  void cacheLastResponse(const bus_frame_t& frame);

  // Debug ring buffer helpers
  size_t pushDebugBytes(const uint8_t* data, size_t len);
  uint16_t popDebugBytes(uint8_t* out, uint16_t maxLen);
};