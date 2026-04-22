#include "Bus_Protocol.h"

#include <string.h>

// -----------------------------------------------------------------------------
// CRC-16-CCITT
// Polynomial: 0x1021
// Init:       0xFFFF
// RefIn/Out:  false
// XorOut:     0x0000
// -----------------------------------------------------------------------------

uint16_t bus_crc16(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return 0xFFFF;
  }

  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; ++i) {
    crc ^= ((uint16_t)data[i] << 8);

    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000) {
        crc = (uint16_t)((crc << 1) ^ 0x1021);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

// -----------------------------------------------------------------------------
// Internal helper: compute frame CRC over
//   DST, SRC, TYPE, SEQ, LEN, PAYLOAD
// -----------------------------------------------------------------------------

static uint16_t bus_compute_frame_crc(const bus_frame_t* frame) {
  if (!frame) {
    return 0;
  }

  if (frame->len > BUS_MAX_PAYLOAD_SIZE) {
    return 0;
  }

  uint8_t crc_buf[5 + BUS_MAX_PAYLOAD_SIZE];
  crc_buf[0] = frame->dst;
  crc_buf[1] = frame->src;
  crc_buf[2] = frame->type;
  crc_buf[3] = frame->seq;
  crc_buf[4] = frame->len;

  if (frame->len > 0) {
    memcpy(&crc_buf[5], frame->payload, frame->len);
  }

  return bus_crc16(crc_buf, 5 + frame->len);
}

// -----------------------------------------------------------------------------
// Encode a frame into raw bytes:
//   SOF1 SOF2 DST SRC TYPE SEQ LEN PAYLOAD CRC_LO CRC_HI
// Returns total encoded size, or 0 on error.
// -----------------------------------------------------------------------------

size_t bus_encode_frame(uint8_t* out,
                        size_t out_size,
                        const bus_frame_t* frame) {
  if (!out || !frame) {
    return 0;
  }

  if (frame->len > BUS_MAX_PAYLOAD_SIZE) {
    return 0;
  }

  const size_t total_size = 2 + 5 + frame->len + 2;  // SOF + fields + payload + CRC
  if (out_size < total_size) {
    return 0;
  }

  const uint16_t crc = bus_compute_frame_crc(frame);

  size_t idx = 0;
  out[idx++] = BUS_SOF1;
  out[idx++] = BUS_SOF2;
  out[idx++] = frame->dst;
  out[idx++] = frame->src;
  out[idx++] = frame->type;
  out[idx++] = frame->seq;
  out[idx++] = frame->len;

  if (frame->len > 0) {
    memcpy(&out[idx], frame->payload, frame->len);
    idx += frame->len;
  }

  out[idx++] = (uint8_t)(crc & 0xFF);
  out[idx++] = (uint8_t)((crc >> 8) & 0xFF);

  return idx;
}

// -----------------------------------------------------------------------------
// Reset parser
// -----------------------------------------------------------------------------

void bus_parser_reset(bus_parser_t* parser) {
  if (!parser) {
    return;
  }

  memset(parser, 0, sizeof(*parser));
  parser->state = BUS_PARSE_WAIT_SOF1;
}

// -----------------------------------------------------------------------------
// Validate a completed frame
// -----------------------------------------------------------------------------

int bus_validate_frame(const bus_frame_t* frame) {
  if (!frame) {
    return 0;
  }

  if (frame->len > BUS_MAX_PAYLOAD_SIZE) {
    return 0;
  }

  const uint16_t expected_crc = bus_compute_frame_crc(frame);
  return (expected_crc == frame->crc) ? 1 : 0;
}

// -----------------------------------------------------------------------------
// Feed one byte into parser.
// Returns 1 only when a valid complete frame is ready.
// Returns 0 otherwise.
// If a frame is malformed or CRC-invalid, parser->frame_error is set.
// -----------------------------------------------------------------------------

int bus_parser_feed(bus_parser_t* parser,
                    uint8_t byte,
                    uint32_t now_ms) {
  if (!parser) {
    return 0;
  }

  // Clear one-shot output flags on each call.
  parser->frame_ready = 0;
  parser->frame_error = 0;

  // Inter-byte timeout protection.
  if (parser->state != BUS_PARSE_WAIT_SOF1) {
    uint32_t delta = now_ms - parser->last_byte_time_ms;
    if (delta > BUS_DEFAULT_INTERBYTE_TIMEOUT_MS) {
      bus_parser_reset(parser);
    }
  }

  parser->last_byte_time_ms = now_ms;

  switch (parser->state) {
    case BUS_PARSE_WAIT_SOF1:
      if (byte == BUS_SOF1) {
        parser->state = BUS_PARSE_WAIT_SOF2;
      }
      break;

    case BUS_PARSE_WAIT_SOF2:
      if (byte == BUS_SOF2) {
        parser->state = BUS_PARSE_DST;
      } else if (byte == BUS_SOF1) {
        // Stay here in case repeated SOF1 bytes appear.
        parser->state = BUS_PARSE_WAIT_SOF2;
      } else {
        parser->state = BUS_PARSE_WAIT_SOF1;
      }
      break;

    case BUS_PARSE_DST:
      parser->frame.dst = byte;
      parser->state = BUS_PARSE_SRC;
      break;

    case BUS_PARSE_SRC:
      parser->frame.src = byte;
      parser->state = BUS_PARSE_TYPE;
      break;

    case BUS_PARSE_TYPE:
      parser->frame.type = byte;
      parser->state = BUS_PARSE_SEQ;
      break;

    case BUS_PARSE_SEQ:
      parser->frame.seq = byte;
      parser->state = BUS_PARSE_LEN;
      break;

    case BUS_PARSE_LEN:
      parser->frame.len = byte;
      parser->payload_index = 0;

      if (parser->frame.len > BUS_MAX_PAYLOAD_SIZE) {
        parser->frame_error = 1;
        bus_parser_reset(parser);
        break;
      }

      if (parser->frame.len == 0) {
        parser->state = BUS_PARSE_CRC_LO;
      } else {
        parser->state = BUS_PARSE_PAYLOAD;
      }
      break;

    case BUS_PARSE_PAYLOAD:
      parser->frame.payload[parser->payload_index++] = byte;

      if (parser->payload_index >= parser->frame.len) {
        parser->state = BUS_PARSE_CRC_LO;
      }
      break;

    case BUS_PARSE_CRC_LO:
      parser->crc_lo = byte;
      parser->state = BUS_PARSE_CRC_HI;
      break;

    case BUS_PARSE_CRC_HI:
      parser->frame.crc = (uint16_t)parser->crc_lo | ((uint16_t)byte << 8);

      if (bus_validate_frame(&parser->frame)) {
        parser->frame_ready = 1;

        // Be ready immediately for the next frame.
        parser->state = BUS_PARSE_WAIT_SOF1;
        return 1;
      } else {
        parser->frame_error = 1;
        bus_parser_reset(parser);
      }
      break;

    default:
      bus_parser_reset(parser);
      break;
  }

  return 0;
}

// -----------------------------------------------------------------------------
// Derive node ID from MAC
// v1 policy: lowest 8 bits of MAC
// -----------------------------------------------------------------------------

uint8_t bus_node_id_from_mac(const uint8_t mac[6]) {
  if (!mac) {
    return 0;
  }

  return mac[5];
}