//
// Created by Ivan Kishchenko on 07.11.2021.
//

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <set>
#include <vector>
#include <string>
#include <memory>

#include "Logger.h"
#include "Timer.h"

typedef uint16_t MsgId;

typedef uint8_t SubMsgId;

enum class System {
    Sys_Bus,
    Sys_Core,
    Sys_User,
};

struct Message {
public:
    typedef std::unique_ptr<Message> Ptr;

    Message() = default;

    [[nodiscard]] virtual MsgId getMsgId() const = 0;

    virtual ~Message() = default;
};

template<SubMsgId subMsgId, System systemId = System::Sys_User>
struct TMessage : Message {
public:
    enum {
        ID = subMsgId | ((uint16_t)systemId << 8)
    };

    [[nodiscard]] MsgId getMsgId() const override {
        return ID;
    }
};

template<typename C, typename... T>
std::unique_ptr<Message> makeMsg(T &&... all) {
    auto *ptr = new C{std::forward<T>(all)...};
    return std::unique_ptr<C>(ptr);
}

class MessageProducer {
public:
    virtual void sendMessage(const Message &msg) = 0;
    virtual void postMessage(Message::Ptr &msg) = 0;
    template<typename T>
    void postMessage(const T& msg) {
        std::unique_ptr<Message> ptr(new T(msg));
        postMessage(ptr);
    }

    virtual void postMessageISR(Message::Ptr &msg) = 0;
    template<typename T>
    void scheduleMessage(uint32_t delay, T& msg) {
        std::unique_ptr<Message> ptr(new T(msg));
        scheduleMessage(delay, ptr);
    }
    virtual void scheduleMessage(uint32_t delay, Message::Ptr &msg) = 0;
    virtual bool schedule(uint32_t delay, bool repeat, const std::function<void()>& callback) = 0;
};

class MessageSubscriber {
public:
    virtual void onMessage(const Message &msg) = 0;
};

template<typename T, typename Msg1 = void, typename Msg2 = void, typename Msg3 = void, typename Msg4 = void>
class TMessageSubscriber : public MessageSubscriber {
public:
    void onMessage(const Message &msg) override {
        switch (msg.getMsgId()) {
            case Msg1::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg1 &>(msg));
                break;
            case Msg2::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg2 &>(msg));
                break;
            case Msg3::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg3 &>(msg));
                break;
            case Msg4::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg4 &>(msg));
                break;
            default:
                break;
        }
    }
};

template<typename T, typename Msg1, typename Msg2, typename Msg3>
class TMessageSubscriber<T, Msg1, Msg2, Msg3, void> : public MessageSubscriber {
public:
    void onMessage(const Message &msg) override {
        switch (msg.getMsgId()) {
            case Msg1::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg1 &>(msg));
                break;
            case Msg2::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg2 &>(msg));
                break;
            case Msg3::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg3 &>(msg));
                break;
            default:
                break;
        }
    }
};

template<typename T, typename Msg1, typename Msg2>
class TMessageSubscriber<T, Msg1, Msg2, void, void> : public MessageSubscriber {
public:
    void onMessage(const Message &msg) override {
        switch (msg.getMsgId()) {
            case Msg1::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg1 &>(msg));
                break;
            case Msg2::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg2 &>(msg));
                break;
            default:
                break;
        }
    }
};

template<typename T, typename Msg1>
class TMessageSubscriber<T, Msg1, void, void, void> : public MessageSubscriber {
public:
    void onMessage(const Message &msg) override {
        switch (msg.getMsgId()) {
            case Msg1::ID:
                static_cast<T *>(this)->onMessage(static_cast<const Msg1 &>(msg));
                break;
            default:
                break;
        }
    }
};

template<typename Msg>
class TMessageFuncSubscriber : public  MessageSubscriber {
    std::function<void(const Msg& msg)> _callback;
public:
    explicit TMessageFuncSubscriber(const std::function<void(const Msg& msg)>& callback) : _callback(callback) {}

    void onMessage(const Message &msg) override {
        if (msg.getMsgId() == Msg::ID) {
            _callback(static_cast<const Msg &>(msg));
        }
    }
};

class MessageBus : public MessageSubscriber, public MessageProducer {
public:
    virtual void subscribe(MessageSubscriber *subscriber) = 0;

    template<typename T>
    void subscribe(const std::function<void(const T& msg)> callback) {
        subscribe(new TMessageFuncSubscriber<T>(callback));
    }

    virtual void loop() = 0;
};

template<size_t queueSize = 10>
class TMessageBus : public MessageBus {
    QueueHandle_t _queue;

    std::vector<MessageSubscriber *> _subscribers;

    struct TimerBusMessage : TMessage<0, System::Sys_Bus> {
        std::function<void()> callback;
    };

public:
    TMessageBus() {
        _queue = xQueueCreate(queueSize, sizeof(void *));
        _subscribers.emplace_back(new TMessageFuncSubscriber<TimerBusMessage>([this](const TimerBusMessage& msg) {
            msg.callback();
        }));
    }

    void subscribe(MessageSubscriber *subscriber) override {
        _subscribers.emplace_back(subscriber);
    }

    void onMessage(const Message &msg) override {
        for (const auto sub: _subscribers) {
            sub->onMessage(msg);
        }
    }

    void loop() override {
        if (_queue) {
            Message *msg = nullptr;
            while (pdPASS == xQueueReceive(_queue, &msg, 100)) {
                sendMessage(*msg);
                delete msg;
            }
        }
    }

    void sendMessage(const Message &msg) override {
        onMessage(msg);
    }

    void postMessage(Message::Ptr &msg) override {
        if (_queue) {
            auto *ptr = msg.release();
            xQueueSendToBack(_queue, &ptr, portMAX_DELAY);
        }
    }

    void postMessageISR(Message::Ptr &msg) override {
        if (_queue) {
            auto *ptr = msg.release();
            xQueueSendFromISR(_queue, &ptr, nullptr);
        }
    }

    bool schedule(uint32_t delay, bool repeat, const std::function<void()> &callback) override {
        Timer *timer = new SoftwareTimer();
        timer->attach(delay, repeat, [this, repeat, callback, timer]() {
            auto timerMsg = new TimerBusMessage();
            timerMsg->callback = callback;
            std::unique_ptr<Message> ptr(timerMsg);
            postMessage(ptr);
            if (!repeat) {
                delete timer;
            }
        });
        return true;
    }

    void scheduleMessage(uint32_t delay, Message::Ptr &msg) override {
        Timer *timer = new SoftwareTimer();
        Message *ptr = msg.release();
        timer->attach(delay, false, [this, ptr, timer]() {
            std::unique_ptr<Message> holder(ptr);
            postMessage(holder);
            delete timer;
        });
    }

    virtual ~TMessageBus() {
        vQueueDelete(_queue);
    }
};

namespace bus {
    inline static void sendMessage(MessageProducer &pub, const Message &&msg) {
        pub.sendMessage(msg);
    }

    inline static bool schedule(MessageProducer &pub, uint32_t delay, bool repeat, const std::function<void()> &callback) {
        return pub.schedule(delay, repeat, callback);
    }

    inline static bool schedule(MessageProducer *pub, uint32_t delay, bool repeat, const std::function<void()> &callback) {
        return schedule(*pub, delay, repeat, callback);
    }

    inline static void postMessage(MessageProducer &pub, Message::Ptr &&msg) {
        pub.postMessage(msg);
    }
}