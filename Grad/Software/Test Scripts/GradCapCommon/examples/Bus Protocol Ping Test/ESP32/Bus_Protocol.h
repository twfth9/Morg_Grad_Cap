#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * ============================================================================
 * File: Bus_Protocol.h
 * Project: Grad Cap Display Bus Protocol
 * Target MCUs: SAMD21 master, ESP32 nodes
 * Protocol Version: 1.1 (first pass of TX_REPORT / BUS_STATE redesign)
 *
 * Description
 * -----------
 * This header defines the shared RS-485 bus protocol used by the SAMD21 master
 * and the ESP32 display nodes.
 *
 * System role summary
 * -------------------
 * - The SAMD21 is the master and bus orchestrator.
 * - The ESP32 boards are nodes on the shared half-duplex RS-485 bus.
 * - The master controls which node is allowed to transmit on the RS-485 bus by
 *   broadcasting BUS_STATE frames.
 * - The master also controls ownership of the shared micro SD card SPI path by
 *   publishing the current SD owner in the BUS_STATE frame.
 * - Nodes use the bus for coordination, command/response traffic, and debug
 *   reporting. Large image/video data is NOT transferred over RS-485.
 * - Nodes may begin SD-card access as soon as they observe that the master has
 *   assigned their node ID into BUS_STATE.sd_owner, even if they do not
 *   currently hold the RS-485 talking stick.
 *
 * Design intent
 * -------------
 * This protocol is intended to hide most of the awkward details of using a
 * shared half-duplex RS-485 link from the application code. In normal node code,
 * the bus should feel closer to a structured UART link with source/destination
 * addressing, turn ownership, and buffered debug messages.
 *
 * Reliability model
 * -----------------
 * - A node does NOT immediately discard the messages it sent during its most
 *   recent transmit turn.
 * - The master acknowledges a successful turn implicitly by advancing BUS_STATE
 *   to a later state without requesting retransmission from that node.
 * - If the master needs the node to resend its most recent transmit bundle, it
 *   reissues BUS_STATE with the same tx_owner and sets
 *   BUS_STATE_FLAG_RETRANSMIT.
 * - The node then retransmits its retained last-turn bundle.
 *
 * Frame format on wire
 * --------------------
 * Each frame is encoded on the wire in the following byte order:
 *
 *   Byte(s)   Name        Meaning
 *   -------   ----------  -----------------------------------------------
 *   0         SOF1        Start-of-frame byte 1 (0x55)
 *   1         SOF2        Start-of-frame byte 2 (0xAA)
 *   2         DST         Logical destination address
 *   3         SRC         Logical source address
 *   4         TYPE        Message type (bus_msg_type_t)
 *   5         SEQ         Sender-managed sequence number
 *   6         LEN         Payload length in bytes
 *   7..N      PAYLOAD     Message-specific payload bytes
 *   N+1       CRC_LO      CRC-16 low byte over header+payload
 *   N+2       CRC_HI      CRC-16 high byte over header+payload
 *
 * Addressing model
 * ----------------
 * - BUS_MASTER_ADDR is the SAMD21 master.
 * - BUS_BROADCAST_ADDR is received by all listeners.
 * - All ESP32 nodes have unique nonzero addresses.
 *
 * Message direction notation used in comments below
 * -------------------------------------------------
 * - Master -> Node      : Sent by the SAMD21, consumed by one node.
 * - Master -> Broadcast : Sent by the SAMD21, observed by all nodes.
 * - Node -> Master      : Sent by one ESP32, consumed by the SAMD21.
 * - Node -> Broadcast   : Sent by one ESP32, physically heard by all, but
 *                         usually only consumed by interested listeners.
 * - Node -> Node        : Optional future traffic; still physically broadcast
 *                         on the wire, but logically addressed to one node.
 * ============================================================================
 */

// -----------------------------------------------------------------------------
// Protocol Version
// -----------------------------------------------------------------------------

