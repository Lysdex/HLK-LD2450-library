# HLK-LD2450 Arduino library

A small, non-blocking reader for the Hi-Link LD2450 UART tracking reports.
It reads up to three target slots, decodes the radar's sign/magnitude encoding,
and exposes position, speed, distance and presence helpers.

## Install in Arduino IDE

Download this repository using **Code → Download ZIP**, then select
**Sketch → Include Library → Add .ZIP Library**. Open
**File → Examples → HLK-LD2450-Stream → ESP32S3**.

This library uses `#include <HLKLD2450.h>` and the `HLKLD2450` class,
so it can coexist with Marcel Ochsendorf's `LD2450.h` library.
It is a separate library, not a drop-in replacement.

## ESP32-S3 wiring

| LD2450 | ESP32-S3 |
| --- | --- |
| 5V | 5V supply |
| GND | GND |
| TX | GPIO18 (RX) |
| RX | GPIO17 (TX) |

Use exposed, unused GPIOs on your particular board. The module requires 5V
power; its UART signals use 3.3V logic. If using a separate 5V supply,
connect its ground to the ESP32 ground. Do not connect 5V to an ESP32 GPIO.
Verify that your board's 5V pin supplies power when USB is connected.

For the S3's native USB connection, enable **USB CDC On Boot**. Open Serial
Monitor at **115200 baud**; the radar UART runs separately at **256000 baud**.

## Minimal use

```cpp
#include <Arduino.h>
#include <HLKLD2450.h>

HardwareSerial radarSerial(1);
HLKLD2450 radar;

void setup() {
  Serial.begin(115200);
  radarSerial.begin(256000, SERIAL_8N1, 18, 17); // RX, TX
  radar.begin(radarSerial);
}

void loop() {
  if (radar.read() > 0) {
    for (uint8_t i = 0; i < HLKLD2450::MAX_TARGETS; ++i) {
      const HLKLD2450::Target target = radar.getTarget(i);
      if (target.valid) {
        Serial.printf("Slot %u: X=%d, Y=%d mm, speed=%d cm/s\n",
                      unsigned(target.id), int(target.x),
                      int(target.y), int(target.speed));
      }
    }
  }
}
```

## Values and API

| Member | Meaning |
| --- | --- |
| `target.x`, `target.y` | Signed position in mm |
| `target.speed` | Signed radial speed in cm/s; positive away, negative approaching |
| `target.speedMmPerSecond()` | Same speed converted to mm/s using a 32-bit result |
| `target.distance` | Straight-line distance in mm, truncated to a whole mm |
| `target.resolution` | Reported distance resolution in mm |
| `target.valid` | Nonzero resolution marks a detected target |
| `target.isMoving()` | Valid target with nonzero reported radial speed |
| `radar.targetCount()` | Number of valid slots in the most recent report |
| `radar.hasTarget()` | At least one valid slot |
| `radar.hasMovingTarget()` | At least one valid slot with nonzero radial speed |
| `radar.hasStillTarget()` | At least one valid slot with zero radial speed |
| `radar.read()` | Number of complete reports parsed this call, not target count |
| `radar.frameCount()` | Complete reports since `begin()` or `reset()` |
| `radar.feed(byte)` | Feed one raw byte; returns true on a complete report |
| `radar.reset()` | Clear parser state, targets and frame count; keep UART attached |

`getTarget()` indexes slots from **0 to 2**; returned IDs are **1 to 3**.
IDs are tracking slots, not persistent identities. An invalid index returns an
empty target with ID 0. A zero-speed target can still be moving sideways;
the helper reports radial movement only.

Call `read()` frequently. It does not wait for bytes, allocate strings, or use
`delay()`. Partial frames survive between calls. Header/footer checks and a
sliding buffer allow recovery after noise, lost bytes or starting mid-frame.
The report protocol has no checksum, so framing validation cannot detect every
possible payload corruption.

Targets remain the latest snapshot until a new complete report arrives.
Use fresh-report timing before taking actions; the full example warns after
one second without reports. Missing UART data does not imply no presence.

## Decoding and scope

The LD2450 tracking protocol stores a 15-bit magnitude and a sign flag:
**bit 15 set means positive; clear means negative**. It must not be decoded as
ordinary two's complement. Distance is calculated after decoding X and Y.

The implementation is original code, informed by the
[Hi-Link protocol](https://h.hlktech.com/download/HLK-LD2450-24G/1/LD2450%20%E4%B8%B2%E5%8F%A3%E9%80%9A%E4%BF%A1%E5%8D%8F%E8%AE%AE%20V1.03.pdf)
and checked against
[ESPHome's decoder](https://github.com/esphome/esphome/blob/dev/esphome/components/ld2450/ld2450.cpp).
This is not the ESPHome component packaged for Arduino and does not depend on it.

Version 0.1.0 reads tracking reports only. Configuration commands, Bluetooth,
zone configuration and firmware queries are not implemented. The core accepts
any Arduino `Stream`; the supplied example is specifically for ESP32-S3.

## Validation

Host tests exercise the actual library parser with captured-format byte fixtures:
signed coordinates/speeds, three slots, empty frames, every frame split point,
bytewise input, consecutive reports, malformed/truncated reports and recovery.
Fixtures based on the reported failure pattern verify that an encoded +1007 mm
Y value is decoded as +1007, not -31761 mm.

Run with a C++11 compiler:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic -Itests/support -Isrc src/HLKLD2450.cpp tests/parser_test.cpp -o parser_test
./parser_test
```

Host parser tests pass under C++11 with warnings treated as errors. An ESP32-S3
build was attempted with Arduino ESP32 core 3.3.12, but the installed cross-compiler
could not start: Windows reported `Illegal System DLL Relocation`. This is an
environment failure, not a successful board build. Compilation in Arduino IDE
and hardware behavior still need verification on an S3 connected to an LD2450.
