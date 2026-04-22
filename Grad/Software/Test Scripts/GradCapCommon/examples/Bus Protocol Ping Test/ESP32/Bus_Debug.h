#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Bus_Node.h"

/*
 * ============================================================================
 * File: Bus_Debug.h
 * Project: Grad Cap Display Bus Protocol
 * Target MCU: ESP32 display nodes
 *
 * Description
 * -----------
 * This header declares the BusDebug class, a user-facing debug/logging wrapper
 * built on top of BusNode.
 *
 * BusDebug provides a Serial-like API so ESP32 application code can write:
 *
 *   DebugBus.print("Hello");
 *   DebugBus.println(" world");
 *   DebugBus.printf("Count=%u\n", count);
 *
 * instead of manually constructing BUS_MSG_DEBUG_TEXT messages.
 *
 * BusDebug is responsible for:
 * - Presenting a simple print/println/printf style interface to node code
 * - Buffering characters locally until a complete line/message is ready
 * - Converting completed lines into BUS_MSG_DEBUG_TEXT payloads
 * - Queueing those messages into the associated BusNode for later transmission
 * - Optionally prepending metadata such as a local prefix string
 *
 * BusDebug is NOT responsible for:
 * - Defining protocol frame formats (Bus_Protocol does that)
 * - Owning the outbound bus queue (BusNode does that)
 * - Deciding when RS-485 transmission actually occurs
 * - Parsing incoming protocol traffic
 *
 * Message model
 * -------------
 * - Debug text is treated as a sequence of complete logical messages, not as a
 *   raw byte stream.
 * - A complete line is typically finalized when '\n' is written.
 * - Each completed line becomes one BUS_MSG_DEBUG_TEXT message queued into the
 *   BusNode.
 * - The SAMD21 master later receives those messages and can present them as:
 *     [node_id] message text
 *
 * First-pass design notes
 * -----------------------
 * - The first implementation uses a fixed-size local line buffer.
 * - If the line buffer fills before a newline is seen, BusDebug may flush the
 *   line early or mark truncation, depending on implementation choice.
 * - Severity level is included in the payload format but left implementation-
 *   defined for now. First pass may default all lines to one common level.
 * ============================================================================
 */

/*
 * Debug severity / class byte carried in BUS_MSG_DEBUG_TEXT.level.
 *
 * These values are placeholders for first pass. The protocol supports carrying
 * this field now even if the master does not yet treat levels differently.
 */
typedef enum {
  BUS_DEBUG_LEVEL_TRACE = 0,
  BUS_DEBUG_LEVEL_INFO,
  BUS_DEBUG_LEVEL_WARN,
  BUS_DEBUG_LEVEL_ERROR
} bus_debug_level_t;

class BusDebug {
 public:
  // ---------------------------------------------------------------------------
  // Construction / Initialization
  // ---------------------------------------------------------------------------

  /*
   * Construct an unattached BusDebug instance.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - new BusDebug object with no associated BusNode
   */
  BusDebug();

  /*
   * Attach this debug wrapper to a BusNode.
   *
   * Inputs:
   * - node: pointer to the BusNode that should receive queued debug messages
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - Must be called before print/println/printf are used.
   */
  void begin(BusNode* node);

  /*
   * Check whether this debug wrapper is currently attached to a BusNode.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - bool: true if a node pointer is attached
   */
  bool isAttached() const;

  // ---------------------------------------------------------------------------
  // Configuration
  // ---------------------------------------------------------------------------

  /*
   * Enable or disable debug output generation.
   *
   * Inputs:
   * - enabled: true to queue debug messages, false to ignore writes
   *
   * Outputs:
   * - none
   */
  void setEnabled(bool enabled);

  /* Return whether debug output is currently enabled. */
  bool isEnabled() const;

  /*
   * Set the default severity/class byte used for newly queued debug messages.
   *
   * Inputs:
   * - level: default bus_debug_level_t for future lines
   *
   * Outputs:
   * - none
   */
  void setLevel(uint8_t level);

  /* Return the current default severity/class byte. */
  uint8_t getLevel() const;

  /*
   * Set an optional local prefix string that will be prepended to each debug
   * line before it is queued.
   *
   * Inputs:
   * - prefix: null-terminated string, or nullptr to clear
   *
   * Outputs:
   * - none
   *
   * Notes:
   * - This is optional convenience behavior. It is separate from the node ID,
   *   which the SAMD21 master will usually prepend when presenting messages.
   */
  void setPrefix(const char* prefix);

  /* Return the currently configured local prefix string. */
  const char* getPrefix() const;

  // ---------------------------------------------------------------------------
  // Serial-like Write API
  // ---------------------------------------------------------------------------

  /*
   * Write one raw character into the local debug line buffer.
   *
   * Inputs:
   * - c: character to append
   *
   * Outputs:
   * - size_t: number of characters accepted (0 or 1)
   *
   * Notes:
   * - If c is '\n', the current line is finalized and queued as one
   *   BUS_MSG_DEBUG_TEXT message.
   */
  size_t write(char c);

