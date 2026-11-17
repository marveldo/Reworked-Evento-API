#pragma once
#include <cstddef>
#include <string>

struct PageInput {
    size_t limit  = 100;
    size_t offset = 0;
};

inline PageInput parsePageQuery(const std::string& limit, const std::string& offset, size_t defaultLimit = 100) {
    static constexpr size_t MAX_LIMIT = 1000;
    PageInput page;
    page.limit = defaultLimit;

    const auto positive = [](const std::string& raw, bool allowZero) -> long long {
        try {
            size_t used = 0;
            const long long value = std::stoll(raw, &used);
            if (used != raw.size() || value < 0 || (!allowZero && value == 0)) return -1;
            return value;
        } catch (const std::exception&) {
            return -1;
        }
    };

    if (!limit.empty()) {
        const long long value = positive(limit, false);
        if (value > 0) page.limit = static_cast<size_t>(std::min<long long>(value, MAX_LIMIT));
    }
    if (!offset.empty()) {
        const long long value = positive(offset, true);
        if (value >= 0) page.offset = static_cast<size_t>(value);
    }
    return page;
}
