#include "Bus_Node.h"
#include <Arduino.h>

#include <string.h>

namespace {
void copy_bounded_string(char* dst, size_t dst_size, const char* src, size_t src_len) {
  if (dst == nullptr || dst_size == 0) return;
  const size_t copy_len = (src != nullptr && src_len < (dst_size - 1)) ? src_len : (dst_size - 1);
  if (src != nullptr && copy_len > 0) memcpy(dst, src, copy_len);
  dst[copy_len] = '\0';
}
}

BusNode::BusNode()
    : node_id_(0),
      next_seq_(0),
      reported_sd_owner_(0),
      lcd_status_(BUS_LCD_STATUS_UNINIT),
      asset_status_(BUS_ASSET_STATUS_NONE),
      error_code_(0),
      psram_used_bytes_(0),
      psram_total_bytes_(0),
      flash_used_bytes_(0),
      flash_total_bytes_(0),
      sd_active_(false),
      display_ready_(false),
      work_pending_(false),
      memory_warning_(false),
      more_pending_(false),
      debug_pending_(false),
      queued_frame_count_(0),
      retained_bundle_count_(0),
      retained_bundle_read_index_(0),
      retained_bundle_valid_(false),
      pending_asset_request_(false),
      pending_asset_storage_target_(0),
      pending_asset_flags_(0) {
  memset(&parser_, 0, sizeof(parser_));
  bus_parser_reset(&parser_);
  memset(&bus_state_, 0, sizeof(bus_state_));
  memset(queued_frames_, 0, sizeof(queued_frames_));
  memset(retained_bundle_, 0, sizeof(retained_bundle_));
  memset(pending_asset_filename_, 0, sizeof(pending_asset_filename_));
}

void BusNode::begin(uint8_t node_id) {
  node_id_ = node_id;
  next_seq_ = 0;
  reported_sd_owner_ = 0;
  lcd_status_ = BUS_LCD_STATUS_UNINIT;
  asset_status_ = BUS_ASSET_STATUS_NONE;
  error_code_ = 0;
  psram_used_bytes_ = 0;
  psram_total_bytes_ = 0;
  flash_used_bytes_ = 0;
  flash_total_bytes_ = 0;
  sd_active_ = false;
  display_ready_ = false;
  work_pending_ = false;
  memory_warning_ = false;
  more_pending_ = false;
  debug_pending_ = false;

  memset(&bus_state_, 0, sizeof(bus_state_));
  bus_parser_reset(&parser_);
  queued_frame_count_ = 0;
  retained_bundle_count_ = 0;
  retained_bundle_read_index_ = 0;
  retained_bundle_valid_ = false;
  pending_asset_request_ = false;
  pending_asset_storage_target_ = 0;
  pending_asset_flags_ = 0;
  memset(pending_asset_filename_, 0, sizeof(pending_asset_filename_));
}

void BusNode::setNodeId(uint8_t node_id) { node_id_ = node_id; }
uint8_t BusNode::getNodeId() const { return node_id_; }

bool BusNode::processIncomingByte(uint8_t byte, uint32_t now_ms) {
  if (!bus_parser_feed(&parser_, byte, now_ms)) return false;
  const bus_frame_t frame = parser_.frame;
  parser_.frame_ready = 0;
  return handleFrame(frame);
}

void BusNode::resetParser() { bus_parser_reset(&parser_); }

bool BusNode::handleFrame(const bus_frame_t& frame) {
  if (frame.dst != node_id_ && frame.dst != BUS_BROADCAST_ADDR) return false;

  switch (frame.type) {
    case BUS_MSG_BUS_STATE: {
      if (frame.len != sizeof(bus_bus_state_t)) return false;
      bus_bus_state_t state{};
      memcpy(&state, frame.payload, sizeof(state));
      applyBusState(state);
      return true;
    }
    case BUS_MSG_PING:
      return handlePing(frame);
    case BUS_MSG_ENUM_REQUEST:
      return handleEnumRequest(frame);
    case BUS_MSG_LOAD_ASSET:
      return handleLoadAsset(frame);
    default:
      return false;
  }
}

