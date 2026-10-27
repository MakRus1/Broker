#pragma once
#include <boost/algorithm/string/trim.hpp>
#include <helpers/database.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/crypto/hash.hpp>
#include <userver/formats/json/value_builder.hpp>

namespace transportations::helpers {
struct RegistryRow {
    pgtypes::TransportationV4 Transportation;
    std::string SortKey;
};

inline std::string RegistryScope(
    const models::ListTransportationsRequest& request, const std::string& organization) {
    userver::formats::json::ValueBuilder value{request};
    value.Remove("cursor");
    value.Remove("after");
    value.Remove("limit");
    value["organization_id"] = organization;
    return userver::crypto::hash::Sha256(
        userver::formats::json::ToStableString(value.ExtractValue()));
}

inline std::optional<std::string> ContainerSearch(const std::optional<std::string>& query) {
    if (!query.has_value())
        return std::nullopt;
    const auto text = boost::algorithm::trim_copy(query.value());
    if (text.empty())
        return std::nullopt;
    std::string pattern;
    for (const char character : text) {
        if (character == '%' || character == '_' || character == '\\')
            pattern += '\\';
        pattern += character;
    }
    return '%' + pattern + '%';
}

inline std::optional<std::vector<std::string>> Convert(
    const std::optional<std::vector<models::ListTransportationsRequest_types_item>>& values) {
    if (!values.has_value())
        return std::nullopt;
    std::vector<std::string> result;
    for (const auto value : values.value())
        result.push_back(models::EnumValue(value));
    return result;
}

inline std::vector<pgtypes::TransportationV4> Convert(const std::vector<RegistryRow>& rows) {
    std::vector<pgtypes::TransportationV4> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.Transportation);
    return result;
}
}  // namespace transportations::helpers
