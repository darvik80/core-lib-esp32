//
// Created by Ivan Kishchenko on 09/08/2023.
//

#include "Timer.h"

void SoftwareTimer::attach(uint32_t milliseconds, bool repeat, const std::function<void()> &callback) {
    _callback = callback;
    _timer = xTimerCreate(
            "timer",
            pdMS_TO_TICKS(milliseconds),
            repeat,
            this,
            SoftwareTimer::onCallback
    );

    xTimerStart(_timer, 0);
}

SoftwareTimer::~SoftwareTimer() {
    xTimerStop(_timer, 0);
    xTimerDelete(_timer, 0);
}