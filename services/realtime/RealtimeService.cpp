#include "RealtimeService.hpp"
#include <trantor/utils/Logger.h>
#include <vector>

using drogon::nosql::RedisClient;

RealtimeService::RealtimeService(const std::string& host, int port, const std::string& password, const std::string& username, int db) {
    redis_ = RedisClient::newRedisClient(trantor::InetAddress(host, static_cast<uint16_t>(port)), 1, password,
                                         static_cast<unsigned int>(db), username);
    subscriber_ = redis_->newSubscriber();
}

drogon::Task<> RealtimeService::publish(std::string channel, std::string message) {
    co_await redis_->execCommandCoro("PUBLISH %s %s", channel.c_str(), message.c_str());
}

uint64_t RealtimeService::listen(const std::string& channel, Listener listener) {
    const uint64_t id = nextId_++;
    bool first = false;
    {
        std::lock_guard lock(mutex_);
        auto& listeners = listeners_[channel];
        first = listeners.empty();
        listeners.emplace(id, std::move(listener));
    }
    if (first) {
        subscriber_->subscribe(channel, [this](const std::string& ch, const std::string& message) { deliver(ch, message); });
    }
    return id;
}

void RealtimeService::stopListening(const std::string& channel, uint64_t id) {
    bool last = false;
    {
        std::lock_guard lock(mutex_);
        auto it = listeners_.find(channel);
        if (it == listeners_.end()) return;
        it->second.erase(id);
        if (it->second.empty()) {
            listeners_.erase(it);
            last = true;
        }
    }
    if (last) subscriber_->unsubscribe(channel);
}

void RealtimeService::deliver(const std::string& channel, const std::string& message) {
    std::vector<Listener> listeners;
    {
        std::lock_guard lock(mutex_);
        auto it = listeners_.find(channel);
        if (it == listeners_.end()) return;
        for (const auto& [_, listener] : it->second) listeners.push_back(listener);
    }
    for (const auto& listener : listeners) {
        try {
            listener(message);
        } catch (const std::exception& e) {
            LOG_ERROR << "RealtimeService: listener on " << channel << " failed: " << e.what();
        }
    }
}
