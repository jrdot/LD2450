#pragma once
#include <Arduino.h>

struct SensorTarget {
    int16_t x;
    int16_t y;
    int16_t speed;
    uint16_t resolution;
    bool present;
};

void webBegin();
void webTick();
void webSensorFrame(const SensorTarget* targets, uint32_t bytes, uint32_t frames);
void webSensorBytes(uint32_t bytes);
