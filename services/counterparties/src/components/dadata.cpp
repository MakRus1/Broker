#include <components/dadata.hpp>
#include <models/errors.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
bool IsInn(const std::string& value) {
    if ((value.size() != 10 && value.size() != 12) ||
        value.find_first_not_of("0123456789") != std::string::npos)
        return false;
    const auto check = [&](std::initializer_list<int> weights, std::size_t digit) {
        int sum = 0;
        std::size_t index = 0;
        for (const auto weight : weights)
            sum += weight * (value[index++] - '0');
        return sum % 11 % 10 == value[digit] - '0';
    };
    if (value.find_first_not_of('0') == std::string::npos)
        return false;
    return value.size() == 10 ? check({2, 4, 10, 3, 5, 9, 4, 6, 8}, 9)
                              : check({7, 2, 4, 10, 3, 5, 9, 4, 6, 8}, 10) &&
            check({3, 7, 2, 4, 10, 3, 5, 9, 4, 6, 8}, 11);
}

counterparties::models::RegistryParty Convert(const userver::formats::json::Value& item) {
    const auto& data = item["data"];
    return {.kpp = data["kpp"].As<std::optional<std::string>>(),
        .legal_address = data["address"].IsNull() || data["address"].IsMissing()
            ? std::nullopt
            : data["address"]["unrestricted_value"].As<std::optional<std::string>>(),
        .legal_name = data["name"]["full_with_opf"].As<std::optional<std::string>>(),
        .name = data["name"]["short_with_opf"].As<std::optional<std::string>>().value_or(
            item["value"].As<std::string>()),
        .registration_id = data["ogrn"].As<std::optional<std::string>>(),
        .status = data["state"]["status"].As<std::optional<std::string>>(),
        .tax_id = data["inn"].As<std::string>()};
}
}  // namespace

namespace counterparties {
DaData::DaData(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>("dadata-http-client")
                .GetHttpClient()),
      endpoint_(config["endpoint"].As<std::string>()),
      api_key_(config["api-key"].As<std::string>()), timeout_(config["timeout-ms"].As<int>()) {
    if (timeout_.count() <= 0 ||
        !(endpoint_.starts_with("https://") ||
            (config["allow-http"].As<bool>() && endpoint_.starts_with("http://"))))
        throw std::invalid_argument("Некорректные настройки DaData");
}

std::vector<models::RegistryParty> DaData::Lookup(const std::string& tax_id) const try {
    if (!IsInn(tax_id))
        throw ApiError<400>{
            "invalid_inn", "Проверьте ИНН: нужны 10 или 12 цифр с верной контрольной суммой"};
    if (api_key_.empty())
        throw ApiError<503>{"registry_not_configured",
            "Поиск реквизитов пока не настроен. Заполните данные вручную"};
    userver::formats::json::ValueBuilder body;
    body["query"] = tax_id;
    body["branch_type"] = "MAIN";
    body["count"] = 1;
    const auto response =
        http_.CreateRequest()
            .post(endpoint_, userver::formats::json::ToString(body.ExtractValue()))
            .headers({{"Authorization", "Token " + api_key_}, {"Content-Type", "application/json"},
                {"Accept", "application/json"}})
            .timeout(timeout_)
            .retry(0)
            .follow_redirects(false)
            .SetLoggedUrl("dadata/find-party")
            .perform();
    if (response->status_code() != 200)
        throw ApiError<503>{"registry_unavailable",
            "DaData сейчас недоступна. Повторите поиск позже или заполните данные вручную"};
    const auto document = userver::formats::json::FromString(response->body());
    const auto& items = document["suggestions"];
    if (!items.IsArray() || items.GetSize() > 1)
        throw std::runtime_error("Некорректный ответ реестра");
    std::vector<models::RegistryParty> result;
    for (const auto& item : items) {
        auto value = Convert(item);
        if (value.tax_id != tax_id || value.name.empty())
            throw std::runtime_error("Ответ реестра не соответствует запросу");
        result.push_back(std::move(value));
    }
    return result;
} catch (const platform::http::HandlerErrorBase&) {

    throw;
} catch (const std::bad_alloc&) {
    throw;
} catch (const std::exception&) {
    // Текст внешних ошибок не переносим в ответ: он может содержать настройки клиента.
    throw ApiError<503>{"registry_unavailable",
        "Не удалось получить реквизиты из DaData. Повторите позже или заполните вручную"};
}

userver::yaml_config::Schema DaData::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Поиск реквизитов головной организации или ИП в DaData
additionalProperties: false
properties:
    endpoint: {type: string, description: Адрес API DaData}
    api-key: {type: string, description: Серверный ключ из окружения}
    timeout-ms: {type: integer, minimum: 1, description: Таймаут запроса}
    allow-http: {type: boolean, description: HTTP только для тестового mockserver}
)");
}
}  // namespace counterparties
