#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Bus_Protocol.h"

class SAMD21_BusPhy;

/*
 * ============================================================================
 * File: Bus_Master.h
 * Project: Grad Cap Display Bus Protocol
 * Target MCU: SAMD21 master
 *
 * Description
 * -----------
 * This header declares the BusMaster class, which implements the master-side
 * protocol engine for the SAMD21 that orchestrates the shared RS-485 bus.
 *
 * BusMaster is responsible for:
 * - Maintaining the authoritative BUS_STATE that all nodes follow
 * - Choosing which node currently owns the RS-485 talking stick
 * - Choosing which node currently owns the shared micro SD SPI resource
 * - Broadcasting BUS_STATE frames so all nodes observe the same state
 * - Parsing incoming protocol frames from ESP32 nodes
 * - Tracking known nodes in a local node table
 * - Processing TX_REPORT frames and updating node-health records
 * - Detecting when a node must retransmit its most recent turn bundle
 * - Sending commands such as LOAD_ASSET and PING
 * - Printing node debug text to the SAMD21 serial monitor or host console
 *
 * BusMaster is NOT responsible for:
 * - The low-level UART / RS-485 driver itself
 * - The node-side message queue implementation
 * - The user-facing print/println debug API used by ESP32 code
 *
 * Typical usage pattern
 * ---------------------
 * 1. The application creates one BusMaster instance on the SAMD21.
 * 2. The application feeds received UART bytes into processIncomingByte().
 * 3. The application periodically calls tick() so the master can advance the
 *    bus state machine, issue BUS_STATE frames, and handle timeouts/retries.
 * 4. The application uses command helpers to queue work for nodes.
 * 5. The application may inspect cached node status to decide what to do next.
 * ============================================================================
 */

/*
 * Asset format classification used by the master-side asset catalog.
 *
 * This is intentionally simple for the first pass. File type may be inferred by
 * the future SD-management layer from file extension or file inspection.
 */
typedef enum {
  BUS_ASSET_FORMAT_UNKNOWN = 0,
  BUS_ASSET_FORMAT_JPEG,
  BUS_ASSET_FORMAT_MJPEG,
  BUS_ASSET_FORMAT_BIN
} bus_asset_format_t;

/*
 * Master-side asset catalog entry.
 *
 * This structure is NOT a wire-format message. It is the SAMD21 master's local
 * bookkeeping record describing one asset discovered on the SD card and how it
 * relates to a node assignment.
 *
 * Fields:
 * - valid:             Entry is populated and usable.
 * - filename:          Asset filename as discovered on the SD card.
 * - size_bytes:        File size in bytes.
 * - format:            Simple asset format classification.
 * - assigned_node:     Node currently assigned this asset, or 0 if unassigned.
 * - assigned:          True if this asset has been assigned to a node.
 * - load_requested:    True once the master has sent a LOAD_ASSET command for
 *                      this asset to the assigned node.
 * - load_confirmed:    True once the assigned node has reported a loaded/ready
 *                      state that confirms the asset is available for use.
 */
typedef struct {
  bool valid;
  char filename[BUS_MAX_PAYLOAD_SIZE - 2];
  uint32_t size_bytes;
  uint8_t format;
  uint8_t assigned_node;
  bool assigned;
  bool load_requested;
  bool load_confirmed;
} bus_asset_info_t;


typedef enum {
  BUS_PING_DATA_BASIC  = 0x01,
  BUS_PING_DATA_MEMORY = 0x02,
  BUS_PING_DATA_ALL    = (BUS_PING_DATA_BASIC | BUS_PING_DATA_MEMORY)
} bus_ping_test_data_flags_t;

typedef struct {
  bool verbose;
  bool request_enum;
  uint8_t max_attempts_per_node;
  uint8_t data_flags;
  uint32_t pong_timeout_ms;
  uint32_t tx_report_timeout_ms;
} bus_ping_test_options_t;

typedef struct {
  uint8_t node_id;
  bool enum_seen;
  bool pong_seen;
  bool tx_report_seen;
  bool passed;
  uint8_t attempts_used;
  uint32_t ping_rtt_us;
  uint16_t status_bits;
  uint16_t error_code;
  uint32_t uptime_ms;
  uint32_t psram_used_bytes;
  uint32_t psram_total_bytes;
  uint32_t flash_used_bytes;
  uint32_t flash_total_bytes;
} bus_ping_test_result_t;