#define BUS_PROTOCOL_VERSION_MAJOR   1
#define BUS_PROTOCOL_VERSION_MINOR   1

// -----------------------------------------------------------------------------
// Framing Constants
// -----------------------------------------------------------------------------

/* Start-of-frame bytes used by the parser to locate frame boundaries. */
#define BUS_SOF1                     0x55
#define BUS_SOF2                     0xAA

/* Reserved logical addresses. */
#define BUS_MASTER_ADDR              0x00   /* SAMD21 master address */
#define BUS_BROADCAST_ADDR           0xFF   /* All listeners should inspect */

/*
 * Header size counts the bytes after SOF and before payload:
 * DST, SRC, TYPE, SEQ, LEN = 5 bytes.
 *
 * The encoded on-wire frame additionally contains 2 SOF bytes in front and
 * 2 CRC bytes at the end.
 */
#define BUS_HEADER_SIZE              5
#define BUS_CRC_SIZE                 2
#define BUS_SOF_SIZE                 2

/* Maximum payload size for any single protocol message. */
#define BUS_MAX_PAYLOAD_SIZE         64

/* Maximum encoded frame size on wire, including SOF, header, payload, CRC. */
#define BUS_MAX_FRAME_SIZE           (BUS_SOF_SIZE + BUS_HEADER_SIZE + BUS_MAX_PAYLOAD_SIZE + BUS_CRC_SIZE)

// -----------------------------------------------------------------------------
// Timing Defaults
// -----------------------------------------------------------------------------

/*
 * Default parser / transaction timing values.
 * These are protocol defaults and may be tuned later in implementation files.
 */
#define BUS_DEFAULT_INTERBYTE_TIMEOUT_MS   3
#define BUS_DEFAULT_REPLY_TIMEOUT_MS       15
#define BUS_DEFAULT_RETRY_COUNT            3

// -----------------------------------------------------------------------------
// Message Types
// -----------------------------------------------------------------------------

/*
 * Message type overview
 * ---------------------
 * 0x01-0x0F : Basic liveness / discovery
 * 0x10-0x1F : Master bus control / turn ownership / node reporting
 * 0x20-0x2F : Tasking / command traffic
 * 0x30-0x3F : Debug / diagnostic traffic
 * 0x40-0x4F : Optional application / peer traffic
 * 0x50-0x5F : Error / fault reporting
 */
