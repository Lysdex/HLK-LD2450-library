#pragma once

#include <Arduino.h>

class HLKLD2450 {
 public:
  static const uint8_t MAX_TARGETS = 3;
  static const uint32_t DEFAULT_BAUD = 256000;

  struct Target {
    uint8_t id = 0;            // Slot 1..3, not a persistent person identifier.
    int16_t x = 0;             // mm
    int16_t y = 0;             // mm
    int16_t speed = 0;         // cm/s: positive = away, negative = approaching.
    uint16_t resolution = 0;  // mm
    uint16_t distance = 0;    // mm, calculated from decoded X/Y.
    bool valid = false;
    int32_t speedMmPerSecond() const { return int32_t(speed) * 10; }
    bool isMoving() const { return valid && speed != 0; }
  };

  // Configure the UART yourself before begin(); this does not change its pins.
  void begin(Stream &uart);
  // Read currently available bytes. Returns complete report count, NOT target count.
  // No report is 0; partial reports are retained for the next call.
  uint32_t read();
  // Feed bytes directly when using another transport or testing captured data.
  bool feed(uint8_t value);
  Target getTarget(uint8_t index) const;  // Zero-based; invalid index gives empty target.
  uint8_t targetCount() const;
  bool hasTarget() const { return targetCount() != 0; }
  bool hasMovingTarget() const;
  bool hasStillTarget() const;
  uint32_t frameCount() const { return frame_count_; }
  void reset();

 private:
  static const uint8_t FRAME_SIZE = 30;
  static int16_t decodeSigned(uint8_t low, uint8_t high);
  void decodeFrame();
  Stream *uart_ = nullptr;
  uint8_t buffer_[FRAME_SIZE] = {};
  uint8_t buffered_ = 0;
  Target targets_[MAX_TARGETS] = {};
  uint32_t frame_count_ = 0;
};
