#include "Bus_Node.h"

#include <string.h>

#if defined(ESP32)
  #include <esp_system.h>
  #include <esp_mac.h>
#endif

Bus_Node::Bus_Node()
    : _transport(nullptr),
      _address(0),
      _fwMajor(1),
      _fwMinor(0),
      _capabilities(BUS_CAP_DEBUG_TEXT | BUS_CAP_STATUS_REPORT),
      _statusFlags(BUS_STATUS_ALIVE),
      _errorCode(0),
      _bootMillis(0),
      _lastRequestSeq(0),
      _lastRequestType(0),
      _lastRequestSrc(0),
      _hasLastRequest(false),
      _hasLastResponse(false),
      _debugHead(0),
      _debugTail(0),
      _debugCount(0),
      _debugOverflow(false),
      _commandHandler(nullptr) {
  memset(_mac, 0, sizeof(_mac));
  memset(&_parser, 0, sizeof(_parser));
  memset(&_lastResponseFrame, 0, sizeof(_lastResponseFrame));
}

bool Bus_Node::begin(Debug485Class* transport, uint8_t nodeAddress) {
  if (!transport) {
    return false;
  }

  _transport = transport;
  _bootMillis = millis();

  readMacAddress();

  if (nodeAddress == 0) {
    _address = deriveAddressFromMac();
  } else {
    _address = nodeAddress;
  }

  parserReset();

  _statusFlags |= BUS_STATUS_ALIVE;
  if (_debugCount > 0) {
    _statusFlags |= BUS_STATUS_DEBUG_PENDING;
  } else {
    _statusFlags &= ~BUS_STATUS_DEBUG_PENDING;
  }

  return true;
}

void Bus_Node::service() {
  if (!_transport) {
    return;
  }

  const uint32_t nowMs = millis();

  while (_transport->available() > 0) {
    int c = _transport->read();
    if (c < 0) {
      break;
    }

    processIncomingByte(static_cast<uint8_t>(c), nowMs);
  }
}

uint8_t Bus_Node::getAddress() const {
  return _address;
}

const uint8_t* Bus_Node::getMac() const {
  return _mac;
}

void Bus_Node::setFirmwareVersion(uint8_t majorVersion, uint8_t minorVersion) {
  _fwMajor = majorVersion;
  _fwMinor = minorVersion;
}

void Bus_Node::setCapabilities(uint8_t capabilityFlags) {
  _capabilities = capabilityFlags;
}

uint8_t Bus_Node::getStatusFlags() const {
  return _statusFlags;
}

void Bus_Node::setStatusFlag(uint8_t flag, bool enabled) {
  if (enabled) {
    _statusFlags |= flag;
  } else {
    _statusFlags &= ~flag;
  }
}

void Bus_Node::setErrorCode(uint8_t errorCode) {
  _errorCode = errorCode;
  setStatusFlag(BUS_STATUS_ERROR_LATCHED, (_errorCode != 0));
}

uint8_t Bus_Node::getErrorCode() const {
  return _errorCode;
}

