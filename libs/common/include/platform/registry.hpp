#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/serialize/boost_uuid.hpp>
#include <userver/utils/text_light.hpp>
#include <utility>

namespace platform::registry {
namespace detail {
inline std::string HashCursorScope(userver::formats::json::ValueBuilder value) {
    value.Remove("cursor");
    value.Remove("after");
    value.Remove("limit");
    return userver::crypto::hash::Sha256(
        userver::formats::json::ToStableString(value.ExtractValue()));
}
}  // namespace detail

template <typename Request>
std::string CursorScope(const Request& request, const boost::uuids::uuid& organization) {
    userver::formats::json::ValueBuilder value{request};
    value["organization_id"] = organization;
    return detail::HashCursorScope(std::move(value));
}

template <typename Request>
std::string GlobalCursorScope(const Request& request, std::string_view registry) {
    userver::formats::json::ValueBuilder value{request};
    value["registry"] = registry;
    return detail::HashCursorScope(std::move(value));
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