void BusNode::applyBusState(const bus_bus_state_t& state) {
  const uint16_t previous_state_seq = bus_state_.state_seq;
  const uint8_t previous_tx_owner = bus_state_.tx_owner;
  bus_state_ = state;
  reported_sd_owner_ = bus_state_.sd_owner;

  if (retained_bundle_valid_) {
    const bool newer_state = (state.state_seq != previous_state_seq);
    const bool moved_to_other_owner = (state.tx_owner != node_id_);
    const bool retry_requested = (state.flags & BUS_STATE_FLAG_RETRANSMIT) != 0;
    if ((newer_state && moved_to_other_owner && !retry_requested) ||
        (previous_tx_owner == node_id_ && state.tx_owner != node_id_ && !retry_requested)) {
      discardRetainedBundle();
    }
  }
}

bool BusNode::hasTxOwnership() const { return bus_state_.tx_owner == node_id_; }
bool BusNode::hasSdOwnership() const { return bus_state_.sd_owner == node_id_; }
const bus_bus_state_t& BusNode::getBusState() const { return bus_state_; }
bool BusNode::retransmitRequested() const { return (bus_state_.flags & BUS_STATE_FLAG_RETRANSMIT) != 0; }

bool BusNode::queueMessage(uint8_t dst, uint8_t type, const void* payload, uint8_t len) {
  if (len > BUS_MAX_PAYLOAD_SIZE) return false;
  bus_frame_t frame{};
  frame.dst = dst;
  frame.src = node_id_;
  frame.type = type;
  frame.seq = next_seq_++;
  frame.len = len;
  if (payload != nullptr && len > 0) memcpy(frame.payload, payload, len);
  return enqueueFrame(frame);
}

bool BusNode::hasQueuedMessages() const { return queued_frame_count_ > 0; }
size_t BusNode::getQueuedMessageCount() const { return queued_frame_count_; }
void BusNode::clearQueuedMessages() {
  queued_frame_count_ = 0;
  memset(queued_frames_, 0, sizeof(queued_frames_));
  debug_pending_ = false;
}

bool BusNode::buildTransmitBundle() {
  retained_bundle_count_ = 0;
  retained_bundle_read_index_ = 0;
  retained_bundle_valid_ = false;
  for (size_t i = 0; i < queued_frame_count_; ++i) {
    if (!appendFrameToBundle(queued_frames_[i])) return false;
  }
  if (!appendTxReportToBundle()) return false;
  clearQueuedMessages();
  retained_bundle_valid_ = true;
  more_pending_ = false;
  return true;
}

bool BusNode::hasRetainedBundle() const { return retained_bundle_valid_ && retained_bundle_count_ > 0; }
bool BusNode::retransmitLastBundle() {
  if (!retained_bundle_valid_) return false;
  resetBundleReadCursor();
  return true;
}
void BusNode::discardRetainedBundle() {
  retained_bundle_count_ = 0;
  retained_bundle_read_index_ = 0;
  retained_bundle_valid_ = false;
  memset(retained_bundle_, 0, sizeof(retained_bundle_));
}

size_t BusNode::getNextEncodedTransmitFrame(uint8_t* out, size_t out_size) {
  if (out == nullptr || out_size == 0) return 0;
  if (!retained_bundle_valid_ || retained_bundle_read_index_ >= retained_bundle_count_) return 0;
  return bus_encode_frame(out, out_size, &retained_bundle_[retained_bundle_read_index_++]);
}

bool BusNode::transmitBundleHasMoreFrames() const {
  return retained_bundle_valid_ && retained_bundle_read_index_ < retained_bundle_count_;
}

uint16_t BusNode::buildStatusBits() const {
  uint16_t bits = 0;
  bits |= BUS_STATUS_SET_LCD(lcd_status_);
  bits |= BUS_STATUS_SET_ASSET(asset_status_);
  if (more_pending_ || queued_frame_count_ > 0) bits |= BUS_STATUS_MORE_PENDING;
  if (debug_pending_) bits |= BUS_STATUS_DEBUG_PENDING;
  if (sd_active_) bits |= BUS_STATUS_SD_ACTIVE;
  if (memory_warning_) bits |= BUS_STATUS_MEM_WARNING;
  if (error_code_ != 0) bits |= BUS_STATUS_ERROR_PRESENT;
  if (display_ready_) bits |= BUS_STATUS_DISPLAY_READY;
  if (work_pending_) bits |= BUS_STATUS_WORK_PENDING;
  return bits;
}

