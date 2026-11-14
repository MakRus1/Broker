#include <components/cities.hpp>
#include <converters/cities.hpp>
#include <platform/handler_error.hpp>
#include <userver/clients/http/error.hpp>

#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace web_bff {
template <int Status> using ApiError = platform::http::HandlerError<Status>;

Cities::Cities(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>("cities-http-client")
                .GetHttpClient()),
      endpoint_(config["endpoint"].As<std::string>()),
      api_key_(config["api-key"].As<std::string>()), timeout_(config["timeout-ms"].As<int>()) {
    if (timeout_.count() <= 0 ||
        !(endpoint_.starts_with("https://") ||
            (config["allow-http"].As<bool>() && endpoint_.starts_with("http://"))))
        throw std::invalid_argument("Некорректные настройки поиска городов");
}

std::vector<models::City> Cities::FindCities(const std::string& query) const try {
    namespace json = userver::formats::json;
    if (api_key_.empty())
        throw ApiError<503>{
            "cities_not_configured", "Поиск городов не настроен. Введите город и страну вручную"};
    json::ValueBuilder body;
    body["query"] = userver::utils::text::Trim(query);
    body["count"] = 10;
    body["from_bound"]["value"] = "city";
    body["to_bound"]["value"] = "city";
    json::ValueBuilder location;
    location["country"] = "*";
    body["locations"] = json::ValueBuilder{json::Type::kArray};
    body["locations"].PushBack(location.ExtractValue());
    const auto response =
        http_.CreateRequest()
            .post(endpoint_, json::ToString(body.ExtractValue()))
            .headers({{"Authorization", "Token " + api_key_}, {"Content-Type", "application/json"}})
            .timeout(timeout_)
            .retry(0)
            .follow_redirects(false)
            .SetLoggedUrl("dadata/cities")
            .perform();
    if (response->status_code() != 200)
        throw ApiError<503>{
            "cities_unavailable", "Не удалось загрузить города. Введите город и страну вручную"};
    const auto document = json::FromString(response->body());
    return converters::ConvertCitySuggestions(document);
} catch (const userver::clients::http::CancelException&) {

    throw;
} catch (const userver::clients::http::BadArgumentException&) {
    throw;
} catch (const userver::clients::http::TechnicalError&) {
    throw;
} catch (const userver::clients::http::BaseException&) {
    throw ApiError<503>{
        "cities_unavailable", "Не удалось загрузить города. Введите город и страну вручную"};
} catch (const userver::formats::json::Exception&) {
    throw ApiError<503>{
        "cities_unavailable", "Не удалось загрузить города. Введите город и страну вручную"};
}

userver::yaml_config::Schema Cities::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Подсказки городов DaData с возможностью ручного ввода
additionalProperties: false
properties:
    endpoint: {type: string, description: Адрес API DaData}
    api-key: {type: string, description: Серверный ключ из окружения}
    timeout-ms: {type: integer, minimum: 1, description: Таймаут запроса}
    allow-http: {type: boolean, description: HTTP только для testsuite}
)");
}
}  // namespace web_bff
