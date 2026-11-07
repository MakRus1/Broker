#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace admin_bff {
template <typename T, typename U> T ConvertSales(const U& value) {
    const auto document = userver::formats::json::ValueBuilder(value).ExtractValue();
    if constexpr (requires { typename T::value_type; }) {
        T result;
        for (const auto& item : document)
            result.push_back(
                models::sales::ParseModelJson(item, std::type_identity<typename T::value_type>{}));
        return result;
    } else {
        return models::sales::ParseModelJson(document, std::type_identity<T>{});
    }
}
}  // namespace admin_bff
