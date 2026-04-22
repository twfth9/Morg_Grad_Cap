#include "Bus_Protocol.h"

#include <string.h>

/*
 * ============================================================================
 * File: Bus_Protocol.cpp
 * Project: Grad Cap Display Bus Protocol
 *
 * Description
 * -----------
 * First-pass implementation of the shared protocol helper functions declared in
 * Bus_Protocol.h.
 *
 * This file implements:
 * - CRC-16 calculation for bus frames
 * - frame encoding from bus_frame_t into on-wire bytes
 * - byte-by-byte parser state machine
 * - basic structural validation helpers
 * - default helper for deriving a compact node ID from a MAC address
 *
 * Notes
 * -----
 * - This file remains intentionally protocol-generic. It contains no node-side
 *   or master-side scheduling logic.
 * - The parser consumes bytes one at a time and marks parser->frame_ready when
 *   a complete candidate frame has been assembled.
 * - Structural validation is intentionally conservative in first pass and can
 *   be extended later if the protocol evolves.
 * ============================================================================
 */

uint16_t bus_crc16(const uint8_t* data, size_t len) {
  if (data == nullptr || len == 0) {
    return 0;
  }

  /*
   * CRC-16/IBM style implementation (polynomial 0xA001 reflected).
   *
   * This is a simple, compact bitwise implementation that is easy to inspect
   * and sufficient for the modest bus bandwidth used in this project.
   */
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]);

    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x0001) {
        crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001u);
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

size_t bus_encode_frame(uint8_t* out,
                        size_t out_size,
                        const bus_frame_t* frame) {
  if (out == nullptr || frame == nullptr) {
    return 0;
  }

  if (frame->len > BUS_MAX_PAYLOAD_SIZE) {
    return 0;
  }

  const size_t encoded_size = BUS_SOF_SIZE + BUS_HEADER_SIZE + frame->len + BUS_CRC_SIZE;
  if (out_size < encoded_size) {
    return 0;
  }

  /*
   * On-wire layout:
   *   SOF1 SOF2 DST SRC TYPE SEQ LEN PAYLOAD... CRC_LO CRC_HI
   */
  out[0] = BUS_SOF1;
  out[1] = BUS_SOF2;
  out[2] = frame->dst;
  out[3] = frame->src;
  out[4] = frame->type;
  out[5] = frame->seq;
  out[6] = frame->len;

  if (frame->len > 0) {
    memcpy(&out[7], frame->payload, frame->len);
  }

  const uint16_t crc = bus_crc16(&out[2], BUS_HEADER_SIZE + frame->len);
  out[7 + frame->len] = static_cast<uint8_t>(crc & 0x00FFu);
  out[8 + frame->len] = static_cast<uint8_t>((crc >> 8) & 0x00FFu);

  return encoded_size;
}

void bus_parser_reset(bus_parser_t* parser) {
  if (parser == nullptr) {
    return;
  }

  memset(&parser->frame, 0, sizeof(parser->frame));
  parser->state = BUS_PARSE_WAIT_SOF1;
  parser->payload_index = 0;
  parser->crc_lo = 0;
  parser->last_byte_time_ms = 0;
  parser->frame_ready = 0;
  parser->frame_error = 0;
}

int bus_parser_feed(bus_parser_t* parser,
                    uint8_t byte,
                    uint32_t now_ms) {
  if (parser == nullptr) {
    return 0;
  }

  /*
   * Interbyte timeout handling.
   *
   * If the parser is in the middle of a frame and too much time has elapsed
   * since the previous byte, abandon the partial frame and resynchronize.
   */
  if (parser->state != BUS_PARSE_WAIT_SOF1) {
    const uint32_t elapsed = now_ms - parser->last_byte_time_ms;
    if (elapsed > BUS_DEFAULT_INTERBYTE_TIMEOUT_MS) {
      bus_parser_reset(parser);
    }
  }

  parser->last_byte_time_ms = now_ms;
  parser->frame_ready = 0;
  parser->frame_error = 0;

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
        /* Allow overlapping SOF detection if another SOF1 arrives. */
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

    case BUS_PARSE_CRC_HI: {
      parser->frame.crc = static_cast<uint16_t>(parser->crc_lo) |
                          static_cast<uint16_t>(byte << 8);

      if (bus_validate_frame(&parser->frame)) {
        parser->frame_ready = 1;
        parser->state = BUS_PARSE_WAIT_SOF1;
        return 1;
      }

      parser->frame_error = 1;
      bus_parser_reset(parser);
      break;
    }

    default:
      bus_parser_reset(parser);
      break;
  }

  return 0;
}

int bus_validate_frame(const bus_frame_t* frame) {
  if (frame == nullptr) {
    return 0;
  }

  if (frame->len > BUS_MAX_PAYLOAD_SIZE) {
    return 0;
  }

  /*
   * Rebuild the logical header+payload byte stream used for CRC coverage:
   *   DST SRC TYPE SEQ LEN PAYLOAD...
   */
  uint8_t crc_buf[BUS_HEADER_SIZE + BUS_MAX_PAYLOAD_SIZE];
  crc_buf[0] = frame->dst;
  crc_buf[1] = frame->src;
  crc_buf[2] = frame->type;
  crc_buf[3] = frame->seq;
  crc_buf[4] = frame->len;

  if (frame->len > 0) {
    memcpy(&crc_buf[5], frame->payload, frame->len);
  }

  const uint16_t computed_crc = bus_crc16(crc_buf, BUS_HEADER_SIZE + frame->len);
  if (computed_crc != frame->crc) {
    return 0;
  }

  /*
   * First-pass structural checks.
   *
   * These are intentionally modest. The goal is to reject obviously malformed
   * frames while keeping room for future protocol growth.
   */
  if (frame->type == 0) {
    return 0;
  }

  /* Source should not be broadcast. Logical destinations may be broadcast. */
  if (frame->src == BUS_BROADCAST_ADDR) {
    return 0;
  }

  return 1;
}

uint8_t bus_node_id_from_mac(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return 0;
  }

  /*
   * First-pass compact node ID helper.
   *
   * Use the low 7 bits of the final MAC byte, offset upward to avoid reserved
   * addresses. This is simple and consistent with the earlier design intent of
   * deriving node identity from the unique ESP MAC.
   *
   * Reserved addresses:
   * - BUS_MASTER_ADDR    = 0x00
   * - BUS_BROADCAST_ADDR = 0xFF
   *
   * We therefore map into the range 0x02..0x7F for node use.
   */
  uint8_t candidate = static_cast<uint8_t>(mac[5] & 0x7Fu);

  if (candidate < 0x02) {
    candidate = static_cast<uint8_t>(candidate + 0x02u);
  }

  if (candidate == BUS_BROADCAST_ADDR || candidate == BUS_MASTER_ADDR) {
    candidate = 0x02;
  }

  return candidate;
}