size_t Bus_Node::debugPrint(const char* text) {
  if (!text) {
    return 0;
  }
  return pushDebugBytes(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

size_t Bus_Node::debugWrite(const uint8_t* data, size_t len) {
  return pushDebugBytes(data, len);
}

uint16_t Bus_Node::getDebugBytesPending() const {
  return _debugCount;
}

bool Bus_Node::hasDebugPending() const {
  return _debugCount > 0;
}

bool Bus_Node::didDebugOverflow() const {
  return _debugOverflow;
}

void Bus_Node::clearDebugOverflow() {
  _debugOverflow = false;
  setStatusFlag(BUS_STATUS_DEBUG_OVERFLOW, false);
}

void Bus_Node::setCommandHandler(CommandHandler handler) {
  _commandHandler = handler;
}

void Bus_Node::setDisplayReady(bool ready) {
  setStatusFlag(BUS_STATUS_DISPLAY_READY, ready);
}

void Bus_Node::readMacAddress() {
#if defined(ESP32)
  esp_read_mac(_mac, ESP_MAC_WIFI_STA);
#else
  memset(_mac, 0, sizeof(_mac));
#endif
}

uint8_t Bus_Node::deriveAddressFromMac() const {
  return bus_node_id_from_mac(_mac);
}

void Bus_Node::parserReset() {
  bus_parser_reset(&_parser);
}

void Bus_Node::processIncomingByte(uint8_t byteIn, uint32_t nowMs) {
  if (bus_parser_feed(&_parser, byteIn, nowMs)) {
    handleFrame(_parser.frame);
  }
}

bool Bus_Node::isFrameForMe(const bus_frame_t& frame) const {
  if (frame.dst == _address) {
    return true;
  }

  if (frame.dst == BUS_BROADCAST_ADDR) {
    return true;
  }

  return false;
}

bool Bus_Node::isDuplicateRequest(const bus_frame_t& frame) const {
  if (!_hasLastRequest) {
    return false;
  }

  return (frame.src  == _lastRequestSrc)  &&
         (frame.type == _lastRequestType) &&
         (frame.seq  == _lastRequestSeq);
}

void Bus_Node::rememberRequest(const bus_frame_t& frame) {
  _lastRequestSrc  = frame.src;
  _lastRequestType = frame.type;
  _lastRequestSeq  = frame.seq;
  _hasLastRequest  = true;
}

bool Bus_Node::sendFrame(const bus_frame_t& frame) {
  if (!_transport) {
    return false;
  }

  uint8_t raw[BUS_MAX_FRAME_SIZE];
  size_t encodedLen = bus_encode_frame(raw, sizeof(raw), &frame);
  if (encodedLen == 0) {
    return false;
  }

  _transport->writeRaw(raw, encodedLen);
  return true;
}

bool Bus_Node::sendSimpleFrame(uint8_t dst,
                               uint8_t type,
                               uint8_t seq,
                               const uint8_t* payload,
                               uint8_t len) {
  if (len > BUS_MAX_PAYLOAD_SIZE) {
    return false;
  }

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.dst = dst;
  frame.src = _address;
  frame.type = type;
  frame.seq = seq;
  frame.len = len;

  if (payload && len > 0) {
    memcpy(frame.payload, payload, len);
  }

  return sendFrame(frame);
}

bool Bus_Node::sendPong(uint8_t dst, uint8_t seq) {
  uint8_t payload[1];
  payload[0] = _statusFlags;

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dst = dst;
  frame.src = _address;
  frame.type = BUS_MSG_PONG;
  frame.seq = seq;
  frame.len = sizeof(payload);
  memcpy(frame.payload, payload, sizeof(payload));

  bool ok = sendFrame(frame);
  if (ok) {
    cacheLastResponse(frame);
  }
  return ok;
}

bool Bus_Node::sendEnumResponse(uint8_t dst, uint8_t seq) {
  bus_enum_response_t resp;
  memset(&resp, 0, sizeof(resp));

  resp.node_id = _address;
  memcpy(resp.mac, _mac, sizeof(_mac));
  resp.fw_major = _fwMajor;
  resp.fw_minor = _fwMinor;
  resp.capabilities = _capabilities;

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dst = dst;
  frame.src = _address;
  frame.type = BUS_MSG_ENUM_RESPONSE;
  frame.seq = seq;
  frame.len = sizeof(resp);
  memcpy(frame.payload, &resp, sizeof(resp));

  bool ok = sendFrame(frame);
  if (ok) {
    cacheLastResponse(frame);
  }
  return ok;
}

bool Bus_Node::sendStatusResponse(uint8_t dst, uint8_t seq) {
  bus_status_response_t resp;
  memset(&resp, 0, sizeof(resp));

  resp.flags = _statusFlags;
  resp.debug_bytes = _debugCount;
  resp.error_code = _errorCode;
  resp.uptime = millis() - _bootMillis;

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dst = dst;
  frame.src = _address;
  frame.type = BUS_MSG_STATUS_RESPONSE;
  frame.seq = seq;
  frame.len = sizeof(resp);
  memcpy(frame.payload, &resp, sizeof(resp));

  bool ok = sendFrame(frame);
  if (ok) {
    cacheLastResponse(frame);
  }
  return ok;
}

bool Bus_Node::sendCommandAck(uint8_t dst, uint8_t seq, uint8_t commandId, uint8_t resultCode) {
  uint8_t payload[2];
  payload[0] = commandId;
  payload[1] = resultCode;

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dst = dst;
  frame.src = _address;
  frame.type = BUS_MSG_COMMAND_ACK;
  frame.seq = seq;
  frame.len = sizeof(payload);
  memcpy(frame.payload, payload, sizeof(payload));

  bool ok = sendFrame(frame);
  if (ok) {
    cacheLastResponse(frame);
  }
  return ok;
}

bool Bus_Node::sendCommandNack(uint8_t dst, uint8_t seq, uint8_t commandId, uint8_t errorCode) {
  uint8_t payload[2];
  payload[0] = commandId;
  payload[1] = errorCode;

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dst = dst;
  frame.src = _address;
  frame.type = BUS_MSG_COMMAND_NACK;
  frame.seq = seq;
  frame.len = sizeof(payload);
  memcpy(frame.payload, payload, sizeof(payload));

  bool ok = sendFrame(frame);
  if (ok) {
    cacheLastResponse(frame);
  }
  return ok;
}

bool Bus_Node::sendDebugData(uint8_t dst, uint8_t seq, uint8_t maxBytesRequested) {
  const uint16_t maxTextBytes = BUS_MAX_PAYLOAD_SIZE - 2;
  uint16_t requested = maxBytesRequested;
  if (requested > maxTextBytes) {
    requested = maxTextBytes;
  }

  uint8_t payload[BUS_MAX_PAYLOAD_SIZE];
  memset(payload, 0, sizeof(payload));

  uint16_t count = popDebugBytes(&payload[2], requested);
  payload[0] = static_cast<uint8_t>(count);
  payload[1] = (_debugCount > 0) ? 1 : 0;

  bus_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.dst = dst;
  frame.src = _address;
  frame.type = BUS_MSG_DEBUG_DATA;
  frame.seq = seq;
  frame.len = static_cast<uint8_t>(count + 2);
  memcpy(frame.payload, payload, frame.len);

  bool ok = sendFrame(frame);
  if (ok) {
    cacheLastResponse(frame);
  }
  return ok;
}

void Bus_Node::cacheLastResponse(const bus_frame_t& frame) {
  _lastResponseFrame = frame;
  _hasLastResponse = true;
}

void Bus_Node::handleFrame(const bus_frame_t& frame) {
  if (!isFrameForMe(frame)) {
    return;
  }

  // Slaves never reply to broadcast in v1.
  if (frame.dst == BUS_BROADCAST_ADDR) {
    return;
  }

  if (frame.src != BUS_MASTER_ADDR) {
    return;
  }

  if (isDuplicateRequest(frame)) {
    if (_hasLastResponse) {
      sendFrame(_lastResponseFrame);
    }
    return;
  }

  rememberRequest(frame);

  switch (frame.type) {
    case BUS_MSG_PING:
      sendPong(frame.src, frame.seq);
      break;

    case BUS_MSG_ENUM_REQUEST:
      sendEnumResponse(frame.src, frame.seq);
      break;

    case BUS_MSG_STATUS_REQUEST:
      sendStatusResponse(frame.src, frame.seq);
      break;

    case BUS_MSG_DEBUG_REQUEST:
      if (frame.len >= 1) {
        sendDebugData(frame.src, frame.seq, frame.payload[0]);
      } else {
        sendDebugData(frame.src, frame.seq, BUS_MAX_PAYLOAD_SIZE - 2);
      }
      break;

    case BUS_MSG_COMMAND:
      if (frame.len < 1) {
        sendCommandNack(frame.src, frame.seq, 0, BUS_RESULT_INVALID);
        break;
      }

      if (_commandHandler) {
        uint8_t commandId = frame.payload[0];
        const uint8_t* commandData = &frame.payload[1];
        uint8_t commandLen = static_cast<uint8_t>(frame.len - 1);

        _commandHandler(commandId, commandData, commandLen);
        sendCommandAck(frame.src, frame.seq, commandId, BUS_RESULT_OK);
      } else {
        sendCommandNack(frame.src, frame.seq, frame.payload[0], BUS_RESULT_UNSUPPORTED);
      }
      break;

    default:
      // Unknown request type addressed to this node.
      break;
  }
}

size_t Bus_Node::pushDebugBytes(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return 0;
  }

  size_t written = 0;

  for (size_t i = 0; i < len; ++i) {
    if (_debugCount >= BUS_NODE_DEBUG_BUFFER_SIZE) {
      // Drop oldest byte to make room.
      _debugTail = static_cast<uint16_t>((_debugTail + 1) % BUS_NODE_DEBUG_BUFFER_SIZE);
      _debugCount--;
      _debugOverflow = true;
      setStatusFlag(BUS_STATUS_DEBUG_OVERFLOW, true);
    }

    _debugBuffer[_debugHead] = data[i];
    _debugHead = static_cast<uint16_t>((_debugHead + 1) % BUS_NODE_DEBUG_BUFFER_SIZE);
    _debugCount++;
    written++;
  }

  setStatusFlag(BUS_STATUS_DEBUG_PENDING, (_debugCount > 0));
  return written;
}

uint16_t Bus_Node::popDebugBytes(uint8_t* out, uint16_t maxLen) {
  if (!out || maxLen == 0) {
    return 0;
  }

  uint16_t count = 0;

  while ((_debugCount > 0) && (count < maxLen)) {
    out[count++] = _debugBuffer[_debugTail];
    _debugTail = static_cast<uint16_t>((_debugTail + 1) % BUS_NODE_DEBUG_BUFFER_SIZE);
    _debugCount--;
  }

  setStatusFlag(BUS_STATUS_DEBUG_PENDING, (_debugCount > 0));
  return count;
}