typedef enum {
  /*
   * BUS_MSG_PING
   * Direction: Master -> Node
   * Purpose:
   *   Simple liveness check. The master may send this to a node to confirm that
   *   the node is alive and parsing frames correctly.
   */
  BUS_MSG_PING           = 0x01,

  /*
   * BUS_MSG_PONG
   * Direction: Node -> Master
   * Purpose:
   *   Response to BUS_MSG_PING. May optionally echo a token or include a small
   *   amount of liveness data.
   */
  BUS_MSG_PONG           = 0x02,

  /*
   * BUS_MSG_ENUM_REQUEST
   * Direction: Master -> Broadcast or Master -> Node
   * Purpose:
   *   Requests identification / capability information from one node or all
   *   nodes during startup and diagnostics.
   */
  BUS_MSG_ENUM_REQUEST   = 0x03,

  /*
   * BUS_MSG_ENUM_RESPONSE
   * Direction: Node -> Master
   * Purpose:
   *   Reports node identity, protocol version, firmware version, and capability
   *   bits to the master.
   */
  BUS_MSG_ENUM_RESPONSE  = 0x04,

  /*
   * BUS_MSG_BUS_STATE
   * Direction: Master -> Broadcast
   * Purpose:
   *   This is the master-controlled authoritative shared-state frame.
   *   It announces:
   *     - which node currently has the RS-485 talking stick (tx_owner)
   *     - which node currently owns the shared micro SD SPI resource (sd_owner)
   *     - flags such as retransmit request and startup state
   *
   *   Nodes should treat BUS_STATE as the source of truth for current bus and
   *   SD ownership.
   */
  BUS_MSG_BUS_STATE      = 0x10,

  /*
   * BUS_MSG_TX_REPORT
   * Direction: Node -> Master
   * Purpose:
   *   Sent by a node at the end of its transmit turn.
   *   This frame hands control back to the master and reports the node's
   *   current health / readiness summary.
   *
   *   It also includes reported_sd_owner, which is the node's current view of
   *   who owns the SD card. This does NOT override the master's authority; the
   *   SAMD21 remains the authoritative source of SD ownership and may choose to
   *   publish an updated BUS_STATE after processing the report.
   */
  BUS_MSG_TX_REPORT      = 0x11,

  /*
   * BUS_MSG_LOAD_ASSET
   * Direction: Master -> Node
   * Purpose:
   *   Tells a node what asset/file it should prepare to load from the SD card.
   *   The node will later act when it sees itself assigned as BUS_STATE.sd_owner.
   */
  BUS_MSG_LOAD_ASSET     = 0x20,

  /*
   * BUS_MSG_COMMAND_ACK
   * Direction: Node -> Master
   * Purpose:
   *   Positive acknowledgment that a command was accepted / understood.
   */
  BUS_MSG_COMMAND_ACK    = 0x21,

  /*
   * BUS_MSG_COMMAND_NACK
   * Direction: Node -> Master
   * Purpose:
   *   Negative acknowledgment that a command was rejected, unsupported, or
   *   invalid in the node's current state.
   */
  BUS_MSG_COMMAND_NACK   = 0x22,

  /*
   * BUS_MSG_DEBUG_TEXT
   * Direction: Node -> Master
   * Purpose:
   *   A single complete debug/log line generated by an ESP32 node.
   *   These messages are buffered locally by the node and transmitted when the
   *   master grants that node the talking stick.
   */
  BUS_MSG_DEBUG_TEXT     = 0x30,

  /*
   * BUS_MSG_APP_DATA
   * Direction: Node -> Node, Node -> Master, or Master -> Node
   * Purpose:
   *   Generic future application payload. This is intentionally flexible so
   *   peer-to-peer or other special traffic has a defined home in the protocol.
   */
  BUS_MSG_APP_DATA       = 0x40,

  /*
   * BUS_MSG_ERROR_REPORT
   * Direction: Node -> Master
   * Purpose:
   *   Structured fault / error report, separate from free-form debug text.
   */
  BUS_MSG_ERROR_REPORT   = 0x50

} bus_msg_type_t;

// -----------------------------------------------------------------------------
// Result Codes
// -----------------------------------------------------------------------------

typedef enum {
  BUS_RESULT_OK          = 0x00,
  BUS_RESULT_INVALID     = 0x01,
  BUS_RESULT_BAD_STATE   = 0x02,
  BUS_RESULT_BAD_CRC     = 0x03,
  BUS_RESULT_TIMEOUT     = 0x04,
  BUS_RESULT_BUSY        = 0x05,
  BUS_RESULT_DUPLICATE   = 0x06,
  BUS_RESULT_UNSUPPORTED = 0x07
} bus_result_t;

// -----------------------------------------------------------------------------
// BUS_STATE Flags (Master-authored)
// -----------------------------------------------------------------------------

/*
 * BUS_STATE.flags is authored by the SAMD21 master and broadcast to all nodes.
 * This field describes shared system-level state, not node-local state.
 *
 * Bit layout:
 *
 *   Bit  Name                        Meaning
 *   ---  --------------------------  -----------------------------------------
 *    0   BUS_STATE_FLAG_RETRANSMIT   The current tx_owner must retransmit the
 *                                    last full transmit bundle it sent on its
 *                                    previous turn.
 *
 *    1   BUS_STATE_FLAG_SD_BUSY      The shared micro SD SPI resource is
 *                                    currently in use. When this bit is set,
 *                                    BUS_STATE.sd_owner should be nonzero.
 *                                    When clear, sd_owner should be 0.
 *
 *    2   BUS_STATE_FLAG_STARTUP      System is still in startup / enumeration /
 *                                    preload behavior rather than steady-state
 *                                    display runtime.
 *
 *    3   BUS_STATE_FLAG_ERROR        The master has latched a system-level
 *                                    error condition somewhere in the network.
 *
 *   4-7  Reserved                    Reserved for future use. Must currently be
 *                                    transmitted as 0 and ignored if unknown.
 */
