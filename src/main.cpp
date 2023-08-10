#include "core/Logger.h"
#include <Arduino.h>

#include "core/Registry.h"
#include "core/service/WifiService.h"
#include "core/service/MqttService.h"
#include "StatusService.h"

struct MagicAction : TMessage<0> {
    uint8_t actionId{0};
};

void fromJson(cJSON *json, MagicAction &props) {
    cJSON *item = json->child;
    while (item) {
        if (!strcmp(item->string, "action-id") && item->type == cJSON_Number) {
            props.actionId = (uint8_t)item->valuedouble;
        }
        item = item->next;
    }
}

class App : public Application, public TMessageSubscriber<App, MagicAction> {
public:
    void onSetup() override {
        getRegistry().getPropsLoader().addReader("wifi", defaultPropertiesReader<WifiProperties>);
        getRegistry().getPropsLoader().addReader("mqtt", defaultPropertiesReader<MqttProperties>);

        getRegistry().getMessageBus().subscribe(this);
        getRegistry().create<WifiService>();

        auto& mqtt = getRegistry().create<MqttService>();
        mqtt.subscribe("/magic-action", 0, [&](std::string_view topic, std::string_view payload) {
            recvJsonMqttMsg<MagicAction>(getRegistry().getMessageBus(), payload);
        });
        getRegistry().getMessageBus().subscribe<StatusMessage>([this](const StatusMessage& msg) {
            sendJsonMqttMsg(getRegistry().getMessageBus(), "/magic-action-reply", msg);
        });
        getRegistry().create<StatusService>();
    }

    void onMessage(const MagicAction& msg) {
        esp_logi(app, "action: %d", msg.actionId);
    }
};

App app;

void setup() {
    Serial.begin(115200);
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    app.setup();
}

void loop() {
    app.loop();
}