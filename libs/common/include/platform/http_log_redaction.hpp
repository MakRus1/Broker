#pragma once

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <userver/formats/json.hpp>
#include <userver/formats/json/exception.hpp>

namespace platform::http {
namespace detail {
inline userver::formats::json::ValueBuilder RedactLogValue(
    const userver::formats::json::Value& value, std::initializer_list<std::string_view> fields) {
    namespace json = userver::formats::json;
    if (value.IsObject()) {
        json::ValueBuilder object{json::Type::kObject};
        for (auto it = value.begin(); it != value.end(); ++it) {
            const auto name = it.GetName();
            if (std::find(fields.begin(), fields.end(), name) != fields.end())
                object[name] = "***";
            else
                object[name] = RedactLogValue(*it, fields);
        }
        return object;
    }

    if (value.IsArray()) {
        json::ValueBuilder array{json::Type::kArray};
        for (const auto& item : value)
            array.PushBack(RedactLogValue(item, fields));
        return array;
    }

    return json::ValueBuilder{value};
}
}  // namespace detail

inline std::string RedactLogBody(
    const std::string& body, std::initializer_list<std::string_view> fields) {
    namespace json = userver::formats::json;
    if (body.empty())
        return {};

    try {
        const auto document = json::FromString(body);
        if (document.IsObject() || document.IsArray())
            return json::ToString(detail::RedactLogValue(document, fields).ExtractValue());
    } catch (const json::Exception&) {
        // Ошибка разбора может содержать части входного тела.
    }
    return "<body omitted>";
}
}  // namespace platform::http
