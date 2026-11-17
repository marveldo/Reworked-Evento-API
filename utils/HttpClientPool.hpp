#pragma once
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <trantor/net/EventLoop.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class HttpClientPool {
public:
    explicit HttpClientPool(std::string baseUrl, size_t connectionsPerLoop = 4)
        : baseUrl_(std::move(baseUrl)), connectionsPerLoop_(std::max<size_t>(1, connectionsPerLoop)) {}

    drogon::HttpClientPtr get() {
        trantor::EventLoop* loop = trantor::EventLoop::getEventLoopOfCurrentThread();
        if (!loop) loop = drogon::app().getLoop();

        std::lock_guard lock(mutex_);
        LoopClients& entry = loops_[loop];
        if (entry.clients.size() < connectionsPerLoop_) {
            entry.clients.push_back(drogon::HttpClient::newHttpClient(baseUrl_, loop));
            return entry.clients.back();
        }
        return entry.clients[entry.next++ % entry.clients.size()];
    }

private:
    struct LoopClients {
        std::vector<drogon::HttpClientPtr> clients;
        size_t next = 0;
    };

    std::string baseUrl_;
    size_t connectionsPerLoop_;
    std::mutex mutex_;
    std::unordered_map<trantor::EventLoop*, LoopClients> loops_;
};
