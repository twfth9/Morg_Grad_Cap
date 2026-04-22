#pragma once

#include <stdint.h>
#include <stddef.h>

//
// Bus Protocol (Grad Cap)
// Version 1
//
// Master: SAMD21
// Nodes:  ESP32 devices
//
// Transport: RS-485 (half-duplex)
// Arbitration: Master-controlled
//

// -----------------------------------------------------------------------------
// Version
// -----------------------------------------------------------------------------

#define BUS_PROTOCOL_VERSION_MAJOR   1
#define BUS_PROTOCOL_VERSION_MINOR   0

// -----------------------------------------------------------------------------
// Framing
// -----------------------------------------------------------------------------

#define BUS_SOF1                    0x55
#define BUS_SOF2                    0xAA

#define BUS_MASTER_ADDR             0x00
#define BUS_BROADCAST_ADDR          0xFF

#define BUS_HEADER_SIZE             7
#define BUS_CRC_SIZE                2

#define BUS_MAX_PAYLOAD_SIZE        64
#define BUS_MAX_FRAME_SIZE          (BUS_HEADER_SIZE + BUS_MAX_PAYLOAD_SIZE + BUS_CRC_SIZE)

// -----------------------------------------------------------------------------
// Timing (defaults)
// -----------------------------------------------------------------------------

#define BUS_DEFAULT_INTERBYTE_TIMEOUT_MS   3
#define BUS_DEFAULT_REPLY_TIMEOUT_MS       15
#define BUS_DEFAULT_RETRY_COUNT            3

// -----------------------------------------------------------------------------
// Message Types
// -----------------------------------------------------------------------------

typedef enum {
  BUS_MSG_PING             = 0x01,
  BUS_MSG_PONG             = 0x02,

  BUS_MSG_ENUM_REQUEST     = 0x10,
  BUS_MSG_ENUM_RESPONSE    = 0x11,

  BUS_MSG_STATUS_REQUEST   = 0x12,
  BUS_MSG_STATUS_RESPONSE  = 0x13,

  BUS_MSG_COMMAND          = 0x20,
  BUS_MSG_COMMAND_ACK      = 0x21,
  BUS_MSG_COMMAND_NACK     = 0x22,

  BUS_MSG_DEBUG_REQUEST    = 0x30,
  BUS_MSG_DEBUG_DATA       = 0x31,

  BUS_MSG_DISPLAY_DATA     = 0x40,
  BUS_MSG_DISPLAY_ACK      = 0x41,

  BUS_MSG_ERROR_REPORT     = 0x50

} bus_msg_type_t;

// -----------------------------------------------------------------------------
// Status Flags
// -----------------------------------------------------------------------------

typedef enum {
  BUS_STATUS_ALIVE            = 0x01,
  BUS_STATUS_DEBUG_PENDING    = 0x02,
  BUS_STATUS_DISPLAY_READY    = 0x04,
  BUS_STATUS_DEBUG_OVERFLOW   = 0x08,
  BUS_STATUS_ERROR_LATCHED    = 0x10
} bus_status_flags_t;

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
// Frame
// -----------------------------------------------------------------------------

typedef struct {
  uint8_t dst;
  uint8_t src;
  uint8_t type;
  uint8_t seq;
  uint8_t len;
  uint8_t payload[BUS_MAX_PAYLOAD_SIZE];
  uint16_t crc;
} bus_frame_t;

// -----------------------------------------------------------------------------
// Parser
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
// ENUM RESPONSE
// -----------------------------------------------------------------------------

typedef struct {
  uint8_t node_id;
  uint8_t mac[6];
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t capabilities;
} bus_enum_response_t;

// -----------------------------------------------------------------------------
// Capability Flags
// -----------------------------------------------------------------------------

typedef enum {
  BUS_CAP_DEBUG_TEXT    = 0x01,
  BUS_CAP_DISPLAY_NODE  = 0x02,
  BUS_CAP_STATUS_REPORT = 0x04
} bus_capabilities_t;

// -----------------------------------------------------------------------------
// STATUS RESPONSE
// -----------------------------------------------------------------------------

typedef struct {
  uint8_t  flags;
  uint16_t debug_bytes;
  uint8_t  error_code;
  uint32_t uptime;
} bus_status_response_t;

// -----------------------------------------------------------------------------
// DEBUG
// -----------------------------------------------------------------------------

typedef struct {
  uint8_t max_bytes;
} bus_debug_request_t;

typedef struct {
  uint8_t count;
  uint8_t more;
  uint8_t data[BUS_MAX_PAYLOAD_SIZE - 2];
} bus_debug_data_t;

// -----------------------------------------------------------------------------
// MASTER NODE TABLE
// -----------------------------------------------------------------------------

typedef struct {
  uint8_t active;      // slot is populated / node is known
  uint8_t online;      // node has responded recently
  uint8_t addr;
  uint8_t mac[6];
  uint8_t fw_major;
  uint8_t fw_minor;
  uint8_t capabilities;
  uint8_t status_flags;
  uint8_t error_code;
  uint32_t last_seen_ms;
} bus_node_info_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

uint16_t bus_crc16(const uint8_t* data, size_t len);

size_t bus_encode_frame(uint8_t* out,
                        size_t out_size,
                        const bus_frame_t* frame);

int bus_parser_feed(bus_parser_t* parser,
                    uint8_t byte,
                    uint32_t now_ms);

void bus_parser_reset(bus_parser_t* parser);

int bus_validate_frame(const bus_frame_t* frame);

uint8_t bus_node_id_from_mac(const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif