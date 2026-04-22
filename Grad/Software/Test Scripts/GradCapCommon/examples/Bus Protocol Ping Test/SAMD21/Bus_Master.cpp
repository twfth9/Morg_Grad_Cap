#include "Bus_Master.h"
#include <Arduino.h>
#include "SAMD21_BusPhy.h"

#include <stdio.h>
#include <string.h>

namespace {

bool copy_bounded_filename(char* dst, size_t dst_size, const char* src) {
  if (dst == nullptr || dst_size == 0 || src == nullptr) {
    return false;
  }

  const size_t src_len = strnlen(src, dst_size - 1);
  memcpy(dst, src, src_len);
  dst[src_len] = '\0';
  return true;
}

int find_first_free_asset_slot(bus_asset_info_t* assets, size_t count) {
  if (assets == nullptr) {
    return -1;
  }

  for (size_t i = 0; i < count; ++i) {
    if (!assets[i].valid) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace

BusMaster::BusMaster()
    : active_node_count_(0),
      last_tx_owner_index_(0),
      waiting_for_tx_report_(false),
      tx_report_wait_start_ms_(0),
      tx_report_timeout_ms_(BUS_DEFAULT_REPLY_TIMEOUT_MS),
      verbose_(false),
      last_frame_src_(0),
      last_frame_type_(0),
      last_pong_src_(0),
      last_pong_rx_us_(0),
      last_pong_valid_(false) {
  memset(&parser_, 0, sizeof(parser_));
  memset(&bus_state_, 0, sizeof(bus_state_));
  memset(nodes_, 0, sizeof(nodes_));
  memset(assets_, 0, sizeof(assets_));
  memset(enum_seen_, 0, sizeof(enum_seen_));
  memset(pong_seen_, 0, sizeof(pong_seen_));
  memset(tx_report_seen_, 0, sizeof(tx_report_seen_));
  memset(node_psram_used_, 0, sizeof(node_psram_used_));
  memset(node_psram_total_, 0, sizeof(node_psram_total_));
  memset(node_flash_used_, 0, sizeof(node_flash_used_));
  memset(node_flash_total_, 0, sizeof(node_flash_total_));
  asset_count_ = 0;
  bus_parser_reset(&parser_);
}

void BusMaster::begin() {
  bus_parser_reset(&parser_);
  clearNodeTable();
  clearAssetCatalog();

  memset(&bus_state_, 0, sizeof(bus_state_));
  bus_state_.tx_owner = 0;
  bus_state_.sd_owner = 0;
  bus_state_.state_seq = 0;
  bus_state_.flags = BUS_STATE_FLAG_STARTUP;

  active_node_count_ = 0;
  last_tx_owner_index_ = 0;
  waiting_for_tx_report_ = false;
  tx_report_wait_start_ms_ = 0;
  tx_report_timeout_ms_ = BUS_DEFAULT_REPLY_TIMEOUT_MS;
  verbose_ = false;
  last_frame_src_ = 0;
  last_frame_type_ = 0;
  last_pong_src_ = 0;
  last_pong_rx_us_ = 0;
  last_pong_valid_ = false;
  memset(enum_seen_, 0, sizeof(enum_seen_));
  memset(pong_seen_, 0, sizeof(pong_seen_));
  memset(tx_report_seen_, 0, sizeof(tx_report_seen_));
  memset(node_psram_used_, 0, sizeof(node_psram_used_));
  memset(node_psram_total_, 0, sizeof(node_psram_total_));
  memset(node_flash_used_, 0, sizeof(node_flash_used_));
  memset(node_flash_total_, 0, sizeof(node_flash_total_));
}

void BusMaster::setVerbose(bool enabled) { verbose_ = enabled; }
bool BusMaster::isVerbose() const { return verbose_; }

bool BusMaster::processIncomingByte(uint8_t byte, uint32_t now_ms) {
  if (!bus_parser_feed(&parser_, byte, now_ms)) {
    return false;
  }

  const bus_frame_t frame = parser_.frame;
  parser_.frame_ready = 0;
  return handleFrame(frame);
}

void BusMaster::resetParser() {
  bus_parser_reset(&parser_);
}

bool BusMaster::handleFrame(const bus_frame_t& frame) {
  last_frame_src_ = frame.src;
  last_frame_type_ = frame.type;

  maybeLogFrame(frame);

  switch (frame.type) {
    case BUS_MSG_PONG:
      return handlePong(frame, millis());

    case BUS_MSG_ENUM_RESPONSE:
      return handleEnumResponse(frame, millis());

    case BUS_MSG_TX_REPORT:
      return handleTxReport(frame, millis());

    case BUS_MSG_DEBUG_TEXT:
      return handleDebugText(frame, millis());

    case BUS_MSG_COMMAND_ACK:
      return handleCommandAck(frame, millis());

    case BUS_MSG_COMMAND_NACK:
      return handleCommandNack(frame, millis());

    case BUS_MSG_ERROR_REPORT:
      return handleErrorReport(frame, millis());

    case BUS_MSG_APP_DATA:
      return handleAppData(frame, millis());

    default:
      return false;
  }
}

void BusMaster::tick(uint32_t now_ms) {
  if (waiting_for_tx_report_ && txReportTimedOut(now_ms)) {
    requestRetransmitForCurrentNode();
    advanceStateSequence();
  }
}

const bus_bus_state_t& BusMaster::getBusState() const {
  return bus_state_;
}

void BusMaster::setTxOwner(uint8_t node_id) {
  bus_state_.tx_owner = node_id;
}

uint8_t BusMaster::getTxOwner() const {
  return bus_state_.tx_owner;
}

void BusMaster::setSdOwner(uint8_t node_id) {
  bus_state_.sd_owner = node_id;
  syncSdBusyFlag();
}

uint8_t BusMaster::getSdOwner() const {
  return bus_state_.sd_owner;
}

void BusMaster::setRetransmitRequested(bool requested) {
  if (requested) {
    bus_state_.flags |= BUS_STATE_FLAG_RETRANSMIT;
  } else {
    bus_state_.flags &= static_cast<uint8_t>(~BUS_STATE_FLAG_RETRANSMIT);
  }
}

bool BusMaster::isRetransmitRequested() const {
  return (bus_state_.flags & BUS_STATE_FLAG_RETRANSMIT) != 0;
}

void BusMaster::setStartupPhase(bool startup) {
  if (startup) {
    bus_state_.flags |= BUS_STATE_FLAG_STARTUP;
  } else {
    bus_state_.flags &= static_cast<uint8_t>(~BUS_STATE_FLAG_STARTUP);
  }
}

bool BusMaster::isStartupPhase() const {
  return (bus_state_.flags & BUS_STATE_FLAG_STARTUP) != 0;
}

void BusMaster::setErrorLatched(bool latched) {
  if (latched) {
    bus_state_.flags |= BUS_STATE_FLAG_ERROR;
  } else {
    bus_state_.flags &= static_cast<uint8_t>(~BUS_STATE_FLAG_ERROR);
  }
}

bool BusMaster::isErrorLatched() const {
  return (bus_state_.flags & BUS_STATE_FLAG_ERROR) != 0;
}

uint16_t BusMaster::advanceStateSequence() {
  ++bus_state_.state_seq;
  return bus_state_.state_seq;
}

size_t BusMaster::buildBusStateFrame(uint8_t* out, size_t out_size) {
  if (out == nullptr || out_size == 0) {
    return 0;
  }

  syncSdBusyFlag();

  bus_frame_t frame{};
  frame.dst = BUS_BROADCAST_ADDR;
  frame.src = BUS_MASTER_ADDR;
  frame.type = BUS_MSG_BUS_STATE;
  frame.seq = static_cast<uint8_t>(bus_state_.state_seq & 0xFFu);
  frame.len = sizeof(bus_bus_state_t);
  memcpy(frame.payload, &bus_state_, sizeof(bus_bus_state_t));

  return bus_encode_frame(out, out_size, &frame);
}

size_t BusMaster::getActiveNodeCount() const {
  return active_node_count_;
}

bus_node_info_t* BusMaster::findNode(uint8_t node_id) {
  for (size_t i = 0; i < kMaxNodes; ++i) {
    if (nodes_[i].active && nodes_[i].addr == node_id) {
      return &nodes_[i];
    }
  }
  return nullptr;
}

const bus_node_info_t* BusMaster::findNode(uint8_t node_id) const {
  for (size_t i = 0; i < kMaxNodes; ++i) {
    if (nodes_[i].active && nodes_[i].addr == node_id) {
      return &nodes_[i];
    }
  }
  return nullptr;
}

void BusMaster::markNodeSeen(uint8_t node_id, uint32_t now_ms) {
  bus_node_info_t* node = upsertNode(node_id);
  if (node == nullptr) {
    return;
  }

  node->online = 1;
  node->last_seen_ms = now_ms;
}

bool BusMaster::updateNodeFromEnumResponse(const bus_enum_response_t& info, uint32_t now_ms) {
  bus_node_info_t* node = upsertNode(info.node_id);
  if (node == nullptr) {
    return false;
  }

  node->online = 1;
  node->addr = info.node_id;
  memcpy(node->mac, info.mac, sizeof(node->mac));
  node->protocol_major = info.protocol_major;
  node->protocol_minor = info.protocol_minor;
  node->fw_major = info.fw_major;
  node->fw_minor = info.fw_minor;
  node->capabilities = info.capabilities;
  node->last_seen_ms = now_ms;

  const int slot = findNodeSlot(info.node_id);
  if (slot >= 0) {
    enum_seen_[slot] = true;
  }

  return true;
}

bool BusMaster::updateNodeFromTxReport(uint8_t src_node,
                                       const bus_tx_report_t& report,
                                       uint32_t now_ms) {
  bus_node_info_t* node = upsertNode(src_node);
  if (node == nullptr) {
    return false;
  }

  node->online = 1;
  node->addr = src_node;
  node->last_status_bits = report.status_bits;
  node->last_error_code = report.error_code;
  node->last_uptime_ms = report.uptime_ms;
  node->last_seen_ms = now_ms;

  const int slot = findNodeSlot(src_node);
  if (slot >= 0) {
    tx_report_seen_[slot] = true;
    node_psram_used_[slot] = report.psram_used_bytes;
    node_psram_total_[slot] = report.psram_total_bytes;
    node_flash_used_[slot] = report.flash_used_bytes;
    node_flash_total_[slot] = report.flash_total_bytes;
  }

  /*
   * If the current authoritative SD owner reports that the card is now free,
   * reflect that in the master's authoritative state.
   */
  if (src_node == bus_state_.sd_owner && report.reported_sd_owner == 0) {
    setSdOwner(0);
  }

  if (src_node == bus_state_.sd_owner) {
    bus_asset_info_t* asset = findAssignedAssetForNode(src_node);
    if (asset != nullptr) {
      const uint8_t asset_state = static_cast<uint8_t>(BUS_STATUS_GET_ASSET(report.status_bits));
      if (asset_state == BUS_ASSET_STATUS_LOADED ||
          asset_state == BUS_ASSET_STATUS_DISPLAYING) {
        asset->load_confirmed = true;
      }
    }
  }

  return true;
}

void BusMaster::clearNodeTable() {
  memset(nodes_, 0, sizeof(nodes_));
  active_node_count_ = 0;
}

void BusMaster::clearAssetCatalog() {
  memset(assets_, 0, sizeof(assets_));
  asset_count_ = 0;
}

size_t BusMaster::getAssetCount() const {
  return asset_count_;
}

bool BusMaster::addAsset(const char* filename, uint32_t size_bytes, uint8_t format) {
  if (filename == nullptr || filename[0] == '\0') {
    return false;
  }

  int slot = find_first_free_asset_slot(assets_, kMaxAssets);
  if (slot < 0) {
    return false;
  }

  bus_asset_info_t& asset = assets_[slot];
  memset(&asset, 0, sizeof(asset));
  asset.valid = true;
  if (!copy_bounded_filename(asset.filename, sizeof(asset.filename), filename)) {
    asset.valid = false;
    return false;
  }
  asset.size_bytes = size_bytes;
  asset.format = format;
  asset.assigned_node = 0;
  asset.assigned = false;
  asset.load_requested = false;
  asset.load_confirmed = false;

  if (static_cast<size_t>(slot + 1) > asset_count_) {
    asset_count_ = static_cast<size_t>(slot + 1);
  }

  return true;
}

bus_asset_info_t* BusMaster::findAssetByFilename(const char* filename) {
  if (filename == nullptr) {
    return nullptr;
  }

  for (size_t i = 0; i < kMaxAssets; ++i) {
    if (assets_[i].valid && strcmp(assets_[i].filename, filename) == 0) {
      return &assets_[i];
    }
  }
  return nullptr;
}

const bus_asset_info_t* BusMaster::findAssetByFilename(const char* filename) const {
  if (filename == nullptr) {
    return nullptr;
  }

  for (size_t i = 0; i < kMaxAssets; ++i) {
    if (assets_[i].valid && strcmp(assets_[i].filename, filename) == 0) {
      return &assets_[i];
    }
  }
  return nullptr;
}

bus_asset_info_t* BusMaster::findAssignedAssetForNode(uint8_t node_id) {
  for (size_t i = 0; i < kMaxAssets; ++i) {
    if (assets_[i].valid && assets_[i].assigned && assets_[i].assigned_node == node_id) {
      return &assets_[i];
    }
  }
  return nullptr;
}

const bus_asset_info_t* BusMaster::findAssignedAssetForNode(uint8_t node_id) const {
  for (size_t i = 0; i < kMaxAssets; ++i) {
    if (assets_[i].valid && assets_[i].assigned && assets_[i].assigned_node == node_id) {
      return &assets_[i];
    }
  }
  return nullptr;
}

bool BusMaster::assignAssetToNode(const char* filename, uint8_t node_id) {
  if (!isValidNodeId(node_id)) {
    return false;
  }

  bus_asset_info_t* asset = findAssetByFilename(filename);
  if (asset == nullptr) {
    return false;
  }

  clearAssetAssignmentForNode(node_id);
  asset->assigned_node = node_id;
  asset->assigned = true;
  asset->load_requested = false;
  asset->load_confirmed = false;
  return true;
}

void BusMaster::clearAssetAssignmentForNode(uint8_t node_id) {
  for (size_t i = 0; i < kMaxAssets; ++i) {
    if (assets_[i].valid && assets_[i].assigned && assets_[i].assigned_node == node_id) {
      assets_[i].assigned_node = 0;
      assets_[i].assigned = false;
      assets_[i].load_requested = false;
      assets_[i].load_confirmed = false;
    }
  }
}

void BusMaster::markAssetLoadRequested(uint8_t node_id) {
  bus_asset_info_t* asset = findAssignedAssetForNode(node_id);
  if (asset != nullptr) {
    asset->load_requested = true;
  }
}

void BusMaster::markAssetLoadConfirmed(uint8_t node_id) {
  bus_asset_info_t* asset = findAssignedAssetForNode(node_id);
  if (asset != nullptr) {
    asset->load_confirmed = true;
  }
}

uint8_t BusMaster::chooseNextTxOwner() const {
  if (active_node_count_ == 0) {
    return 0;
  }

  for (size_t step = 1; step <= kMaxNodes; ++step) {
    const size_t index = (last_tx_owner_index_ + step) % kMaxNodes;
    if (nodes_[index].active && nodes_[index].online) {
      return nodes_[index].addr;
    }
  }

  return 0;
}

uint8_t BusMaster::advanceToNextTxOwner() {
  const uint8_t next = chooseNextTxOwner();
  setTxOwner(next);

  if (next != 0) {
    for (size_t i = 0; i < kMaxNodes; ++i) {
      if (nodes_[i].active && nodes_[i].addr == next) {
        last_tx_owner_index_ = i;
        break;
      }
    }
  }

  return next;
}

void BusMaster::requestRetransmitForCurrentNode() {
  setRetransmitRequested(true);
}

void BusMaster::clearRetransmitRequest() {
  setRetransmitRequested(false);
}

void BusMaster::startWaitingForTxReport(uint32_t now_ms) {
  waiting_for_tx_report_ = true;
  tx_report_wait_start_ms_ = now_ms;
}

bool BusMaster::isWaitingForTxReport() const {
  return waiting_for_tx_report_;
}

bool BusMaster::txReportTimedOut(uint32_t now_ms) const {
  if (!waiting_for_tx_report_) {
    return false;
  }
  return (now_ms - tx_report_wait_start_ms_) > tx_report_timeout_ms_;
}

void BusMaster::clearTxReportWait() {
  waiting_for_tx_report_ = false;
}

size_t BusMaster::buildPingFrame(uint8_t node_id, uint8_t* out, size_t out_size) {
  if (out == nullptr || out_size == 0 || !isValidNodeId(node_id)) {
    return 0;
  }

  bus_frame_t frame{};
  frame.dst = node_id;
  frame.src = BUS_MASTER_ADDR;
  frame.type = BUS_MSG_PING;
  frame.seq = static_cast<uint8_t>(bus_state_.state_seq & 0xFFu);
  frame.len = 0;
  return bus_encode_frame(out, out_size, &frame);
}

size_t BusMaster::buildEnumRequestFrame(uint8_t node_id, uint8_t* out, size_t out_size) {
  if (out == nullptr || out_size == 0) {
    return 0;
  }

  if (node_id != BUS_BROADCAST_ADDR && !isValidNodeId(node_id)) {
    return 0;
  }

  bus_frame_t frame{};
  frame.dst = node_id;
  frame.src = BUS_MASTER_ADDR;
  frame.type = BUS_MSG_ENUM_REQUEST;
  frame.seq = static_cast<uint8_t>(bus_state_.state_seq & 0xFFu);
  frame.len = 0;
  return bus_encode_frame(out, out_size, &frame);
}

size_t BusMaster::buildLoadAssetFrame(uint8_t node_id,
                                      const char* filename,
                                      uint8_t storage_target,
                                      uint8_t flags,
                                      uint8_t* out,
                                      size_t out_size) {
  if (out == nullptr || out_size == 0 || filename == nullptr || !isValidNodeId(node_id)) {
    return 0;
  }

  const size_t filename_len = strnlen(filename, BUS_MAX_PAYLOAD_SIZE - 3);
  if (filename_len == 0 || filename_len > (BUS_MAX_PAYLOAD_SIZE - 3)) {
    return 0;
  }

  bus_load_asset_t payload{};
  payload.storage_target = storage_target;
  payload.flags = flags;
  payload.filename_len = static_cast<uint8_t>(filename_len);
  memcpy(payload.filename, filename, filename_len);

  bus_frame_t frame{};
  frame.dst = node_id;
  frame.src = BUS_MASTER_ADDR;
  frame.type = BUS_MSG_LOAD_ASSET;
  frame.seq = static_cast<uint8_t>(bus_state_.state_seq & 0xFFu);
  frame.len = static_cast<uint8_t>(3 + filename_len);
  memcpy(frame.payload, &payload, frame.len);

  return bus_encode_frame(out, out_size, &frame);
}

bool BusMaster::handlePong(const bus_frame_t& frame, uint32_t now_ms) {
  markNodeSeen(frame.src, now_ms);
  last_pong_src_ = frame.src;
  last_pong_rx_us_ = micros();
  last_pong_valid_ = true;
  const int slot = findNodeSlot(frame.src);
  if (slot >= 0) { pong_seen_[slot] = true; }
  return true;
}

uint8_t BusMaster::getLastPongSource() const { return last_pong_src_; }
uint32_t BusMaster::getLastPongRxUs() const { return last_pong_rx_us_; }
bool BusMaster::hasLastPong() const { return last_pong_valid_; }
void BusMaster::clearLastPong() { last_pong_valid_ = false; }

bool BusMaster::handleEnumResponse(const bus_frame_t& frame, uint32_t now_ms) {
  if (frame.len != sizeof(bus_enum_response_t)) {
    return false;
  }

  bus_enum_response_t info{};
  memcpy(&info, frame.payload, sizeof(info));
  return updateNodeFromEnumResponse(info, now_ms);
}

bool BusMaster::handleTxReport(const bus_frame_t& frame, uint32_t now_ms) {
  if (frame.len != sizeof(bus_tx_report_t)) {
    requestRetransmitForCurrentNode();
    return false;
  }

  bus_tx_report_t report{};
  memcpy(&report, frame.payload, sizeof(report));

  if (!updateNodeFromTxReport(frame.src, report, now_ms)) {
    requestRetransmitForCurrentNode();
    return false;
  }

  acceptCurrentTxReport(frame.src, now_ms);
  return true;
}

bool BusMaster::handleDebugText(const bus_frame_t& frame, uint32_t /*now_ms*/) {
  if (frame.len < 2) {
    return false;
  }

  bus_debug_text_t msg{};
  msg.level = frame.payload[0];
  msg.text_len = frame.payload[1];

  if (msg.text_len > (frame.len - 2) || msg.text_len > sizeof(msg.text)) {
    return false;
  }

  if (msg.text_len > 0) {
    memcpy(msg.text, &frame.payload[2], msg.text_len);
  }

  presentDebugText(frame.src, msg);
  markNodeSeen(frame.src, millis());
  return true;
}

bool BusMaster::handleCommandAck(const bus_frame_t& frame, uint32_t now_ms) {
  markNodeSeen(frame.src, now_ms);
  return frame.len == sizeof(bus_command_ack_t);
}

bool BusMaster::handleCommandNack(const bus_frame_t& frame, uint32_t now_ms) {
  markNodeSeen(frame.src, now_ms);
  return frame.len == sizeof(bus_command_nack_t);
}

bool BusMaster::handleErrorReport(const bus_frame_t& frame, uint32_t now_ms) {
  markNodeSeen(frame.src, now_ms);
  if (frame.len != sizeof(bus_error_report_t)) {
    return false;
  }

  bus_error_report_t report{};
  memcpy(&report, frame.payload, sizeof(report));

  bus_node_info_t* node = upsertNode(frame.src);
  if (node != nullptr) {
    node->last_error_code = report.error_code;
  }
  setErrorLatched(true);
  return true;
}

bool BusMaster::handleAppData(const bus_frame_t& frame, uint32_t now_ms) {
  markNodeSeen(frame.src, now_ms);
  return frame.len >= 1;
}

void BusMaster::presentDebugText(uint8_t src_node, const bus_debug_text_t& msg) {
  char line[128];
  const size_t text_len = (msg.text_len < sizeof(msg.text)) ? msg.text_len : sizeof(msg.text);
  char text_buf[sizeof(msg.text) + 1];
  memset(text_buf, 0, sizeof(text_buf));
  if (text_len > 0) {
    memcpy(text_buf, msg.text, text_len);
  }

  if (!verbose_) { return; }
  snprintf(line, sizeof(line), "[%u] %s\n", static_cast<unsigned>(src_node), text_buf);
  Serial.print(line);
}

bus_node_info_t* BusMaster::upsertNode(uint8_t node_id) {
  if (!isValidNodeId(node_id)) {
    return nullptr;
  }

  bus_node_info_t* existing = findNode(node_id);
  if (existing != nullptr) {
    return existing;
  }

  for (size_t i = 0; i < kMaxNodes; ++i) {
    if (!nodes_[i].active) {
      memset(&nodes_[i], 0, sizeof(nodes_[i]));
      nodes_[i].active = 1;
      nodes_[i].online = 0;
      nodes_[i].addr = node_id;
      ++active_node_count_;
      return &nodes_[i];
    }
  }

  return nullptr;
}

bool BusMaster::isValidNodeId(uint8_t node_id) const {
  return node_id != 0 && node_id != BUS_BROADCAST_ADDR;
}

void BusMaster::syncSdBusyFlag() {
  if (bus_state_.sd_owner != 0) {
    bus_state_.flags |= BUS_STATE_FLAG_SD_BUSY;
  } else {
    bus_state_.flags &= static_cast<uint8_t>(~BUS_STATE_FLAG_SD_BUSY);
  }
}

void BusMaster::acceptCurrentTxReport(uint8_t src_node, uint32_t now_ms) {
  if (src_node != bus_state_.tx_owner) {
    return;
  }

  clearRetransmitRequest();
  clearTxReportWait();
  markNodeSeen(src_node, now_ms);
}



int BusMaster::findNodeSlot(uint8_t node_id) const {
  for (size_t i = 0; i < kMaxNodes; ++i) {
    if (nodes_[i].active && nodes_[i].addr == node_id) return static_cast<int>(i);
  }
  return -1;
}

void BusMaster::resetPingTrackingForNode(uint8_t node_id) {
  const int slot = findNodeSlot(node_id);
  if (slot >= 0) {
    enum_seen_[slot] = false;
    pong_seen_[slot] = false;
    tx_report_seen_[slot] = false;
    node_psram_used_[slot] = 0;
    node_psram_total_[slot] = 0;
    node_flash_used_[slot] = 0;
    node_flash_total_[slot] = 0;
  }
}

void BusMaster::fillPingResultFromNode(uint8_t node_id, uint8_t attempts_used, uint32_t ping_rtt_us,
                                       uint8_t data_flags, bus_ping_test_result_t* out_result) const {
  if (out_result == nullptr) return;

  memset(out_result, 0, sizeof(*out_result));
  out_result->node_id = node_id;
  out_result->attempts_used = attempts_used;
  out_result->ping_rtt_us = ping_rtt_us;

  const bus_node_info_t* node = findNode(node_id);
  const int slot = findNodeSlot(node_id);

  if (slot >= 0) {
    out_result->enum_seen = enum_seen_[slot];
    out_result->pong_seen = pong_seen_[slot];
    out_result->tx_report_seen = tx_report_seen_[slot];

    if ((data_flags & BUS_PING_DATA_MEMORY) != 0) {
      out_result->psram_used_bytes = node_psram_used_[slot];
      out_result->psram_total_bytes = node_psram_total_[slot];
      out_result->flash_used_bytes = node_flash_used_[slot];
      out_result->flash_total_bytes = node_flash_total_[slot];
    }
  }

  if (node != nullptr) {
    out_result->status_bits = node->last_status_bits;
    out_result->error_code = node->last_error_code;
    out_result->uptime_ms = node->last_uptime_ms;
  }

  /* The caller decides final pass/fail because enum may be optional. */
  out_result->passed = false;
}

void BusMaster::servicePhyRx(SAMD21_BusPhy& phy, uint32_t now_ms) {
  while (phy.available() > 0) {
    int value = phy.read();
    if (value < 0) break;
    processIncomingByte(static_cast<uint8_t>(value), now_ms);
  }
}

void BusMaster::maybeLogFrame(const bus_frame_t& frame) const {
  if (!verbose_) return;
  Serial.print(F("RX frame: src="));
  Serial.print(frame.src);
  Serial.print(F(" dst="));
  Serial.print(frame.dst);
  Serial.print(F(" type=0x"));
  Serial.print(frame.type, HEX);
  Serial.print(F(" len="));
  Serial.println(frame.len);
}

bool BusMaster::runPingTestForNode(SAMD21_BusPhy& phy,
                                   uint8_t node_id,
                                   const bus_ping_test_options_t& options,
                                   bus_ping_test_result_t* out_result) {
  if (!isValidNodeId(node_id) || out_result == nullptr) return false;

  const bool old_verbose = verbose_;
  verbose_ = options.verbose;

  uint8_t attempts = 0;
  bool passed = false;

  for (attempts = 1; attempts <= options.max_attempts_per_node; ++attempts) {
    uint32_t ping_rtt_us = 0;

    clearLastPong();
    resetPingTrackingForNode(node_id);

    if (verbose_) {
      Serial.println();
      Serial.print(F("Pinging node "));
      Serial.println(node_id);
    }

    uint8_t frame_buf[BUS_MAX_FRAME_SIZE];

    if (options.request_enum) {
      const size_t enum_len = buildEnumRequestFrame(node_id, frame_buf, sizeof(frame_buf));
      if (enum_len > 0) {
        phy.write(frame_buf, enum_len);
      }
    }

    const size_t ping_len = buildPingFrame(node_id, frame_buf, sizeof(frame_buf));
    if (ping_len == 0) {
      continue;
    }
    phy.write(frame_buf, ping_len);
    const uint32_t ping_sent_us = micros();

    clearRetransmitRequest();
    clearTxReportWait();
    setTxOwner(node_id);
    setSdOwner(0);
    advanceStateSequence();

    const size_t state_len = buildBusStateFrame(frame_buf, sizeof(frame_buf));
    if (verbose_) {
      Serial.print(F("Granting TX to node "));
      Serial.println(node_id);
    }
    if (state_len > 0) {
      phy.write(frame_buf, state_len);
    }
    startWaitingForTxReport(millis());

    const uint32_t start_ms = millis();
    while ((millis() - start_ms) < options.tx_report_timeout_ms) {
      servicePhyRx(phy, millis());
      tick(millis());

      if (hasLastPong() && getLastPongSource() == node_id && ping_rtt_us == 0) {
        ping_rtt_us = getLastPongRxUs() - ping_sent_us;
        if (verbose_) {
          Serial.print(F("PONG received from node "));
          Serial.print(node_id);
          Serial.print(F(", RTT us = "));
          Serial.println(ping_rtt_us);
        }
      }

      if (!isWaitingForTxReport()) {
        fillPingResultFromNode(node_id, attempts, ping_rtt_us, options.data_flags, out_result);

        out_result->passed =
            out_result->pong_seen &&
            out_result->tx_report_seen &&
            (out_result->error_code == 0) &&
            (!options.request_enum || out_result->enum_seen);

        passed = out_result->passed;
        break;
      }

      if (isRetransmitRequested()) {
        const size_t retry_len = buildBusStateFrame(frame_buf, sizeof(frame_buf));
        if (retry_len > 0) {
          phy.write(frame_buf, retry_len);
        }
        clearRetransmitRequest();
      }

      delay(1);
    }

    if (passed) {
      break;
    }
  }

  if (!passed) {
    /* Populate final result from the most recent observed state even on failure. */
    fillPingResultFromNode(node_id, attempts, 0, options.data_flags, out_result);
    out_result->passed =
        out_result->pong_seen &&
        out_result->tx_report_seen &&
        (out_result->error_code == 0) &&
        (!options.request_enum || out_result->enum_seen);
  }

  verbose_ = old_verbose;
  return out_result->passed;
}

bool BusMaster::runPingTestForNodes(SAMD21_BusPhy& phy,
                                    const uint8_t* node_ids,
                                    size_t node_count,
                                    const bus_ping_test_options_t& options,
                                    bus_ping_test_summary_t* out_summary) {
  if (node_ids == nullptr || out_summary == nullptr) return false;
  memset(out_summary, 0, sizeof(*out_summary));
  out_summary->node_count = (node_count <= kMaxNodes) ? node_count : kMaxNodes;
  out_summary->all_passed = true;
  for (size_t i = 0; i < out_summary->node_count; ++i) {
    const bool ok = runPingTestForNode(phy, node_ids[i], options, &out_summary->results[i]);
    if (ok) out_summary->pass_count++; else { out_summary->fail_count++; out_summary->all_passed = false; }
  }
  if (options.verbose) printPingTestSummary(*out_summary);
  return out_summary->all_passed;
}

void BusMaster::printPingTestSummary(const bus_ping_test_summary_t& summary) const {
  if (!verbose_) return;
  Serial.println();
  Serial.println(F("================================================================================"));
  Serial.println(F("Final Harness Test Summary"));
  Serial.println(F("--------------------------------------------------------------------------------"));
  Serial.println(F("Node  Enum  Pong  TXRpt  Ping RTT (us)   Status Bits   Uptime (ms)   Error  Result"));
  Serial.println(F("--------------------------------------------------------------------------------"));
  for (size_t i = 0; i < summary.node_count; ++i) {
    const bus_ping_test_result_t& r = summary.results[i];
    Serial.print(r.node_id); Serial.print(F("     "));
    Serial.print(r.enum_seen ? F("Y") : F("N")); Serial.print(F("     "));
    Serial.print(r.pong_seen ? F("Y") : F("N")); Serial.print(F("     "));
    Serial.print(r.tx_report_seen ? F("Y") : F("N")); Serial.print(F("      "));
    Serial.print(r.ping_rtt_us); Serial.print(F("            "));
    Serial.print(F("0x")); Serial.print(r.status_bits, HEX); Serial.print(F("         "));
    Serial.print(r.uptime_ms); Serial.print(F("       "));
    Serial.print(r.error_code); Serial.print(F("      "));
    Serial.println(r.passed ? F("PASS") : F("FAIL"));
  }
  Serial.println(F("================================================================================"));
}

bool BusMaster::shouldRequestRetransmit(const bus_frame_t& /*frame*/) const {
  return false;
}
