#ifndef TIMEOUT_TIMER_H
#define TIMEOUT_TIMER_H

#include <stdint.h>

class TimeoutTimer {

  private:
    const uint32_t interval_ms;
    uint32_t start_ms;

  public:
    TimeoutTimer(const uint32_t interval_ms);

    bool hasTimedOut() const;

    void reset();
};

#endif