#define BUS_STATE_FLAG_RETRANSMIT    0x01
#define BUS_STATE_FLAG_SD_BUSY       0x02
#define BUS_STATE_FLAG_STARTUP       0x04
#define BUS_STATE_FLAG_ERROR         0x08

// -----------------------------------------------------------------------------
// TX_REPORT Status Bits (Node-authored)
// -----------------------------------------------------------------------------

/*
 * TX_REPORT.status_bits is authored by a node and sent to the master at the end
 * of that node's transmit turn.
 *
 * This 16-bit field packs multiple state summaries into one compact value.
 * Some fields are multi-bit enumerations, while others are single-bit flags.
 *
 * Full bit layout:
 *
 *   Bits  Name                 Meaning
 *   ----  -------------------  -----------------------------------------------
 *   0-1   LCD status           Current LCD initialization / health state.
 *   2-4   Asset status         Current asset/data loading / display state.
 *     5   MORE_PENDING         Node still has more outbound messages pending.
 *     6   DEBUG_PENDING        Node still has queued debug lines pending.
 *     7   SD_ACTIVE            Node is actively using the SD card right now.
 *     8   MEM_WARNING          Node is nearing a memory/storage limit.
 *     9   ERROR_PRESENT        error_code field is meaningful / active error.
 *    10   DISPLAY_READY        Node is ready to participate in image playback.
 *    11   WORK_PENDING         Node still has unfinished work to do.
 *  12-15  Reserved             Reserved for future use.
 */
#define BUS_STATUS_LCD_SHIFT         0
#define BUS_STATUS_LCD_MASK          0x0003

#define BUS_STATUS_ASSET_SHIFT       2
#define BUS_STATUS_ASSET_MASK        0x001C

/* Single-bit node status flags. */
#define BUS_STATUS_MORE_PENDING      0x0020
#define BUS_STATUS_DEBUG_PENDING     0x0040
#define BUS_STATUS_SD_ACTIVE         0x0080
#define BUS_STATUS_MEM_WARNING       0x0100
#define BUS_STATUS_ERROR_PRESENT     0x0200
#define BUS_STATUS_DISPLAY_READY     0x0400
#define BUS_STATUS_WORK_PENDING      0x0800

/*
 * LCD state encoding stored in bits 0-1 of TX_REPORT.status_bits.
 *
 *   Value  Name                      Meaning
 *   -----  ------------------------  -----------------------------------------
 *    0     BUS_LCD_STATUS_UNINIT     LCD has not been initialized yet.
 *    1     BUS_LCD_STATUS_INITING    LCD initialization/configuration is in
 *                                    progress.
 *    2     BUS_LCD_STATUS_READY      LCD is initialized and operating normally.
 *    3     BUS_LCD_STATUS_ERROR      LCD initialization or runtime fault.
 */
typedef enum {
  BUS_LCD_STATUS_UNINIT   = 0x0,
  BUS_LCD_STATUS_INITING  = 0x1,
  BUS_LCD_STATUS_READY    = 0x2,
  BUS_LCD_STATUS_ERROR    = 0x3
} bus_lcd_status_t;

