#include "timeout_timer.h"

#include <Arduino.h>

TimeoutTimer::TimeoutTimer(const uint32_t ms) : interval_ms(ms) {
    start_ms = millis();
}

bool TimeoutTimer::hasTimedOut() const {
    return millis() - start_ms > interval_ms;
}

void TimeoutTimer::reset() { start_ms = millis(); }