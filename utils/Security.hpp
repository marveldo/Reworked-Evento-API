#pragma once
#include <sodium.h>
#include <drogon/utils/coroutine.h>
#include <drogon/utils/Utilities.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <trantor/net/EventLoopThread.h>
#include <stdexcept>
#include <string>

namespace security {

inline constexpr size_t MEMLIMIT = 19 * 1024 * 1024;

inline trantor::EventLoopThread* hashThread = nullptr;

inline void init() {
    if (sodium_init() < 0) throw std::runtime_error("libsodium failed to initialise");
    if (!hashThread) {
        hashThread = new trantor::EventLoopThread("PasswordHash");
        hashThread->run();
    }
}

inline std::string hash(const std::string& plain) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, plain.c_str(), plain.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          MEMLIMIT) != 0)
        throw std::runtime_error("out of memory while hashing password");
    return out;
}

inline bool isDjangoHash(const std::string& stored) {
    return stored.rfind("pbkdf2_sha256$", 0) == 0;
}

inline bool verifyDjango(const std::string& plain, const std::string& stored) {
    const auto first  = stored.find('$');
    const auto second = stored.find('$', first + 1);
    const auto third  = stored.find('$', second + 1);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos) return false;

    int iterations = 0;
    try { iterations = std::stoi(stored.substr(first + 1, second - first - 1)); } catch (...) { return false; }
    if (iterations < 1) return false;
    const std::string salt     = stored.substr(second + 1, third - second - 1);
    const std::string expected = drogon::utils::base64Decode(stored.substr(third + 1));
    if (expected.empty()) return false;

    std::string derived(expected.size(), '\0');
    if (PKCS5_PBKDF2_HMAC(plain.data(), static_cast<int>(plain.size()),
                          reinterpret_cast<const unsigned char*>(salt.data()), static_cast<int>(salt.size()),
                          iterations, EVP_sha256(), static_cast<int>(derived.size()),
                          reinterpret_cast<unsigned char*>(derived.data())) != 1) return false;
    return CRYPTO_memcmp(derived.data(), expected.data(), expected.size()) == 0;
}

inline std::string unusablePassword() {
    return "!" + drogon::utils::getUuid() + drogon::utils::getUuid();
}

inline bool verify(const std::string& plain, const std::string& stored) {
    if (stored.empty() || stored[0] == '!') return false;
    if (isDjangoHash(stored)) return verifyDjango(plain, stored);
    return crypto_pwhash_str_verify(stored.c_str(), plain.c_str(), plain.size()) == 0;
}

inline bool needsRehash(const std::string& stored) {
    if (isDjangoHash(stored)) return true;
    return crypto_pwhash_str_needs_rehash(stored.c_str(),
                                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                          MEMLIMIT) != 0;
}

inline drogon::Task<std::string> hashAsync(std::string plain) {
    trantor::EventLoop* origin = trantor::EventLoop::getEventLoopOfCurrentThread();
    std::string result = co_await drogon::queueInLoopCoro<std::string>(hashThread->getLoop(), [plain]() { return hash(plain); });
    if (origin) co_await drogon::switchThreadCoro(origin);
    co_return result;
}

inline drogon::Task<bool> verifyAsync(std::string plain, std::string stored) {
    trantor::EventLoop* origin = trantor::EventLoop::getEventLoopOfCurrentThread();
    bool result = co_await drogon::queueInLoopCoro<bool>(hashThread->getLoop(), [plain, stored]() { return verify(plain, stored); });
    if (origin) co_await drogon::switchThreadCoro(origin);
    co_return result;
}

}
