#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Bus_Protocol.h"

/*
 * ============================================================================
 * File: Bus_Node.h
 * Project: Grad Cap Display Bus Protocol
 * Target MCU: ESP32 display nodes
 * ============================================================================
 */

class BusNode {
 public:
  BusNode();

  void begin(uint8_t node_id);
  void setNodeId(uint8_t node_id);
  uint8_t getNodeId() const;

  bool processIncomingByte(uint8_t byte, uint32_t now_ms);
  void resetParser();
  bool handleFrame(const bus_frame_t& frame);

  void applyBusState(const bus_bus_state_t& state);
  bool hasTxOwnership() const;
  bool hasSdOwnership() const;
  const bus_bus_state_t& getBusState() const;
  bool retransmitRequested() const;

  bool queueMessage(uint8_t dst, uint8_t type, const void* payload, uint8_t len);
  bool hasQueuedMessages() const;
  size_t getQueuedMessageCount() const;
  void clearQueuedMessages();

  bool buildTransmitBundle();
  bool hasRetainedBundle() const;
  bool retransmitLastBundle();
  void discardRetainedBundle();
  size_t getNextEncodedTransmitFrame(uint8_t* out, size_t out_size);
  bool transmitBundleHasMoreFrames() const;

  uint16_t buildStatusBits() const;
  bus_tx_report_t buildTxReport() const;
  void setReportedSdOwner(uint8_t owner);
  uint8_t getReportedSdOwner() const;

  void setLcdStatus(bus_lcd_status_t status);
  bus_lcd_status_t getLcdStatus() const;
  void setAssetStatus(bus_asset_status_t status);
  bus_asset_status_t getAssetStatus() const;
  void setErrorCode(uint16_t error_code);
  uint16_t getErrorCode() const;
  void setPsramUsage(uint32_t used_bytes, uint32_t total_bytes);
  void setFlashUsage(uint32_t used_bytes, uint32_t total_bytes);
  void setSdActive(bool active);
  void setDisplayReady(bool ready);
  void setWorkPending(bool pending);
  void setMemoryWarning(bool warning);
  void setMorePending(bool pending);
  void setDebugPending(bool pending);
  bool hasDebugPending() const;

  bool queueEnumResponse(uint8_t request_seq = 0);
  bool queuePong(uint8_t request_seq);
  bool queueCommandAck(uint8_t acked_type, uint8_t result = BUS_RESULT_OK);
  bool queueCommandNack(uint8_t nacked_type, uint8_t result);
  bool queueErrorReport(uint16_t error_code, uint8_t context);
  bool queueAppData(uint8_t dst, uint8_t subtype, const void* data, uint8_t len);

  bool hasPendingAssetRequest() const;
  const char* getPendingAssetFilename() const;
  uint8_t getPendingAssetStorageTarget() const;
  uint8_t getPendingAssetFlags() const;
  void clearPendingAssetRequest();

 private:
  bool handlePing(const bus_frame_t& frame);
  bool handleEnumRequest(const bus_frame_t& frame);
  bool handleLoadAsset(const bus_frame_t& frame);
  bool onLoadAsset(const bus_load_asset_t& payload);

  bool enqueueFrame(const bus_frame_t& frame);
  bool appendFrameToBundle(const bus_frame_t& frame);
  void resetBundleReadCursor();
  bool appendTxReportToBundle();
  void updateBundleRetentionStateAfterBusState();

  static constexpr size_t kMaxQueuedFrames = 15;
  static constexpr size_t kMaxBundleFrames = 16;

  uint8_t node_id_;
  uint8_t next_seq_;
  bus_parser_t parser_;
  bus_bus_state_t bus_state_;
  uint8_t reported_sd_owner_;

  bus_lcd_status_t lcd_status_;
  bus_asset_status_t asset_status_;
  uint16_t error_code_;
  uint32_t psram_used_bytes_;
  uint32_t psram_total_bytes_;
  uint32_t flash_used_bytes_;
  uint32_t flash_total_bytes_;
  bool sd_active_;
  bool display_ready_;
  bool work_pending_;
  bool memory_warning_;
  bool more_pending_;
  bool debug_pending_;

  bus_frame_t queued_frames_[kMaxQueuedFrames];
  size_t queued_frame_count_;

  bus_frame_t retained_bundle_[kMaxBundleFrames];
  size_t retained_bundle_count_;
  size_t retained_bundle_read_index_;
  bool retained_bundle_valid_;

  bool pending_asset_request_;
  uint8_t pending_asset_storage_target_;
  uint8_t pending_asset_flags_;
  char pending_asset_filename_[BUS_MAX_PAYLOAD_SIZE - 2];
};