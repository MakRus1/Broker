#pragma once
#include <optional>
#include <string>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/utils/text_light.hpp>

namespace platform::registry {
template <typename Request>
std::string CursorScope(const Request& request, const std::string& organization) {
    userver::formats::json::ValueBuilder value{request};
    value.Remove("cursor");
    value.Remove("after");
    value.Remove("limit");
    value["organization_id"] = organization;
    return userver::crypto::hash::Sha256(
        userver::formats::json::ToStableString(value.ExtractValue()));
}

inline std::optional<std::string> SearchPattern(const std::optional<std::string>& query) {
    if (!query.has_value())
        return std::nullopt;
    const auto text = userver::utils::text::Trim(query.value());
    if (text.empty())
        return std::nullopt;
    std::string pattern = "%";
    for (const char character : text) {
        if (character == '%' || character == '_' || character == '\\')
            pattern += '\\';
        pattern += character;
    }
    return pattern + '%';
}
}  // namespace platform::registry
