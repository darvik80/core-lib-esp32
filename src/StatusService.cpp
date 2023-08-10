//
// Created by Ivan Kishchenko on 10/08/2023.
//

#include "StatusService.h"
#include <Arduino.h>

void toJson(const StatusMessage& msg, cJSON* json) {
    cJSON_AddStringToObject(json, "status", msg.status.c_str());
    cJSON_AddNumberToObject(json, "timestamp", msg.timestamp);
}

StatusService::StatusService(Registry &registry) : TService(registry) {}

void StatusService::setup() {
    _timer.attach(1000, true, [this]() {
        StatusMessage msg;
        msg.timestamp = millis();
        msg.status = "active";

        getRegistry().getMessageBus().postMessage(msg);
    });
}
