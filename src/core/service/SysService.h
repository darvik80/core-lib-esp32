//
// Created by Ivan Kishchenko on 07/08/2023.
//

#pragma once
#include "core/MessageBus.h"

enum SystemServiceId {
    Sys_Wifi_Service,
    Sys_Mqtt_Service,
};

enum SystemMessage {
    Sys_Wifi_Connected,
    Sys_Wifi_Disconnected,
    Sys_Mqtt_Connected,
    Sys_Mqtt_Disconnected,
    Sys_Mqtt_Message,
};

struct WifiConnected : TMessage<Sys_Wifi_Connected, System::Sys_Core> {
    std::string ip;
    std::string gw;
    std::string mask;
    std::string mac;
};

struct WifiDisconnected : TMessage<Sys_Wifi_Disconnected, System::Sys_Core> {
    uint8_t reason;
};

struct MqttConnected : TMessage<Sys_Mqtt_Connected, System::Sys_Core> {
};

struct MqttDisconnected : TMessage<Sys_Mqtt_Disconnected, System::Sys_Core> {
    int reason;
};

struct MqttMessage : TMessage<Sys_Mqtt_Message, System::Sys_Core> {
    std::string topic;
    std::string payload;
    int qos;
};