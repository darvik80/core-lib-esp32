//
// Created by Ivan Kishchenko on 08/08/2023.
//

#include "Logger.h"
#include <LittleFS.h>
#include "Properties.h"

[[maybe_unused]] void fromJson(cJSON *json, WifiProperties &props) {
    cJSON *item = json->child;
    while (item) {
        if (!strcmp(item->string, "ssid") && item->type == cJSON_String) {
            props.ssid = item->valuestring;
        } else if (!strcmp(item->string, "password") && item->type == cJSON_String) {
            props.password = item->valuestring;
        }
        item = item->next;
    }
}

void fromJson(cJSON *json, MqttProperties &props) {
    cJSON *item = json->child;
    while (item) {
        if (!strcmp(item->string, "uri") && item->type == cJSON_String) {
            props.uri = item->valuestring;
        } else if (!strcmp(item->string, "username") && item->type == cJSON_String) {
            props.username = item->valuestring;
        } else if (!strcmp(item->string, "password") && item->type == cJSON_String) {
            props.password = item->valuestring;
        } else if (!strcmp(item->string, "ca-cert-file") && item->type == cJSON_String) {
            props.caCertFile = item->valuestring;
        } else if (!strcmp(item->string, "client-cert-file") && item->type == cJSON_String) {
            props.clientCertFile = item->valuestring;
        } else if (!strcmp(item->string, "client-key-file") && item->type == cJSON_String) {
            props.clientKeyFile = item->valuestring;
        } else if (!strcmp(item->string, "device-name") && item->type == cJSON_String) {
            props.deviceName = item->valuestring;
        } else if (!strcmp(item->string, "product-name") && item->type == cJSON_String) {
            props.productName = item->valuestring;
        }
        item = item->next;
    }
}

void PropertiesLoader::load(std::string_view filePath) {
    if (File file = LittleFS.open(filePath.data()); file) {
        String cfg;
        while (file.available()) {
            cfg += file.readString();
        }
        file.close();

        cJSON *json = cJSON_Parse(cfg.c_str());
        cJSON *item = json->child;
        while (item) {
            esp_logi(props, "read props: %s", item->string);
            if (item->type == cJSON_Object) {
                if (
                    auto it = _readers.find(item->string);it != _readers.end()) {
                    auto props = it->second(item);
                    for (
                        auto consumer : _consumers) {
                        consumer->applyProperties(*props);
                    }
                }
            }
            item = item->next;
        }
        cJSON_Delete(json);
    }
}