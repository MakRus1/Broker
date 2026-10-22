#pragma once
#include <algorithm>
#include <cctype>
#include <string_view>

namespace platform {
inline bool IsEmail(std::string_view value) {
    const auto at = value.find('@');
    if (value.size() > 254 || at == std::string_view::npos || at == 0 || at > 64 ||
        value.find('@', at + 1) != std::string_view::npos)
        return false;
    const auto local = value.substr(0, at);
    const auto domain = value.substr(at + 1);
    if (local.front() == '.' || local.back() == '.' || local.find("..") != std::string_view::npos ||
        domain.find('.') == std::string_view::npos)
        return false;
    if (std::any_of(local.begin(), local.end(), [](unsigned char c) {
            return c <= 32 || c == 127 ||
                std::string_view{"<>(),;:\"\\[]"}.find(c) != std::string_view::npos;
        }))
        return false;
    std::size_t offset = 0;
    while (offset < domain.size()) {
        const auto end = domain.find('.', offset);
        const auto label =
            domain.substr(offset, end == std::string_view::npos ? end : end - offset);
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-' ||
            std::any_of(label.begin(), label.end(),
                [](unsigned char c) { return !(std::isalnum(c) || c >= 128 || c == '-'); }))
            return false;
        if (end == std::string_view::npos)
            return true;
        offset = end + 1;
    }
    return false;
}

inline bool IsPhone(std::string_view value) {
    int digits = 0;
    bool opened = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = value[i];
        if (c >= '0' && c <= '9')
            ++digits;
        else if (c == '(' && !opened)
            opened = true;
        else if (c == ')' && opened)
            opened = false;
        else if (c != ' ' && c != '-' && c != '.' && !(c == '+' && i == 0))
            return false;
    }
    return !opened && digits >= 7 && digits <= 15;
}
}  // namespace platform
