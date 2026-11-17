#pragma once
#include <drogon/nosql/RedisClient.h>
#include <drogon/nosql/RedisSubscriber.h>
#include <drogon/utils/coroutine.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

class RealtimeService {
public:
    using Listener = std::function<void(const std::string& message)>;

    RealtimeService(const std::string& host, int port, const std::string& password, const std::string& username = "", int db = 0);

    static std::string notificationsChannel(const std::string& userId) { return "notifications:" + userId; }
    static std::string logoutChannel(const std::string& deviceId)       { return "logout:" + deviceId; }

    drogon::Task<> publish(std::string channel, std::string message);

    uint64_t listen(const std::string& channel, Listener listener);
    void stopListening(const std::string& channel, uint64_t id);

private:
    void deliver(const std::string& channel, const std::string& message);

    drogon::nosql::RedisClientPtr redis_;
    std::shared_ptr<drogon::nosql::RedisSubscriber> subscriber_;

    std::mutex mutex_;
    std::unordered_map<std::string, std::unordered_map<uint64_t, Listener>> listeners_;
    std::atomic<uint64_t> nextId_{1};
};
