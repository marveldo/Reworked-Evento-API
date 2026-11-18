#include "Settings.h"
#include "../utils/dotenv.hpp"
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <cctype>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace {
    std::unique_ptr<Settings> g_settings;

    int toInt(const std::string& key, const std::string& fallback) {
        const std::string raw = dotenv::get_or(key, fallback);
        try { return std::stoi(raw); }
        catch (...) { throw std::runtime_error(key + " must be a number, got '" + raw + "'"); }
    }

    bool toBool(const std::string& key, const std::string& fallback) {
        std::string raw = dotenv::get_or(key, fallback);
        for (auto& c : raw) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (raw == "true" || raw == "1" || raw == "yes") return true;
        if (raw == "false" || raw == "0" || raw == "no") return false;
        throw std::runtime_error(key + " must be true or false, got '" + raw + "'");
    }

    std::string withoutTrailingSlash(std::string value) {
        while (!value.empty() && value.back() == '/') value.pop_back();
        return value;
    }

    std::vector<std::string> splitList(const std::string& raw) {
        std::vector<std::string> out;
        std::stringstream in(raw);
        std::string item;
        while (std::getline(in, item, ',')) {
            item = withoutTrailingSlash(dotenv::trim(item));
            if (!item.empty()) out.push_back(item);
        }
        return out;
    }

    // Drogon's Redis client (trantor::InetAddress) only accepts an IP like "10.0.0.5".
    // Render gives Key Value a hostname like "red-abc123", so look it up once at startup.
    std::string resolveHost(const std::string& key, const std::string& host) {
        in_addr ip{};
        if (inet_pton(AF_INET, host.c_str(), &ip) == 1) return host;   // already an IP

    #ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    #endif
        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* found = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || !found) {
            throw std::runtime_error(key + ": could not resolve host '" + host + "'");
        }
        char text[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(found->ai_addr)->sin_addr, text, sizeof(text));
        freeaddrinfo(found);
        return text;
    }

    void readRedisUrl(const std::string& url, RedisSettings& redis) {
        static const std::regex pattern(R"(redis://(?:([^:@/]*)(?::([^@/]*))?@)?([^:/]+)(?::(\d+))?(?:/(\d+))?/?)");
        std::smatch m;
        if (!std::regex_match(url, m, pattern)) {
            throw std::runtime_error("REDIS_URL must look like redis://user:password@host:6379/0 (rediss:// is not supported)");
        }
        if (m[1].matched && m[1].length() > 0 && m[1] != "default") redis.username = m[1];
        if (m[2].matched) redis.password = m[2];
        redis.host = m[3];
        if (m[4].matched) redis.port = std::stoi(m[4]);
        if (m[5].matched) redis.db = std::stoi(m[5]);
    }
}

Settings* getSettings() {
    if (g_settings) return g_settings.get();

    dotenv::load_dotenv();
    auto s = std::make_unique<Settings>();
    s->port        = toInt("PORT", "10000");
    s->threads     = toInt("THREADS", "4");
    s->environment = dotenv::get_or("APP_ENV", "development");
    s->host        = dotenv::get_or("APP_HOST", "127.0.0.1");
    s->frontendLink = withoutTrailingSlash(dotenv::require("FRONTEND_LINK"));
    s->corsAllowedOrigins = splitList(dotenv::get_or("CORS_ALLOWED_ORIGINS", ""));

    // Either one connection URL (Render) or separate fields (local .env).
    s->db.url         = dotenv::get_or("DB_URL", "");
    if (s->db.url.empty()) {
        s->db.host     = dotenv::require("DB_HOST");
        s->db.port     = toInt("DB_PORT", "5432");
        s->db.name     = dotenv::require("DB_NAME");
        s->db.user     = dotenv::require("DB_USER");
        s->db.password = dotenv::require("DB_PASSWORD");
    }
    s->db.schema      = dotenv::get_or("DB_SCHEMA", "public");
    s->db.connections = toInt("DB_CONNECTIONS", "4");

    s->jwt.accessSecret      = dotenv::require("JWT_ACCESS_SECRET");
    s->jwt.refreshSecret     = dotenv::require("JWT_REFRESH_SECRET");
    s->jwt.accessExpiration  = dotenv::get_or("JWT_ACCESS_EXPIRATION", "30d");
    s->jwt.refreshExpiration = dotenv::get_or("JWT_REFRESH_EXPIRATION", "90d");
    if (s->jwt.accessSecret == s->jwt.refreshSecret) {
        throw std::runtime_error("JWT_ACCESS_SECRET and JWT_REFRESH_SECRET must be different");
    }

    s->redis.host     = dotenv::get_or("REDIS_HOST", "127.0.0.1");
    s->redis.port     = toInt("REDIS_PORT", "6379");
    s->redis.password = dotenv::get_or("REDIS_PASSWORD", "");
    s->redis.username = dotenv::get_or("REDIS_USERNAME", "");
    s->redis.db       = toInt("REDIS_DB", "0");
    if (const auto url = dotenv::get_or("REDIS_URL", ""); !url.empty()) readRedisUrl(url, s->redis);
    s->redis.host        = resolveHost("REDIS_HOST", s->redis.host);
    s->redis.maxAttempts = toInt("QUEUE_MAX_ATTEMPTS", "3");
    s->redis.maxInFlight = toInt("QUEUE_MAX_IN_FLIGHT", "10");

    s->email.resendApiKey = dotenv::require("RESEND_API_KEY");
    s->email.from         = dotenv::require("EMAIL_FROM");
    s->email.templatesDir = dotenv::get_or("EMAIL_TEMPLATES_DIR", "templates/emails");
    s->email.appUrl       = s->frontendLink;
    s->email.assetsUrl    = dotenv::get_or("ASSETS_URL", "");

    s->cloudinary.cloudName = dotenv::require("CLOUDINARY_CLOUD_NAME");
    s->cloudinary.apiKey    = dotenv::require("CLOUDINARY_API_KEY");
    s->cloudinary.apiSecret = dotenv::require("CLOUDINARY_API_SECRET");
    s->cloudinary.apiUrl    = dotenv::get_or("CLOUDINARY_API_URL", "https://api.cloudinary.com");
    s->cloudinary.folder    = withoutTrailingSlash(dotenv::get_or("CLOUDINARY_FOLDER", "evento"));

    s->google.clientId = dotenv::get_or("GOOGLE_CLIENT_ID", "");

    s->geo.enabled     = toBool("GEO_LOOKUP_ENABLED", "true");
    s->geo.ipinfoToken = dotenv::get_or("IPINFO_TOKEN", "");
    s->paystack.apiKey = dotenv::get_or("PAYSTACK_SECRET_KEY", "");
    s->paystack.url    = dotenv::get_or("PAYSTACK_API_URL", "https://api.paystack.co");

    if (s->port < 1 || s->port > 65535) throw std::runtime_error("PORT out of range");
    static const std::regex folder(R"([A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*)");
    if (!std::regex_match(s->cloudinary.folder, folder)) {
        throw std::runtime_error("CLOUDINARY_FOLDER may only contain letters, numbers, _, - and /");
    }

    g_settings = std::move(s);
    return g_settings.get();
}