/*
 * Asset state encoding stored in bits 2-4 of TX_REPORT.status_bits.
 *
 *   Value  Name                           Meaning
 *   -----  -----------------------------  ------------------------------------
 *    0     BUS_ASSET_STATUS_NONE          No asset assigned / nothing loaded.
 *    1     BUS_ASSET_STATUS_ASSIGNED      Asset has been assigned, but loading
 *                                         has not meaningfully begun.
 *    2     BUS_ASSET_STATUS_WAITING_FOR_SD
 *                                         Node is waiting for the master to
 *                                         assign it SD ownership.
 *    3     BUS_ASSET_STATUS_LOADING       Node is actively loading asset data
 *                                         from the SD card and/or staging it
 *                                         into PSRAM/external flash.
 *    4     BUS_ASSET_STATUS_LOADED        Asset data is loaded and ready.
 *    5     BUS_ASSET_STATUS_DISPLAYING    Asset is currently being displayed.
 *    6     BUS_ASSET_STATUS_ERROR         Asset load/display operation failed.
 *    7     BUS_ASSET_STATUS_RESERVED      Reserved.
 */
typedef enum {
  BUS_ASSET_STATUS_NONE            = 0x0,
  BUS_ASSET_STATUS_ASSIGNED        = 0x1,
  BUS_ASSET_STATUS_WAITING_FOR_SD  = 0x2,
  BUS_ASSET_STATUS_LOADING         = 0x3,
  BUS_ASSET_STATUS_LOADED          = 0x4,
  BUS_ASSET_STATUS_DISPLAYING      = 0x5,
  BUS_ASSET_STATUS_ERROR           = 0x6,
  BUS_ASSET_STATUS_RESERVED        = 0x7
} bus_asset_status_t;

/* Helper macros for packing / unpacking the 16-bit BUS_STATUS field. */
#define BUS_STATUS_SET_LCD(v) \
  ((((uint16_t)(v)) << BUS_STATUS_LCD_SHIFT) & BUS_STATUS_LCD_MASK)

#define BUS_STATUS_GET_LCD(bits) \
  ((((uint16_t)(bits)) & BUS_STATUS_LCD_MASK) >> BUS_STATUS_LCD_SHIFT)

#define BUS_STATUS_SET_ASSET(v) \
  ((((uint16_t)(v)) << BUS_STATUS_ASSET_SHIFT) & BUS_STATUS_ASSET_MASK)

#define BUS_STATUS_GET_ASSET(bits) \
  ((((uint16_t)(bits)) & BUS_STATUS_ASSET_MASK) >> BUS_STATUS_ASSET_SHIFT)

// -----------------------------------------------------------------------------
// Capability Flags
// -----------------------------------------------------------------------------

/*
 * Capability bits reported by a node in ENUM_RESPONSE.capabilities.
 * These let the master understand what a node supports.
 */
typedef enum {
  BUS_CAP_DEBUG_TEXT     = 0x01,  /* Node can emit BUS_MSG_DEBUG_TEXT */
  BUS_CAP_LOAD_ASSET     = 0x02,  /* Node supports BUS_MSG_LOAD_ASSET */
  BUS_CAP_PSRAM          = 0x04,  /* Node has usable PSRAM staging memory */
  BUS_CAP_EXT_FLASH      = 0x08,  /* Node has usable external flash staging */
  BUS_CAP_APP_DATA       = 0x10   /* Node supports BUS_MSG_APP_DATA */
} bus_capabilities_t;

// -----------------------------------------------------------------------------
// Frame Definition (decoded representation)
// -----------------------------------------------------------------------------

/*
 * bus_frame_t is the decoded in-memory frame representation used by the parser
 * and encoder. The SOF bytes are not stored here because they are framing bytes
 * used only on the wire.
 */
typedef struct {
  uint8_t dst;                              /* Logical destination address */
  uint8_t src;                              /* Logical source address */
  uint8_t type;                             /* bus_msg_type_t */
  uint8_t seq;                              /* Sender-managed sequence number */
  uint8_t len;                              /* Payload byte count */
  uint8_t payload[BUS_MAX_PAYLOAD_SIZE];    /* Message-specific payload */
  uint16_t crc;                             /* CRC-16 over header+payload */
} bus_frame_t;

