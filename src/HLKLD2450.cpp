#include "HLKLD2450.h"

#include <math.h>
#include <string.h>

void HLKLD2450::begin(Stream &uart) {
  uart_ = &uart;
  reset();
}

void HLKLD2450::reset() {
  buffered_ = 0;
  frame_count_ = 0;
  for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
    targets_[i] = Target();
    targets_[i].id = i + 1;
  }
}

uint32_t HLKLD2450::read() {
  if (uart_ == nullptr) return 0;
  uint32_t reports = 0;
  // Snapshot the available count so this call cannot chase a continuous stream.
  int remaining = uart_->available();
  while (remaining-- > 0) {
    const int value = uart_->read();
    if (value < 0) break;
    if (feed(static_cast<uint8_t>(value))) ++reports;
  }
  return reports;
}

bool HLKLD2450::feed(uint8_t value) {
  if (buffered_ == FRAME_SIZE) {
    // Slide one byte after a bad frame, preserving possible overlapping headers.
    memmove(buffer_, buffer_ + 1, FRAME_SIZE - 1);
    --buffered_;
  }
  buffer_[buffered_++] = value;
  if (buffered_ != FRAME_SIZE) return false;
  if (buffer_[0] != 0xAA || buffer_[1] != 0xFF ||
      buffer_[2] != 0x03 || buffer_[3] != 0x00 ||
      buffer_[28] != 0x55 || buffer_[29] != 0xCC) return false;

  decodeFrame();
  ++frame_count_;
  buffered_ = 0;
  return true;
}

int16_t HLKLD2450::decodeSigned(uint8_t low, uint8_t high) {
  // Protocol bit 15 selects positive when set; this is NOT two's complement.
  const uint16_t packed = uint16_t(low) | (uint16_t(high) << 8);
  const int16_t magnitude = static_cast<int16_t>(packed & 0x7FFF);
  return (packed & 0x8000) != 0 ? magnitude : -magnitude;
}

void HLKLD2450::decodeFrame() {
  for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
    const uint8_t offset = 4 + i * 8;
    Target target;
    target.id = i + 1;
    target.x = decodeSigned(buffer_[offset], buffer_[offset + 1]);
    target.y = decodeSigned(buffer_[offset + 2], buffer_[offset + 3]);
    target.speed = decodeSigned(buffer_[offset + 4], buffer_[offset + 5]);
    target.resolution = uint16_t(buffer_[offset + 6]) |
                        (uint16_t(buffer_[offset + 7]) << 8);
    target.valid = target.resolution != 0;
    const float x = target.x;
    const float y = target.y;
    target.distance = static_cast<uint16_t>(sqrtf(x * x + y * y));
    targets_[i] = target;
  }
}

HLKLD2450::Target HLKLD2450::getTarget(uint8_t index) const {
  return index < MAX_TARGETS ? targets_[index] : Target();
}

uint8_t HLKLD2450::targetCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
    if (targets_[i].valid) ++count;
  }
  return count;
}

bool HLKLD2450::hasMovingTarget() const {
  for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
    if (targets_[i].isMoving()) return true;
  }
  return false;
}

bool HLKLD2450::hasStillTarget() const {
  for (uint8_t i = 0; i < MAX_TARGETS; ++i) {
    if (targets_[i].valid && targets_[i].speed == 0) return true;
  }
  return false;
}
