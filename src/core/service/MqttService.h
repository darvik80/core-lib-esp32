#pragma once

#include <string>
#include <unordered_map>

#include <mqtt_client.h>

#include "SysService.h"
#include "core/Registry.h"

class IotCredentials {
public:
    typedef std::unique_ptr<IotCredentials> Ptr;

    virtual const std::string &product() = 0;

    virtual const std::string &deviceName() = 0;

    virtual const std::string &username() = 0;

    virtual const std::string &password() = 0;

    virtual const std::string &clientId() = 0;

    virtual const std::string &uri() = 0;

    virtual const std::string &caCert() = 0;


    virtual const std::string &clientCert() = 0;

    virtual const std::string &clientKey() = 0;

    virtual ~IotCredentials() = default;
};


class RabbitMQSign : public IotCredentials {
    std::string _product;
    std::string _deviceName;
    std::string _username;
    std::string _password;
    std::string _clientId;
    std::string _uri;

    std::string _caCert;
    std::string _clientCert;
    std::string _clientKey;
public:
    RabbitMQSign(const MqttProperties &props);

    const std::string &product() override;

    const std::string &deviceName() override;

    const std::string &username() override;

    const std::string &password() override;

    const std::string &clientId() override;

    const std::string &uri() override;

    const std::string &caCert() override;

    const std::string &clientKey() override;

    const std::string &clientCert() override;
};

typedef std::function<void(std::string_view, std::string_view)> MqttDataCallback;


template<typename E>
void recvJsonMqttMsg(MessageBus &bus, std::string_view data) {
    auto json = cJSON_ParseWithLength(data.data(), data.length());
    if (json) {
        auto event = new E();
        fromJson(json, *event);
        cJSON_Delete(json);
        std::unique_ptr<Message> msg(event);
        bus.postMessage(msg);
    }
}

template<typename Msg>
void sendJsonMqttMsg(MessageBus &bus, std::string_view topic, const Msg& msg) {
    cJSON* json = cJSON_CreateObject();
    toJson(msg, json);
    const char* res = cJSON_Print(json);
    auto mqttMessage = new MqttMessage();
    mqttMessage->topic = topic;
    mqttMessage->payload = res;
    std::unique_ptr<Message> mqtt(mqttMessage);
    bus.postMessage(mqtt);
    cJSON_free((void*)res);
    cJSON_Delete(json);
}

class MqttService
        : public TService<Sys_Mqtt_Service, System::Sys_Core>,
          public TMessageSubscriber<MqttService, WifiConnected>,
          public TPropertiesConsumer<MqttService, MqttProperties> {
private:
    esp_mqtt_client_handle_t _client{};
    std::string _topicPrefix{};

    IotCredentials::Ptr _credentials{};

    std::unordered_map<std::string, std::string> _partitions;
    std::unordered_map<std::string, MqttDataCallback> _callbacks;
private:

    static void eventCallback(void *event_handler_arg, esp_event_base_t group, int32_t id, void *event_data) {
        ((MqttService *) event_handler_arg)->handleMqttEvent((esp_mqtt_event_handle_t) event_data);
    }

    void onConnect();
    void onMessage(std::string_view topic, std::string_view payload, size_t offset, size_t total);

    void handleMqttEvent(esp_mqtt_event_handle_t event);

public:
    explicit MqttService(Registry &registry);;

    void setup() override;

    void applyProperties(const MqttProperties &props);

    void onMessage(const WifiConnected &);

    void subscribe(std::string_view topic, int qos, const MqttDataCallback &callback);

    void publish(std::string_view topic, int qos, std::string_view payload);
};