typedef struct {
  bool all_passed;
  size_t node_count;
  size_t pass_count;
  size_t fail_count;
  bus_ping_test_result_t results[9];
} bus_ping_test_summary_t;

class BusMaster {
 public:
  // ---------------------------------------------------------------------------
  // Construction / Initialization
  // ---------------------------------------------------------------------------

  /*
   * Construct an uninitialized BusMaster instance.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - new BusMaster object with default/empty state
   */
  BusMaster();

  /*
   * Initialize the master-side protocol engine.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - Initializes authoritative BUS_STATE, clears node table, and prepares
   *   parser / transmit state for use.
   */
  void begin();

  void setVerbose(bool enabled);
  bool isVerbose() const;

  // ---------------------------------------------------------------------------
  // Main Receive / Parse Path
  // ---------------------------------------------------------------------------

  /*
   * Feed one received UART byte into the protocol parser.
   *
   * Inputs:
   * - byte: received byte from the UART / RS-485 interface
   * - now_ms: current system time in milliseconds for parser timeout handling
   *
   * Outputs:
   * - bool: true if a complete valid frame was received and handled
   */
  bool processIncomingByte(uint8_t byte, uint32_t now_ms);

  /* Reset the internal parser state machine to idle. */
  void resetParser();

  /*
   * Handle one fully parsed and validated frame received from a node.
   *
   * Inputs:
   * - frame: decoded protocol frame
   *
   * Outputs:
   * - bool: true if frame was accepted/handled
   */
  bool handleFrame(const bus_frame_t& frame);

  /*
   * Periodic service function for master-side timing and scheduling.
   *
   * Inputs:
   * - now_ms: current system time in milliseconds
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - Intended to be called regularly from the SAMD21 main loop.
   * - May advance the active node, request retransmits, broadcast BUS_STATE,
   *   or perform timeout/retry bookkeeping.
   */
  void tick(uint32_t now_ms);

  // ---------------------------------------------------------------------------
  // Authoritative BUS_STATE Control
  // ---------------------------------------------------------------------------

  /*
   * Return the master's current authoritative BUS_STATE payload.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - const reference to authoritative BUS_STATE structure
   */
  const bus_bus_state_t& getBusState() const;

  /*
   * Set the node that currently owns the RS-485 talking stick.
   *
   * Inputs:
   * - node_id: logical node address, or 0 if no node currently selected
   *
   * Outputs:
   * - none
   */
  void setTxOwner(uint8_t node_id);

  /* Return the node that currently owns the RS-485 talking stick. */
  uint8_t getTxOwner() const;

  /*
   * Set the node that currently owns the shared micro SD SPI resource.
   *
   * Inputs:
   * - node_id: logical node address, or 0 if SD is free/unowned
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - This updates the authoritative BUS_STATE.sd_owner field.
   */
  void setSdOwner(uint8_t node_id);

  /* Return the node that currently owns the shared micro SD resource. */
  uint8_t getSdOwner() const;

  /* Set or clear the BUS_STATE retransmit request flag. */
  void setRetransmitRequested(bool requested);

  /* Read back whether retransmit is currently requested for tx_owner. */
  bool isRetransmitRequested() const;

  /* Set or clear the startup/preload phase flag in BUS_STATE. */
  void setStartupPhase(bool startup);

  /* Read back whether the master is currently in startup/preload phase. */
  bool isStartupPhase() const;

  /* Set or clear the master-latched error flag in BUS_STATE. */
  void setErrorLatched(bool latched);

  /* Read back the master-latched error flag state. */
  bool isErrorLatched() const;

  /*
   * Increment the authoritative BUS_STATE state sequence number.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - uint16_t: new BUS_STATE sequence value after increment
   */
  uint16_t advanceStateSequence();

  /*
   * Encode and emit the current authoritative BUS_STATE frame.
   *
   * Inputs:
   * - out: destination byte buffer for the encoded on-wire frame
   * - out_size: size of out buffer in bytes
   *
   * Outputs:
   * - size_t: encoded BUS_STATE frame length, or 0 on failure
   */
  size_t buildBusStateFrame(uint8_t* out, size_t out_size);