// -----------------------------------------------------------------------------
// Parser State
// -----------------------------------------------------------------------------

typedef enum {
  BUS_PARSE_WAIT_SOF1 = 0,
  BUS_PARSE_WAIT_SOF2,
  BUS_PARSE_DST,
  BUS_PARSE_SRC,
  BUS_PARSE_TYPE,
  BUS_PARSE_SEQ,
  BUS_PARSE_LEN,
  BUS_PARSE_PAYLOAD,
  BUS_PARSE_CRC_LO,
  BUS_PARSE_CRC_HI
} bus_parse_state_t;

typedef struct {
  bus_parse_state_t state;
  bus_frame_t frame;
  uint8_t payload_index;
  uint8_t crc_lo;
  uint32_t last_byte_time_ms;
  uint8_t frame_ready;
  uint8_t frame_error;
} bus_parser_t;

// -----------------------------------------------------------------------------
// Message Payload Structures
// -----------------------------------------------------------------------------

/*
 * ENUM_RESPONSE payload
 * Direction: Node -> Master
 *
 * Fields:
 * - node_id:             Node's logical bus address.
 * - mac:                 6-byte MAC used to derive / identify the node.
 * - protocol_major/minor Shared protocol version implemented by the node.
 * - fw_major/minor:      Node firmware version.
 * - capabilities:        Capability bitfield (bus_capabilities_t).
 */
typedef struct {
  uint8_t node_id;
  uint8_t mac[6];
  uint8_t protocol_major;
  uint8_t protocol_minor;
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t capabilities;
} bus_enum_response_t;

/*
 * BUS_STATE payload
 * Direction: Master -> Broadcast
 *
 * Fields:
 * - tx_owner:  Node currently allowed to transmit on RS-485.
 * - sd_owner:  Node currently allowed to access the shared micro SD SPI path.
 *              0 means the SD card is currently unowned / free.
 * - state_seq: Monotonic master state counter. Useful for detecting new state
 *              broadcasts and distinguishing them from stale duplicates.
 * - flags:     BUS_STATE_FLAG_* bitfield.
 */
typedef struct {
  uint8_t tx_owner;
  uint8_t sd_owner;
  uint16_t state_seq;
  uint8_t flags;
} bus_bus_state_t;

/*
 * TX_REPORT payload
 * Direction: Node -> Master
 *
 * This is sent at the end of a node's transmit turn.
 *
 * Fields:
 * - reported_sd_owner:  The node's current view of who owns the SD card.
 *                       This is informative only; the SAMD21 remains the
 *                       authoritative publisher of SD ownership through
 *                       BUS_STATE.sd_owner.
 * - status_bits:        16-bit packed node state field described above.
 * - error_code:         Structured node error code. Meaningful when
 *                       BUS_STATUS_ERROR_PRESENT is set.
 * - uptime_ms:          Milliseconds since node boot.
 * - psram_used_bytes:   Current PSRAM usage estimate.
 * - psram_total_bytes:  Total PSRAM capacity available to the node.
 * - flash_used_bytes:   Current external flash usage estimate.
 * - flash_total_bytes:  Total external flash capacity available to the node.
 */
typedef struct {
  uint8_t  reported_sd_owner;
  uint16_t status_bits;
  uint16_t error_code;
  uint32_t uptime_ms;
  uint32_t psram_used_bytes;
  uint32_t psram_total_bytes;
  uint32_t flash_used_bytes;
  uint32_t flash_total_bytes;
} bus_tx_report_t;

/*
 * LOAD_ASSET payload
 * Direction: Master -> Node
 *
 * Fields:
 * - storage_target: Storage destination hint. The implementation may define
 *                   values such as PSRAM, external flash, or auto-select.
 * - flags:          Command-specific options.
 * - filename_len:   Number of valid bytes in filename[].
 * - filename[]:     Variable-length asset filename bytes.
 */
