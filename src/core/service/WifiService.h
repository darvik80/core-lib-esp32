//
// Created by Ivan Kishchenko on 07/08/2023.
//

#pragma once

#include <WiFi.h>
#include "SysService.h"
#include "core/Registry.h"
#include "core/Properties.h"

class WifiService : public TService<Sys_Wifi_Service, System::Sys_Core>, public TPropertiesConsumer<WifiService, WifiProperties> {
public:
    explicit WifiService(Registry &registry);

    void applyProperties(const WifiProperties &props);
};
