#pragma once
#include <Arduino.h>
#include "ansi_color.h"

namespace serialLog {
template <typename... Args>
void print(std::string_view color, const char* tag, const char* format, Args... args) {
    Serial.write(reinterpret_cast<const uint8_t*>(color.data()), color.size());
    Serial.printf("[%8lu ms] [%-5s] ", static_cast<unsigned long>(millis()), tag);
    Serial.printf(format, args...);
    Serial.write(reinterpret_cast<const uint8_t*>(cli::style::RESET.data()), cli::style::RESET.size());
    Serial.println();
}
}
