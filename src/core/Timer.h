//
// Created by Ivan Kishchenko on 28/07/2023.
//

#pragma once

#include <cstdint>
#include <functional>
#include <memory>

class Timer {
public:
    typedef std::shared_ptr<Timer> Ptr;

    virtual void attach(uint32_t milliseconds, bool repeat, const std::function<void()> &callback) = 0;

    virtual ~Timer() = default;
};

extern "C" {
#include "esp_timer.h"
}

class EspTimer : public Timer {
    std::function<void()> _callback;

    esp_timer_handle_t _timer{};
private:
    static void onCallback(void *arg) {
        auto *timer = static_cast<EspTimer *>(arg);
        timer->doCallback();
    }

    void doCallback() {
        _callback();
    }

public:
    void attach(uint32_t milliseconds, bool repeat, const std::function<void()> &callback) override {
        _callback = callback;

        esp_timer_create_args_t _timerConfig;
        _timerConfig.arg = reinterpret_cast<void *>(this);
        _timerConfig.callback = onCallback;
        _timerConfig.dispatch_method = ESP_TIMER_TASK;
        _timerConfig.name = "Ticker";
        if (_timer) {
            esp_timer_stop(_timer);
            esp_timer_delete(_timer);
        }
        esp_timer_create(&_timerConfig, &_timer);
        if (repeat) {
            esp_timer_start_periodic(_timer, milliseconds * 1000ULL);
        } else {
            esp_timer_start_once(_timer, milliseconds * 1000ULL);
        }
    }

    void detach() {
        if (_timer) {
            esp_timer_stop(_timer);
            esp_timer_delete(_timer);
            _timer = nullptr;
        }
    }

    virtual ~EspTimer() {
        detach();
    }
};

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

class SoftwareTimer : public Timer {
    TimerHandle_t _timer{};

    std::function<void()> _callback;
private:
    static void onCallback(TimerHandle_t timer) {
        auto self = static_cast<SoftwareTimer *>( pvTimerGetTimerID(timer));
        self->doCallback();
    }

    void doCallback() {
        _callback();
    }

public:
    SoftwareTimer() = default;

    void attach(uint32_t milliseconds, bool repeat, const std::function<void()> &callback) override;

    ~SoftwareTimer() override;
};