bus_tx_report_t BusNode::buildTxReport() const {
  bus_tx_report_t report{};
  report.reported_sd_owner = reported_sd_owner_;
  report.status_bits = buildStatusBits();
  report.error_code = error_code_;
  report.uptime_ms = millis();
  report.psram_used_bytes = psram_used_bytes_;
  report.psram_total_bytes = psram_total_bytes_;
  report.flash_used_bytes = flash_used_bytes_;
  report.flash_total_bytes = flash_total_bytes_;
  return report;
}

void BusNode::setReportedSdOwner(uint8_t owner) { reported_sd_owner_ = owner; }
uint8_t BusNode::getReportedSdOwner() const { return reported_sd_owner_; }
void BusNode::setLcdStatus(bus_lcd_status_t status) { lcd_status_ = status; }
bus_lcd_status_t BusNode::getLcdStatus() const { return lcd_status_; }
void BusNode::setAssetStatus(bus_asset_status_t status) { asset_status_ = status; }
bus_asset_status_t BusNode::getAssetStatus() const { return asset_status_; }
void BusNode::setErrorCode(uint16_t error_code) { error_code_ = error_code; }
uint16_t BusNode::getErrorCode() const { return error_code_; }
void BusNode::setPsramUsage(uint32_t used_bytes, uint32_t total_bytes) { psram_used_bytes_ = used_bytes; psram_total_bytes_ = total_bytes; }
void BusNode::setFlashUsage(uint32_t used_bytes, uint32_t total_bytes) { flash_used_bytes_ = used_bytes; flash_total_bytes_ = total_bytes; }
void BusNode::setSdActive(bool active) { sd_active_ = active; }
void BusNode::setDisplayReady(bool ready) { display_ready_ = ready; }
void BusNode::setWorkPending(bool pending) { work_pending_ = pending; }
void BusNode::setMemoryWarning(bool warning) { memory_warning_ = warning; }
void BusNode::setMorePending(bool pending) { more_pending_ = pending; }
void BusNode::setDebugPending(bool pending) { debug_pending_ = pending; }
bool BusNode::hasDebugPending() const { return debug_pending_; }

bool BusNode::queueEnumResponse(uint8_t) {
  bus_enum_response_t response{};
  response.node_id = node_id_;
  response.protocol_major = BUS_PROTOCOL_VERSION_MAJOR;
  response.protocol_minor = BUS_PROTOCOL_VERSION_MINOR;
  response.fw_major = 0;
  response.fw_minor = 1;
  response.capabilities = BUS_CAP_DEBUG_TEXT | BUS_CAP_LOAD_ASSET | BUS_CAP_PSRAM | BUS_CAP_EXT_FLASH;
  return queueMessage(BUS_MASTER_ADDR, BUS_MSG_ENUM_RESPONSE, &response, sizeof(response));
}

bool BusNode::queuePong(uint8_t request_seq) {
  bus_frame_t frame{};
  frame.dst = BUS_MASTER_ADDR;
  frame.src = node_id_;
  frame.type = BUS_MSG_PONG;
  frame.seq = request_seq;
  frame.len = 0;
  return enqueueFrame(frame);
}

bool BusNode::queueCommandAck(uint8_t acked_type, uint8_t result) {
  bus_command_ack_t payload{};
  payload.acked_type = acked_type;
  payload.result = result;
  return queueMessage(BUS_MASTER_ADDR, BUS_MSG_COMMAND_ACK, &payload, sizeof(payload));
}

bool BusNode::queueCommandNack(uint8_t nacked_type, uint8_t result) {
  bus_command_nack_t payload{};
  payload.nacked_type = nacked_type;
  payload.result = result;
  return queueMessage(BUS_MASTER_ADDR, BUS_MSG_COMMAND_NACK, &payload, sizeof(payload));
}

