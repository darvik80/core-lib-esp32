//
// Created by Ivan Kishchenko on 10/08/2023.
//

#pragma once

#include "core/Registry.h"
#include "UserService.h"
#include "cJSON.h"


struct StatusMessage : public TMessage<Usr_Status> {
    std::string status;
    uint32_t timestamp;
};

void toJson(const StatusMessage& msg, cJSON* json);

class StatusService : public TService<Usr_Status_Service> {
    SoftwareTimer _timer;
public:
    explicit StatusService(Registry &registry);

    void setup() override;
};
