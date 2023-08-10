//
// Created by Ivan Kishchenko on 07/08/2023.
//

#include "WifiService.h"

WifiService::WifiService(Registry &registry) : TService(registry) {
    registry.getPropsLoader().addConsumer(this);
}

void WifiService::applyProperties(const WifiProperties &props) {
    esp_logi(wifi, "SSID: %s", props.ssid.c_str());
    WiFi.mode(WIFI_AP);
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
                WifiConnected msg;
                msg.ip = WiFi.localIP().toString().c_str();
                msg.mask = WiFi.subnetMask().toString().c_str();
                msg.gw = WiFi.gatewayIP().toString().c_str();
                msg.mac = WiFi.macAddress().c_str();

                esp_logi(wifi, "Connected");
                esp_logi(wifi, "IP address: %s/%s", msg.ip.c_str(), msg.mask.c_str());
                esp_logi(wifi, "GW address: %s", msg.gw.c_str());
                esp_logi(wifi, "MAC address: %s", msg.mac.c_str());

                getRegistry().getMessageBus().postMessage(msg);
            }
                break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
                esp_logi(wifi, "Lost connection");
                WifiDisconnected msg;
                msg.reason = info.wifi_sta_disconnected.reason;
                getRegistry().getMessageBus().postMessage(msg);
            }
                break;
            default:
                break;
        }
    });
    WiFi.setAutoReconnect(true);
    WiFi.begin(props.ssid.c_str(), props.password.c_str());
}