typedef struct {
  uint8_t storage_target;
  uint8_t flags;
  uint8_t filename_len;
  char filename[BUS_MAX_PAYLOAD_SIZE - 3];
} bus_load_asset_t;

/*
 * COMMAND_ACK payload
 * Direction: Node -> Master
 *
 * - acked_type:  Message type being acknowledged.
 * - result:      BUS_RESULT_OK or another result code if desired.
 */
typedef struct {
  uint8_t acked_type;
  uint8_t result;
} bus_command_ack_t;

/*
 * COMMAND_NACK payload
 * Direction: Node -> Master
 *
 * - nacked_type: Message type being rejected.
 * - result:      Reason for rejection.
 */
typedef struct {
  uint8_t nacked_type;
  uint8_t result;
} bus_command_nack_t;

/*
 * DEBUG_TEXT payload
 * Direction: Node -> Master
 *
 * A single complete debug/log line.
 *
 * - level:      Logging severity / class (implementation-defined for now).
 * - text_len:   Number of valid bytes in text[].
 * - text[]:     ASCII/UTF-8 text bytes for one complete debug message.
 */
typedef struct {
  uint8_t level;
  uint8_t text_len;
  char text[BUS_MAX_PAYLOAD_SIZE - 2];
} bus_debug_text_t;

/*
 * ERROR_REPORT payload
 * Direction: Node -> Master
 *
 * - error_code: Structured fault code.
 * - context:    Optional small context byte.
 */
typedef struct {
  uint16_t error_code;
  uint8_t context;
} bus_error_report_t;

/*
 * APP_DATA payload
 * Direction: Flexible (future use)
 *
 * - subtype:    Application-defined subtype.
 * - data[]:     Application-defined payload bytes.
 */
typedef struct {
  uint8_t subtype;
  uint8_t data[BUS_MAX_PAYLOAD_SIZE - 1];
} bus_app_data_t;

// -----------------------------------------------------------------------------
// Master Node Table Support
// -----------------------------------------------------------------------------

/*
 * Cached node information held by the master.
 * This is not transmitted directly as a wire format. It is the master's local
 * bookkeeping structure for what it currently knows about a node.
 */
typedef struct {
  uint8_t active;               /* Slot populated / node known to master */
  uint8_t online;               /* Node responded recently */
  uint8_t addr;                 /* Logical node address */
  uint8_t mac[6];               /* Node MAC for identification */
  uint8_t protocol_major;
  uint8_t protocol_minor;
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t capabilities;
  uint16_t last_status_bits;    /* Most recent TX_REPORT.status_bits */
  uint16_t last_error_code;     /* Most recent node error code */
  uint32_t last_uptime_ms;      /* Most recent reported uptime */
  uint32_t last_seen_ms;        /* Master-side timestamp of latest contact */
} bus_node_info_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

/* CRC helper for encoded header+payload bytes. */
uint16_t bus_crc16(const uint8_t* data, size_t len);

/*
 * Encodes a decoded bus_frame_t into an on-wire byte stream with SOF and CRC.
 * Returns the total encoded byte count on success, or 0 on failure.
 */
size_t bus_encode_frame(uint8_t* out,
                        size_t out_size,
                        const bus_frame_t* frame);

/*
 * Feeds one received byte into the parser state machine.
 * Returns nonzero when a complete frame becomes ready.
 */
int bus_parser_feed(bus_parser_t* parser,
                    uint8_t byte,
                    uint32_t now_ms);

/* Resets parser state to waiting-for-SOF. */
void bus_parser_reset(bus_parser_t* parser);

/* Validates frame length / CRC / basic structural rules. */
int bus_validate_frame(const bus_frame_t* frame);

/* Default helper for deriving a compact node ID from a MAC address. */
uint8_t bus_node_id_from_mac(const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif
