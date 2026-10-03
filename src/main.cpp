#include <Arduino.h>
#include <cstring>
#include "serial_log.h"
#include "sensor_web.h"

#ifndef LD2450_RX_PIN
#define LD2450_RX_PIN 4
#endif
#ifndef LD2450_TX_PIN
#define LD2450_TX_PIN 5
#endif

namespace {
HardwareSerial radar(2);
constexpr size_t frameSize = 30;
uint8_t frame[frameSize] = {};
size_t buffered = 0;
uint32_t receivedBytes = 0;
uint32_t receivedFrames = 0;
uint32_t lastFrameAt = 0;
uint32_t lastReportAt = 0;

struct Target {
    int16_t x = 0;
    int16_t y = 0;
    int16_t speed = 0;
    uint16_t resolution = 0;
    bool present = false;
};
Target targets[3];

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

int16_t readSigned(const uint8_t* data) {
    const uint16_t raw = readU16(data);
    const int16_t magnitude = raw & 0x7FFF;
    // LD2450은 최상위 비트가 1이면 양수, 0이면 음수임.
    return (raw & 0x8000) ? magnitude : -magnitude;
}

void consumeByte(uint8_t value) {
    ++receivedBytes;
    frame[buffered++] = value;
    if (buffered < frameSize) return;

    if (frame[0] == 0xAA && frame[1] == 0xFF &&
        frame[2] == 0x03 && frame[3] == 0x00 &&
        frame[28] == 0x55 && frame[29] == 0xCC) {
        for (size_t i = 0; i < 3; ++i) {
            const uint8_t* data = frame + 4 + i * 8;
            Target& target = targets[i];
            target.x = readSigned(data);
            target.y = readSigned(data + 2);
            target.speed = readSigned(data + 4);
            target.resolution = readU16(data + 6);
            target.present = false;
            for (size_t j = 0; j < 8; ++j) {
                target.present |= data[j] != 0;
            }
        }
        ++receivedFrames;
        lastFrameAt = millis();
        SensorTarget snapshot[3];
        for (size_t i = 0; i < 3; ++i) {
            snapshot[i] = {targets[i].x, targets[i].y, targets[i].speed,
                           targets[i].resolution, targets[i].present};
        }
        webSensorFrame(snapshot, receivedBytes, receivedFrames);
        buffered = 0;
    } else {
        // 잡음이나 중간 프레임부터 수신해도 한 바이트씩 이동하며 재동기화함.
        memmove(frame, frame + 1, frameSize - 1);
        buffered = frameSize - 1;
    }
}
}

void setup() {
    Serial.begin(115200);
    radar.setRxBufferSize(1024);
    radar.begin(256000, SERIAL_8N1, LD2450_RX_PIN, LD2450_TX_PIN);
    serialLog::print(cli::fg::BRIGHT_CYAN, "BOOT", "LD2450 테스트 시작함: RX=%d TX=%d UART=256000 8N1",
                  LD2450_RX_PIN, LD2450_TX_PIN);
    webBegin();
}

void loop() {
    while (radar.available() > 0) {
        consumeByte(static_cast<uint8_t>(radar.read()));
    }
    webSensorBytes(receivedBytes);
    webTick();

    const uint32_t now = millis();
    if (now - lastReportAt >= 500) {
        lastReportAt = now;
        serialLog::print(cli::fg::CYAN, "RADAR", "수신 bytes=%lu frames=%lu",
                      static_cast<unsigned long>(receivedBytes),
                      static_cast<unsigned long>(receivedFrames));
        if (receivedFrames == 0 || now - lastFrameAt > 2000) {
            serialLog::print(cli::fg::YELLOW, "WARN", "유효 프레임 없음: 전원·배선·baud 확인 필요함");
        } else {
            for (size_t i = 0; i < 3; ++i) {
                const Target& target = targets[i];
                if (!target.present) {
                    serialLog::print(cli::fg::BRIGHT_BLACK, "TARGET", "타깃 %u: 없음", static_cast<unsigned>(i + 1));
                    continue;
                }
                const std::string_view colors[] = {cli::fg::GREEN, cli::fg::BLUE, cli::fg::MAGENTA};
                serialLog::print(colors[i], "TARGET", "타깃 %u: x=%d mm y=%d mm speed=%d cm/s resolution=%u mm",
                              static_cast<unsigned>(i + 1), target.x, target.y,
                              target.speed, target.resolution);
            }
        }
    }
    delay(1);
}
