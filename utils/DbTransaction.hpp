#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <functional>
#include <memory>
#include <stdexcept>

struct CommitAwaiter : public drogon::CallbackAwaiter<bool> {
    explicit CommitAwaiter(std::shared_ptr<drogon::orm::Transaction> tx) : tx_(std::move(tx)) {}

    void await_suspend(std::coroutine_handle<> handle) {
        tx_->setCommitCallback([this, handle](bool committed) {
            setValue(committed);
            handle.resume();
        });
        tx_.reset();
    }

private:
    std::shared_ptr<drogon::orm::Transaction> tx_;
};

inline drogon::Task<> withTransaction(const drogon::orm::DbClientPtr& db,
                                      std::function<drogon::Task<>(drogon::orm::DbClientPtr tx)> work) {
    auto tx = co_await db->newTransactionCoro();
    try {
        co_await work(tx);
    } catch (...) {
        tx->rollback();
        throw;
    }
    if (!co_await CommitAwaiter(std::move(tx))) throw std::runtime_error("Transaction failed to commit");
}
