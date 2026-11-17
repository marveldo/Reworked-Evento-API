#pragma once
#include <regex>
#include <string>

struct DeviceDetails {
    std::string type;
    std::string name;
    std::string os;
};

namespace user_agent {

inline bool has(const std::string& ua, const char* text) {
    return ua.find(text) != std::string::npos;
}

inline std::string osFamily(const std::string& ua) {
    if (has(ua, "Windows"))                                          return "Windows";
    if (has(ua, "iPhone") || has(ua, "iPad") || has(ua, "iPod"))     return "iOS";
    if (has(ua, "Android"))                                          return "Android";
    if (has(ua, "CrOS"))                                             return "Chrome OS";
    if (has(ua, "Mac OS X") || has(ua, "Macintosh"))                 return "Mac OS X";
    if (has(ua, "Ubuntu"))                                           return "Ubuntu";
    if (has(ua, "Fedora"))                                           return "Fedora";
    if (has(ua, "Linux"))                                            return "Linux";
    return "Other";
}

inline std::string deviceFamily(const std::string& ua) {
    static const std::regex bot(R"(bot|spider|crawl|slurp)", std::regex::icase);
    if (std::regex_search(ua, bot)) return "Spider";
    if (has(ua, "iPhone")) return "iPhone";
    if (has(ua, "iPad"))   return "iPad";
    if (has(ua, "iPod"))   return "iPod";
    if (has(ua, "Android")) {
        static const std::regex model(R"(Android[^;)]*;\s*(?:[a-z]{2}[-_][a-zA-Z]{2};\s*)?([^;)]+?)(?:\s+Build/[^;)]*)?\s*[;)])");
        std::smatch m;
        if (std::regex_search(ua, m, model)) {
            std::string name = m[1].str();
            if (name.rfind("SM-", 0) == 0) return "Samsung " + name;
            if (!name.empty() && name != "wv") return name;
        }
        return "Generic Smartphone";
    }
    if (has(ua, "Macintosh")) return "Mac";
    return "Other";
}

inline DeviceDetails parse(const std::string& ua) {
    DeviceDetails details;
    details.os   = osFamily(ua);
    details.name = deviceFamily(ua);

    const bool tablet = has(ua, "iPad") || has(ua, "Tablet") || has(ua, "Kindle") || has(ua, "Silk") ||
                        (has(ua, "Android") && !has(ua, "Mobile"));
    const bool mobile = !tablet && (has(ua, "iPhone") || has(ua, "iPod") || has(ua, "Mobile") ||
                                    has(ua, "Windows Phone") || has(ua, "Opera Mini"));
    details.type = mobile ? "Mobile" : tablet ? "Tablet" : "PC";
    return details;
}

}
