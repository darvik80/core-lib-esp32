#include "MqttService.h"
#include <LittleFS.h>

esp_err_t readFile(std::string_view filePath, std::string &result) {
    if (auto file = LittleFS.open(filePath.data()); file) {
        result.clear();
        while (file.available()) {
            result.append(file.readString().c_str());
        }
        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

RabbitMQSign::RabbitMQSign(const MqttProperties &props)
        : _product(props.productName),
          _deviceName(props.deviceName),
          _clientId(props.deviceName),
          _username(props.username),
          _password(props.password),
          _uri(props.uri) {
    ESP_ERROR_CHECK(readFile(props.caCertFile, _caCert));
    ESP_ERROR_CHECK(readFile(props.clientCertFile, _clientCert));
    ESP_ERROR_CHECK(readFile(props.clientKeyFile, _clientKey));
}

const std::string &RabbitMQSign::product() {
    return _product;
}

const std::string &RabbitMQSign::deviceName() {
    return _deviceName;
}

const std::string &RabbitMQSign::username() {
    return _username;
}

const std::string &RabbitMQSign::password() {
    return _password;
}

const std::string &RabbitMQSign::clientId() {
    return _clientId;
}

const std::string &RabbitMQSign::uri() {
    return _uri;
}

const std::string &RabbitMQSign::caCert() {
    return _caCert;
}

const std::string &RabbitMQSign::clientCert() {
    return _clientCert;
}

const std::string &RabbitMQSign::clientKey() {
    return _clientKey;
}

MqttService::MqttService(Registry &registry) : TService(registry) {
    registry.getPropsLoader().addConsumer(this);
}

void MqttService::setup() {
    getRegistry().getMessageBus().subscribe(this);
}

void MqttService::applyProperties(const MqttProperties &props) {
    _credentials.reset(new RabbitMQSign(props));
    _topicPrefix = "/" + props.productName + "/" + props.deviceName;
}

void MqttService::onMessage(const WifiConnected &) {
    esp_logi(mqtt, "uri: %s", _credentials->uri().c_str());
    esp_logi(mqtt, "username: %s", _credentials->username().c_str());
    esp_logi(mqtt, "client-id: %s", _credentials->clientId().c_str());

    const esp_mqtt_client_config_t config{
            .uri = _credentials->uri().c_str(),
            .client_id = _credentials->clientId().c_str(),
            .username = _credentials->username().c_str(),
            .password = _credentials->password().c_str(),
            .keepalive = 60,
            .disable_auto_reconnect = false,
            .cert_pem = _credentials->caCert().c_str(),
            .client_cert_pem = _credentials->clientCert().c_str(),
            .client_key_pem = _credentials->clientKey().c_str(),
            .reconnect_timeout_ms = 1000,
            .skip_cert_common_name_check=true,
    };

    _client = esp_mqtt_client_init(&config);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_CONNECTED, eventCallback, this);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_DISCONNECTED, eventCallback, this);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_DATA, eventCallback, this);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_ERROR, eventCallback, this);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_SUBSCRIBED, eventCallback, this);
    esp_mqtt_client_register_event(_client, MQTT_EVENT_PUBLISHED, eventCallback, this);

    ESP_ERROR_CHECK(esp_mqtt_client_start(_client));
}

void MqttService::handleMqttEvent(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            onConnect();
            break;
            case MQTT_EVENT_SUBSCRIBED:
                esp_logd(mqtt, "SubTopic: msg-id: %d, qos: %d", event->msg_id, event->qos);
            break;
            case MQTT_EVENT_PUBLISHED:
                esp_logd(mqtt, "PubTopic: msg-id: %d, qos: %d", event->msg_id, event->qos);
            break;
            case MQTT_EVENT_DISCONNECTED:
                getRegistry().getMessageBus().postMessage(MqttDisconnected{});
            break;
            case MQTT_EVENT_ERROR: {
                esp_logw(mqtt, "Handled error");
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                    esp_logw(mqtt,
                             "esp-tls: 0x%x",
                             event->error_handle->esp_tls_last_esp_err
                    );
                    esp_logw(mqtt, "tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                    esp_logw(
                            mqtt,
                            "captured errno : %d (%s)",
                            event->error_handle->esp_transport_sock_errno,
                            strerror(event->error_handle->esp_transport_sock_errno)
                    );
                } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                    esp_logw(mqtt, "conn refused error: 0x%x", event->error_handle->connect_return_code);
                } else {
                    esp_logw(mqtt, "error type: 0x%x", event->error_handle->error_type);
                }
            }
            break;
            case MQTT_EVENT_DATA: {
                onMessage(
                        std::string_view(event->topic, event->topic_len),
                        std::string_view(event->data, event->data_len),
                        event->current_data_offset,
                        event->total_data_len
                );
            }
            break;
            default:
                break;

        }
    }
}

void MqttService::onConnect() {
    getRegistry().getMessageBus().postMessage(MqttConnected{});
    for (auto callback : _callbacks) {
        auto id = esp_mqtt_client_subscribe(_client, callback.first.data(), 0);
        if (id < 0) {
            esp_loge(mqtt, "Sub failed: %s", callback.first.data());
        } else {
            esp_logd(mqtt, "Sub topic: %s", callback.first.data());
        }
    }
};

void MqttService::onMessage(std::string_view topic, std::string_view payload, size_t offset, size_t total) {
    std::string topicName(topic.data(), topic.length());

    auto it = _partitions.end();
    if (total > payload.size()) {
        it = _partitions.find(topicName);
        if (it == _partitions.end()) {
            std::tie(it, std::ignore) = _partitions.emplace(topicName, payload);
        } else {
            it->second.append(payload);
        }
        if (payload.size() + offset < total) {
            return;
        }
    }

    auto itc = _callbacks.find(topicName);
    if (itc == _callbacks.end()) {
        esp_loge(mqtt, "No handle for topic: %s", topicName.data());
    } else {
        if (it != _partitions.end()) {
            itc->second(topicName, it->second);
            _partitions.erase(it);
        } else {
            itc->second(topicName, payload);
        }
    }
}

void MqttService::subscribe(std::string_view topic, int qos, const MqttDataCallback &callback) {
    auto topicPath = _topicPrefix;
    topicPath.append(topic);
    _callbacks.emplace(topicPath, callback);
//    auto id = esp_mqtt_client_subscribe(_client, topicPath.data(), qos);
//    if (id < 0) {
//        esp_loge(mqtt, "Sub failed: %s", topicPath.data());
//    } else {
//        esp_logd(mqtt, "Sub topic: %s", topicPath.c_str());
//    }
}

void MqttService::publish(std::string_view topic, int qos, std::string_view payload) {
    auto topicPath = _topicPrefix;
    topicPath.append(topic);

    esp_logd(mqtt, "Pub: topic: %s, payload: %d", topicPath.c_str(), payload.size());
    auto id = esp_mqtt_client_publish(_client, topicPath.data(), payload.data(), (int) payload.size(), 0, false);
    if (id < 0) {
        esp_logd(mqtt, "Pub failed: %s:%d", topicPath.data(), id);
    }
}
