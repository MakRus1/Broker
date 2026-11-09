#pragma once
#include <models/user.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/parse/common_containers.hpp>

namespace organizations::converters {
inline std::optional<std::string> ReadMiddleName(const userver::formats::json::Value& user) {
    const auto attribute = user["attributes"]["middle_name"];
    if (attribute.IsMissing() || attribute.IsNull())
        return std::nullopt;
    if (!attribute.IsArray() || attribute.GetSize() > 1)
        throw std::runtime_error("Keycloak вернул некорректный атрибут middle_name");
    if (attribute.IsEmpty())
        return std::nullopt;
    return attribute[0].As<std::string>();
}

inline User Convert(const userver::formats::json::Value& value) {
    const auto last = value["lastName"].As<std::optional<std::string>>();
    const auto first = value["firstName"].As<std::optional<std::string>>();
    const auto middle = ReadMiddleName(value);
    std::string display;
    for (const auto& part : {last, first, middle}) {
        if (part.has_value() && !part.value().empty()) {
            if (!display.empty())
                display += " ";
            display += part.value();
        }
    }
    return {value["id"].As<std::string>(), value["username"].As<std::string>(),
        display.empty() ? value["username"].As<std::string>() : display,
        value["email"].As<std::string>(""), last, first, middle};
}
}  // namespace organizations::converters
