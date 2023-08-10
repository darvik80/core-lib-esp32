//
// Created by Ivan Kishchenko on 07.11.2021.
//

#pragma once

#include <utility>
#include <LittleFS.h>

#include "core/MessageBus.h"
#include "Properties.h"

typedef uint16_t ServiceId;
typedef uint_least8_t ServiceSubId;

class Registry;

class Service {
public:
    [[nodiscard]] virtual ServiceId getServiceId() const = 0;

    virtual Registry &getRegistry() = 0;

    virtual void setup() = 0;

    virtual void loop() = 0;

    virtual ~Service() = default;
};

typedef std::vector<Service *> ServiceArray;

class Registry {
public:
    virtual void addService(Service *service) = 0;

    virtual ServiceArray &getServices() = 0;

    template<typename C, typename... T>
    C &create(T &&... all) {
        auto *service = new C(*this, std::forward<T>(all)...);
        addService(service);
        return *service;
    }

    template<typename C>
    C *getService(ServiceId id) {
        if (id) {
            for (auto service: getServices()) {
                if (service->getServiceId() == id) {
                    return static_cast<C*>(service);
                }
            }
        }

        return nullptr;
    }

    virtual MessageBus &getMessageBus() = 0;

    virtual PropertiesLoader &getPropsLoader() = 0;
};

template<typename MsgBus>
class TRegistry : public Registry {
    ServiceArray _services;
    MsgBus _bus;

    PropertiesLoader _propsLoader;
public:
    void addService(Service *service) override {
        _services.push_back(service);
    }

    ServiceArray &getServices() override {
        return _services;
    }

    MessageBus &getMessageBus() override {
        return _bus;
    }

    PropertiesLoader &getPropsLoader() override {
        return _propsLoader;
    }

    ~TRegistry() {
        for (auto *service: _services) {
            delete service;
        }
    }
};

template<ServiceSubId Id, System systemId = System::Sys_User>
class TService : public Service {
    Registry &_registry;
public:
    explicit TService(Registry &registry) : _registry(registry) {}

    enum {
        ID = Id || (uint16_t) systemId << 8
    };

    [[nodiscard]] ServiceId getServiceId() const override {
        return Id || (uint16_t) systemId << 8;
    }

    Registry &getRegistry() override {
        return _registry;
    }

    void setup() override {}

    void loop() override {}
};

class Application {
    TRegistry<TMessageBus<10>> _registry;
public:
    Registry &getRegistry() {
        return _registry;
    }

    virtual void setup() {
        LittleFS.begin(false);

        onSetup();
        std::sort(getRegistry().getServices().begin(), getRegistry().getServices().end(), [](auto f, auto s) {
            return f->getServiceId() < s->getServiceId();
        });

        esp_logd(app, "Load config");
        getRegistry().getPropsLoader().load("/config.json");
        for (auto service: getRegistry().getServices()) {
            if (service) {
                service->setup();
                esp_logd(app, "Setup: %d", service->getServiceId());
            }
        }
    }

    virtual void onSetup() {}

    virtual void loop() {
        getRegistry().getMessageBus().loop();
    }
};
