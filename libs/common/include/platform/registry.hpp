#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/formats/serialize/boost_uuid.hpp>
#include <userver/utils/text_light.hpp>
#include <variant>

namespace platform::registry {
template <typename Request>
std::string CursorScope(
    const Request& request, const std::variant<boost::uuids::uuid, std::string_view>& scope) {
    userver::formats::json::ValueBuilder value{request};
    value.Remove("cursor");
    value.Remove("after");
    value.Remove("limit");
    if (const auto* organization = std::get_if<boost::uuids::uuid>(&scope))
        value["organization_id"] = *organization;
    else
        value["registry"] = std::get<std::string_view>(scope);
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