bool BusNode::queueErrorReport(uint16_t error_code, uint8_t context) {
  bus_error_report_t payload{};
  payload.error_code = error_code;
  payload.context = context;
  return queueMessage(BUS_MASTER_ADDR, BUS_MSG_ERROR_REPORT, &payload, sizeof(payload));
}

bool BusNode::queueAppData(uint8_t dst, uint8_t subtype, const void* data, uint8_t len) {
  if (len > (BUS_MAX_PAYLOAD_SIZE - 1)) return false;
  bus_app_data_t payload{};
  payload.subtype = subtype;
  if (data != nullptr && len > 0) memcpy(payload.data, data, len);
  return queueMessage(dst, BUS_MSG_APP_DATA, &payload, static_cast<uint8_t>(1 + len));
}

bool BusNode::hasPendingAssetRequest() const { return pending_asset_request_; }
const char* BusNode::getPendingAssetFilename() const { return pending_asset_filename_; }
uint8_t BusNode::getPendingAssetStorageTarget() const { return pending_asset_storage_target_; }
uint8_t BusNode::getPendingAssetFlags() const { return pending_asset_flags_; }
void BusNode::clearPendingAssetRequest() {
  pending_asset_request_ = false;
  pending_asset_storage_target_ = 0;
  pending_asset_flags_ = 0;
  memset(pending_asset_filename_, 0, sizeof(pending_asset_filename_));
}

bool BusNode::handlePing(const bus_frame_t& frame) { return queuePong(frame.seq); }
bool BusNode::handleEnumRequest(const bus_frame_t&) { return queueEnumResponse(); }

bool BusNode::handleLoadAsset(const bus_frame_t& frame) {
  if (frame.len < 3) return queueCommandNack(BUS_MSG_LOAD_ASSET, BUS_RESULT_INVALID);
  bus_load_asset_t payload{};
  memcpy(&payload, frame.payload, frame.len);
  const uint8_t filename_len = payload.filename_len;
  if (filename_len > (frame.len - 3)) return queueCommandNack(BUS_MSG_LOAD_ASSET, BUS_RESULT_INVALID);
  if (!onLoadAsset(payload)) return queueCommandNack(BUS_MSG_LOAD_ASSET, BUS_RESULT_BAD_STATE);
  return queueCommandAck(BUS_MSG_LOAD_ASSET, BUS_RESULT_OK);
}

bool BusNode::onLoadAsset(const bus_load_asset_t& payload) {
  pending_asset_request_ = true;
  pending_asset_storage_target_ = payload.storage_target;
  pending_asset_flags_ = payload.flags;
  copy_bounded_string(pending_asset_filename_, sizeof(pending_asset_filename_), payload.filename, payload.filename_len);
  asset_status_ = BUS_ASSET_STATUS_ASSIGNED;
  work_pending_ = true;
  return true;
}

bool BusNode::enqueueFrame(const bus_frame_t& frame) {
  if (queued_frame_count_ >= kMaxQueuedFrames) return false;
  queued_frames_[queued_frame_count_++] = frame;
  return true;
}

bool BusNode::appendFrameToBundle(const bus_frame_t& frame) {
  if (retained_bundle_count_ >= kMaxBundleFrames) return false;
  retained_bundle_[retained_bundle_count_++] = frame;
  return true;
}

void BusNode::resetBundleReadCursor() { retained_bundle_read_index_ = 0; }

bool BusNode::appendTxReportToBundle() {
  const bus_tx_report_t report = buildTxReport();
  bus_frame_t frame{};
  frame.dst = BUS_MASTER_ADDR;
  frame.src = node_id_;
  frame.type = BUS_MSG_TX_REPORT;
  frame.seq = next_seq_++;
  frame.len = sizeof(report);
  memcpy(frame.payload, &report, sizeof(report));
  return appendFrameToBundle(frame);
}

void BusNode::updateBundleRetentionStateAfterBusState() {}
