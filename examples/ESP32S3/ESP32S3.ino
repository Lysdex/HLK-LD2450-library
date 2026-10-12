#include <Arduino.h>
#include <HLKLD2450.h>

HardwareSerial radarSerial(1);
HLKLD2450 radar;

const int RADAR_RX = 18;  // LD2450 TX -> ESP32-S3 GPIO18
const int RADAR_TX = 17;  // LD2450 RX -> ESP32-S3 GPIO17
uint32_t lastReport = 0;
uint32_t lastPrint = 0;
bool receivedReport = false;
bool staleWarningPrinted = false;

void setup() {
  Serial.begin(115200);
  radarSerial.begin(HLKLD2450::DEFAULT_BAUD, SERIAL_8N1, RADAR_RX, RADAR_TX);
  radar.begin(radarSerial);
  Serial.println("LD2450 test: waiting for radar reports.");
}

void loop() {
  const uint32_t now = millis();
  const uint32_t reports = radar.read();
  if (reports > 0) {
    lastReport = now;
    receivedReport = true;
    staleWarningPrinted = false;
  }

  // Loss of UART data is not the same as an empty room.
  if ((!receivedReport && now >= 1000) ||
      (receivedReport && uint32_t(now - lastReport) >= 1000)) {
    if (!staleWarningPrinted) {
      Serial.println("No recent radar reports: check power, wiring and baud rate.");
      staleWarningPrinted = true;
    }
    return;
  }
  if (reports == 0 || uint32_t(now - lastPrint) < 500) return;
  lastPrint = now;

  for (uint8_t i = 0; i < HLKLD2450::MAX_TARGETS; ++i) {
    const HLKLD2450::Target target = radar.getTarget(i);
    if (!target.valid) continue;
    Serial.printf("Target %u: X=%d mm, Y=%d mm, distance=%u mm, speed=%d cm/s\n",
                  unsigned(target.id), int(target.x), int(target.y),
                  unsigned(target.distance), int(target.speed));
  }
  Serial.printf("Targets=%u | Presence=%s | Movement=%s | Still=%s\n\n",
                unsigned(radar.targetCount()), radar.hasTarget() ? "YES" : "NO",
                radar.hasMovingTarget() ? "YES" : "NO",
                radar.hasStillTarget() ? "YES" : "NO");
  // Set an LED once here, after combining every target's state.
}