  /*
   * Write a block of raw bytes into the local debug line buffer.
   *
   * Inputs:
   * - data: pointer to bytes to append
   * - len: number of bytes to append
   *
   * Outputs:
   * - size_t: number of bytes accepted
   */
  size_t write(const uint8_t* data, size_t len);

  /*
   * Append a null-terminated string to the local debug line buffer.
   *
   * Inputs:
   * - text: null-terminated text string
   *
   * Outputs:
   * - size_t: number of characters accepted
   */
  size_t print(const char* text);

  /* Append a signed integer in decimal form. */
  size_t print(int value);

  /* Append an unsigned integer in decimal form. */
  size_t print(unsigned int value);

  /* Append a long integer in decimal form. */
  size_t print(long value);

  /* Append an unsigned long integer in decimal form. */
  size_t print(unsigned long value);

  /* Append a string followed by newline, causing the line to be finalized. */
  size_t println(const char* text);

  /* Finalize the current line by appending newline and queueing it. */
  size_t println();

  /* Append an integer followed by newline. */
  size_t println(int value);

  /* Append an unsigned integer followed by newline. */
  size_t println(unsigned int value);

  /*
   * printf-style formatter that appends formatted text to the current line
   * buffer.
   *
   * Inputs:
   * - fmt: printf-style format string
   * - ...: formatting arguments
   *
   * Outputs:
   * - int: number of formatted characters produced, or negative on error
   *
   * Notes:
   * - If the formatted output contains '\n', the line may be finalized.
   */
  int printf(const char* fmt, ...);

  // ---------------------------------------------------------------------------
  // Line Finalization / Flush Control
  // ---------------------------------------------------------------------------

  /*
   * Finalize the current line buffer and queue it immediately as one
   * BUS_MSG_DEBUG_TEXT message, even if no newline has been written.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - bool: true if a message was queued successfully
   */
  bool flush();

  /*
   * Clear the current in-progress line buffer without queueing it.
   *
   * Inputs:
   * - none
   *
   * Outputs:
   * - none
   */
  void clearLineBuffer();

  /* Return the number of characters currently stored in the local line buffer. */
  size_t getLineLength() const;

  /* Return true if the current line buffer is empty. */
  bool isLineEmpty() const;

  // ---------------------------------------------------------------------------
  // Diagnostics / Counters
  // ---------------------------------------------------------------------------

  /* Return the number of complete debug lines successfully queued so far. */
  uint32_t getLinesQueuedCount() const;

  /* Return the number of lines dropped because queueing failed. */
  uint32_t getLinesDroppedCount() const;

  /* Return whether the current/last line experienced truncation. */
  bool wasTruncated() const;

 private:
  // ---------------------------------------------------------------------------
  // Internal Helpers
  // ---------------------------------------------------------------------------

  /*
   * Append one character into the line buffer, handling newline/finalization.
   */
  size_t appendChar(char c);

  /*
   * Queue the current line buffer as one BUS_MSG_DEBUG_TEXT message.
   *
   * Inputs:
   * - add_newline_stripped: true if trailing newline should not be stored in the
   *   transmitted text payload
   *
   * Outputs:
   * - bool: true if queueing succeeded
   */
  bool queueCurrentLine(bool newline_stripped = true);

  /* Copy/append the configured prefix into an output staging buffer. */
  size_t appendPrefix(char* out, size_t out_size) const;

  /* Convert an integer to text and append it using the normal write path. */
  size_t printNumberUnsigned(unsigned long value);

  /* Convert a signed integer to text and append it using the normal write path. */
  size_t printNumberSigned(long value);

  // ---------------------------------------------------------------------------
  // First-Pass Capacity / Storage Assumptions
  // ---------------------------------------------------------------------------

  /*
   * The first implementation uses fixed-size local text buffers.
   *
   * Planned first-pass capacities:
   * - In-progress line buffer: 96 characters
   * - Optional prefix buffer:   24 characters
   *
   * The line buffer may be larger than the transmitted payload text because the
   * implementation may need temporary formatting/prefix space before producing a
   * final BUS_MSG_DEBUG_TEXT payload that fits into BUS_MAX_PAYLOAD_SIZE.
   */
  static constexpr size_t kLineBufferSize = 96;
  static constexpr size_t kPrefixBufferSize = 24;

  // ---------------------------------------------------------------------------
  // Internal State
  // ---------------------------------------------------------------------------

  /* Associated BusNode that owns the actual outbound bus queue. */
  BusNode* node_;

  /* Master enable switch for this debug wrapper. */
  bool enabled_;

  /* Default severity/class byte for future debug messages. */
  uint8_t level_;

  /* Current in-progress line buffer and its length. */
  char line_buffer_[kLineBufferSize];
  size_t line_length_;

  /* Optional local prefix string prepended to each queued debug line. */
  char prefix_[kPrefixBufferSize];

  /* Diagnostics/counters. */
  uint32_t lines_queued_count_;
  uint32_t lines_dropped_count_;
  bool truncated_;
};
