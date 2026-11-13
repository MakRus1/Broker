#include <converters/catalog.hpp>
#include <helpers/catalog_validation.hpp>
#include <helpers/parameters.hpp>
#include <platform/handler_error.hpp>

namespace localization::helpers {
namespace json = userver::formats::json;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

json::Value ValidateAndEncodeCatalog(
    const models::EditableCatalog& catalog, const json::Value& source) {
    const auto ru = source["ru"];
    const auto en = source["en"];
    const auto validate = [&](const auto& entries, const auto& builtin) {
        std::set<std::string> keys;
        for (const auto& entry : entries) {
            if (!builtin.HasMember(entry.key) || !keys.insert(entry.key).second ||
                entry.value.empty() || entry.value.size() > 10000)
                throw ApiError<400>{"invalid_translation", "Некорректный перевод: " + entry.key};
            if (Parameters(entry.value) !=
                Parameters(builtin[entry.key].template As<std::string>()))
                throw ApiError<400>{
                    "invalid_parameters", "Не совпадают параметры перевода: " + entry.key};
        }
    };
    validate(catalog.ru, ru);
    validate(catalog.en, en);
    auto value = converters::EncodeCatalog(catalog);
    if (json::ToString(value).size() > 2 * 1024 * 1024)
        throw ApiError<400>{"catalog_too_large", "Каталог превышает 2 МБ"};
    return value;
}

}  // namespace localization::helpers