  // ---------------------------------------------------------------------------
  // Node Table Access / Bookkeeping
  // ---------------------------------------------------------------------------

  /*
   * Return the number of node slots currently marked active in the table.
   */
  size_t getActiveNodeCount() const;

  /*
   * Find a mutable node-table entry by logical node address.
   *
   * Inputs:
   * - node_id: logical node address to search for
   *
   * Outputs:
   * - pointer to matching node table entry, or nullptr if not found
   */
  bus_node_info_t* findNode(uint8_t node_id);

  /*
   * Find a read-only node-table entry by logical node address.
   */
  const bus_node_info_t* findNode(uint8_t node_id) const;

  /*
   * Mark a node as recently seen by the master.
   *
   * Inputs:
   * - node_id: logical node address
   * - now_ms: current master timebase in milliseconds
   *
   * Outputs:
   * - none
   */
  void markNodeSeen(uint8_t node_id, uint32_t now_ms);

  /*
   * Update / insert node-table data from a received ENUM_RESPONSE payload.
   *
   * Inputs:
   * - info: decoded enumeration payload from a node
   * - now_ms: current master timebase in milliseconds
   *
   * Outputs:
   * - bool: true if table updated successfully
   */
  bool updateNodeFromEnumResponse(const bus_enum_response_t& info, uint32_t now_ms);

  /*
   * Update cached node status fields from a received TX_REPORT payload.
   *
   * Inputs:
   * - src_node: node that sent the TX_REPORT
   * - report: decoded TX_REPORT payload
   * - now_ms: current master timebase in milliseconds
   *
   * Outputs:
   * - bool: true if table updated successfully
   */
  bool updateNodeFromTxReport(uint8_t src_node,
                              const bus_tx_report_t& report,
                              uint32_t now_ms);

  /* Clear the entire node table back to an empty state. */
  void clearNodeTable();

  // ---------------------------------------------------------------------------
  // Asset Catalog / Node Assignment Bookkeeping
  // ---------------------------------------------------------------------------

  /* Clear the entire cached asset catalog. */
  void clearAssetCatalog();

  /* Return the number of valid asset entries currently stored. */
  size_t getAssetCount() const;

  /*
   * Add one discovered asset entry to the master's catalog.
   *
   * Inputs:
   * - filename: null-terminated asset filename discovered on the SD card
   * - size_bytes: file size in bytes
   * - format: simple asset format classification
   *
   * Outputs:
   * - bool: true if the asset was added successfully
   *
   * Notes:
   * - Intended to be called later by the SD-management layer after scanning the
   *   SD card during master startup.
   */
  bool addAsset(const char* filename, uint32_t size_bytes, uint8_t format);

  /*
   * Find a mutable asset entry by filename.
   *
   * Inputs:
   * - filename: null-terminated filename to search for
   *
   * Outputs:
   * - pointer to matching asset entry, or nullptr if not found
   */
  bus_asset_info_t* findAssetByFilename(const char* filename);

  /* Read-only version of findAssetByFilename(). */
  const bus_asset_info_t* findAssetByFilename(const char* filename) const;

  /*
   * Find the asset currently assigned to the supplied node.
   *
   * Inputs:
   * - node_id: logical node address
   *
   * Outputs:
   * - pointer to matching asset entry, or nullptr if no asset is assigned
   */
  bus_asset_info_t* findAssignedAssetForNode(uint8_t node_id);

  /* Read-only version of findAssignedAssetForNode(). */
  const bus_asset_info_t* findAssignedAssetForNode(uint8_t node_id) const;

  /*
   * Assign an asset entry to a node.
   *
   * Inputs:
   * - filename: filename of the asset to assign
   * - node_id: logical node address that should load this asset
   *
   * Outputs:
   * - bool: true if assignment succeeded
   */
  bool assignAssetToNode(const char* filename, uint8_t node_id);

  /* Clear any existing asset assignment for the supplied node. */
  void clearAssetAssignmentForNode(uint8_t node_id);

  /* Mark that LOAD_ASSET has been sent for the asset assigned to this node. */
  void markAssetLoadRequested(uint8_t node_id);

  /* Mark that the asset assigned to this node has been confirmed loaded. */
  void markAssetLoadConfirmed(uint8_t node_id);

  // ---------------------------------------------------------------------------
  // Turn Scheduling / Retry Control
  // ---------------------------------------------------------------------------

  /*
   * Choose the next node that should receive the RS-485 talking stick.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - uint8_t: selected next node ID, or 0 if no node should be selected
   *
   * Notes:
   * - First-pass implementation may use a simple round-robin policy.
   */
  uint8_t chooseNextTxOwner() const;

  /*
   * Advance the authoritative tx_owner to the next selected node.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - uint8_t: new tx_owner value after advancement
   */
  uint8_t advanceToNextTxOwner();

  /*
   * Request retransmission from the current tx_owner.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - This sets BUS_STATE_FLAG_RETRANSMIT so the current tx_owner knows to
   *   resend its retained last-turn bundle.
   */
  void requestRetransmitForCurrentNode();

  /* Clear the retransmit request for the current tx_owner. */
  void clearRetransmitRequest();

  /*
   * Record that the master is now waiting for a TX_REPORT from the current
   * tx_owner.
   *
   * Inputs:
   * - now_ms: current master timebase in milliseconds
   *
   * Outputs:
   * - none
   */
  void startWaitingForTxReport(uint32_t now_ms);

  /* Return true if the master is currently waiting for a TX_REPORT. */
  bool isWaitingForTxReport() const;

  /*
   * Check whether the current TX_REPORT wait has timed out.
   *
   * Inputs:
   * - now_ms: current master timebase in milliseconds
   *
   * Outputs:
   * - bool: true if waiting and timeout expired
   */
  bool txReportTimedOut(uint32_t now_ms) const;

  /* Clear the current TX_REPORT wait state. */
  void clearTxReportWait();

  // ---------------------------------------------------------------------------
  // Outbound Command / Request Helpers
  // ---------------------------------------------------------------------------

  /* Build and encode a PING command for one node. */
  size_t buildPingFrame(uint8_t node_id, uint8_t* out, size_t out_size);

  uint8_t getLastPongSource() const;
  uint32_t getLastPongRxUs() const;
  bool hasLastPong() const;
  void clearLastPong();

  bool runPingTestForNode(SAMD21_BusPhy& phy,
                          uint8_t node_id,
                          const bus_ping_test_options_t& options,
                          bus_ping_test_result_t* out_result);

  bool runPingTestForNodes(SAMD21_BusPhy& phy,
                           const uint8_t* node_ids,
                           size_t node_count,
                           const bus_ping_test_options_t& options,
                           bus_ping_test_summary_t* out_summary);

  void printPingTestSummary(const bus_ping_test_summary_t& summary) const;

  /* Build and encode an ENUM_REQUEST for one node or broadcast. */
  size_t buildEnumRequestFrame(uint8_t node_id, uint8_t* out, size_t out_size);

  /*
   * Build and encode a LOAD_ASSET command for one node.
   *
   * Inputs:
   * - node_id: destination node
   * - filename: null-terminated asset filename
   * - storage_target: PSRAM / flash / auto hint
   * - flags: command option bits
   * - out: destination byte buffer for encoded frame
   * - out_size: size of out buffer in bytes
   *
   * Outputs:
   * - size_t: encoded frame length, or 0 on failure
   */
  size_t buildLoadAssetFrame(uint8_t node_id,
                             const char* filename,
                             uint8_t storage_target,
                             uint8_t flags,
                             uint8_t* out,
                             size_t out_size);

  // ---------------------------------------------------------------------------
  // Received Message Handlers
  // ---------------------------------------------------------------------------

  /* Handle a received PONG frame from a node. */
  bool handlePong(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received ENUM_RESPONSE frame from a node. */
  bool handleEnumResponse(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received TX_REPORT frame from a node. */
  bool handleTxReport(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received DEBUG_TEXT frame from a node. */
  bool handleDebugText(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received COMMAND_ACK frame from a node. */
  bool handleCommandAck(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received COMMAND_NACK frame from a node. */
  bool handleCommandNack(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received ERROR_REPORT frame from a node. */
  bool handleErrorReport(const bus_frame_t& frame, uint32_t now_ms);

  /* Handle a received APP_DATA frame. */
  bool handleAppData(const bus_frame_t& frame, uint32_t now_ms);

  // ---------------------------------------------------------------------------
  // Debug / Host Presentation Helpers
  // ---------------------------------------------------------------------------

  /*
   * Format one received node debug line for output on the SAMD21 side.
   *
   * Inputs:
   * - src_node: node that sent the debug line
   * - msg: decoded BUS_MSG_DEBUG_TEXT payload
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - First-pass behavior may simply print lines like:
   *     [3] LCD init complete
   */
  void presentDebugText(uint8_t src_node, const bus_debug_text_t& msg);

 private:
  // ---------------------------------------------------------------------------
  // Internal Helpers
  // ---------------------------------------------------------------------------

  /* Append/update one node entry in the table, creating it if possible. */
  bus_node_info_t* upsertNode(uint8_t node_id);

  /* Return true if the supplied node ID is valid for an ESP32 node. */
  bool isValidNodeId(uint8_t node_id) const;

  /* Update BUS_STATE_FLAG_SD_BUSY to match the current sd_owner field. */
  void syncSdBusyFlag();

  /*
   * Internal helper to accept one TX_REPORT from the current tx_owner and clear
   * retransmit/wait state if the report is valid.
   */
  void acceptCurrentTxReport(uint8_t src_node, uint32_t now_ms);

  /*
   * Internal helper to decide whether an incoming frame should trigger a retry
   * request instead of advancing to the next node.
   */
  bool shouldRequestRetransmit(const bus_frame_t& frame) const;
  int findNodeSlot(uint8_t node_id) const;
  void resetPingTrackingForNode(uint8_t node_id);
  void fillPingResultFromNode(uint8_t node_id, uint8_t attempts_used, uint32_t ping_rtt_us,
                              uint8_t data_flags, bus_ping_test_result_t* out_result) const;
  void servicePhyRx(SAMD21_BusPhy& phy, uint32_t now_ms);
  void maybeLogFrame(const bus_frame_t& frame) const;

  // ---------------------------------------------------------------------------
  // First-Pass Capacity / Storage Assumptions
  // ---------------------------------------------------------------------------

  /*
   * The first implementation of BusMaster will use a simple fixed-size node
   * table sized for the expected 3x3 display system.
   *
   * Planned first-pass capacities:
   * - Maximum managed ESP32 nodes: 9
   *
   * This matches the current hardware concept of a 3x3 grid of display nodes.
   */
  static constexpr size_t kMaxNodes = 9;
  static constexpr size_t kMaxAssets = 32;

  // ---------------------------------------------------------------------------
  // Internal State
  // ---------------------------------------------------------------------------

  /* Master-side parser for incoming node traffic. */
  bus_parser_t parser_;

  /* Authoritative shared bus/resource state published in BUS_STATE. */
  bus_bus_state_t bus_state_;

  /* Master-managed node table. */
  bus_node_info_t nodes_[kMaxNodes];
  size_t active_node_count_;

  /*
   * Master-managed asset catalog populated later by the SD-management layer.
   * This allows the master to keep track of what files exist, what type/size
   * they are, and which node each asset has been assigned to.
   */
  bus_asset_info_t assets_[kMaxAssets];
  size_t asset_count_;

  /*
   * Scheduler bookkeeping.
   * last_tx_owner_index_ may be used by a first-pass round-robin scheduler.
   */
  size_t last_tx_owner_index_;

  /*
   * TX_REPORT wait / retry tracking.
   * The master starts waiting after granting a node the talking stick.
   */
  bool waiting_for_tx_report_;
  uint32_t tx_report_wait_start_ms_;
  uint32_t tx_report_timeout_ms_;
  bool verbose_;

  /*
   * Optional cached info about the most recently heard node/debug event.
   * These fields are helpful during bring-up and diagnostics.
   */
  uint8_t last_frame_src_;
  uint8_t last_frame_type_;
  /*
  *  Helper values for calculating PING and PONG times
  */
  uint8_t last_pong_src_;
  uint32_t last_pong_rx_us_;
  bool last_pong_valid_;

  bool enum_seen_[kMaxNodes];
  bool pong_seen_[kMaxNodes];
  bool tx_report_seen_[kMaxNodes];
  uint32_t node_psram_used_[kMaxNodes];
  uint32_t node_psram_total_[kMaxNodes];
  uint32_t node_flash_used_[kMaxNodes];
  uint32_t node_flash_total_[kMaxNodes];

};
