#include <limits>
#include <platform/auth/service_token.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace platform::auth {
ServiceTokenProvider::ServiceTokenProvider(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>("auth-http-client")
                .GetHttpClient()),
      endpoint_(config["endpoint"].As<std::string>()),
      client_id_(config["client-id"].As<std::string>()),
      client_secret_(config["client-secret"].As<std::string>()),
      timeout_(config["timeout-ms"].As<int>()), retries_(config["max-retries"].As<int>()) {
    if (client_id_.empty() || client_secret_.empty() || timeout_.count() <= 0 || retries_ < 0 ||
        retries_ >= std::numeric_limits<short>::max() ||
        !(endpoint_.starts_with("https://") ||
            (config["allow-http"].As<bool>() && endpoint_.starts_with("http://"))))
        throw std::invalid_argument("Некорректные настройки служебной авторизации");
}

std::string ServiceTokenProvider::GetAccessToken() const {
    const auto form = userver::http::MakeQuery({{"grant_type", "client_credentials"},
        {"client_id", client_id_}, {"client_secret", client_secret_}});
    const auto response =
        http_.CreateRequest()
            .post(endpoint_, form)
            .headers({{"Content-Type", "application/x-www-form-urlencoded"}})
            .timeout(timeout_)
            // API userver принимает общее число попыток, конфиг — число дополнительных.
            .retry(static_cast<short>(1 + retries_), true)
            // Служебный секрет не должен пересылаться на адрес из Location.
            .follow_redirects(false)
            .perform();
    response->raise_for_status();
    const auto token =
        userver::formats::json::FromString(response->body())["access_token"].As<std::string>();
    if (token.empty())
        throw std::runtime_error("Провайдер вернул пустой служебный токен");
    return token;
}

userver::yaml_config::Schema ServiceTokenProvider::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Получение OAuth client credentials токена
additionalProperties: false
properties:
    endpoint:
        type: string
        description: Endpoint провайдера авторизации
    client-id:
        type: string
        description: Служебный клиент
    client-secret:
        type: string
        description: Секрет из окружения
    timeout-ms:
        type: integer
        minimum: 1
        description: Таймаут одной попытки получения токена
    max-retries:
        type: integer
        minimum: 0
        description: Дополнительные HTTP попытки, помимо повторов задания доставки
    allow-http:
        type: boolean
        description: Разрешить HTTP в локальном тестовом контуре
)");
}
}  // namespace platform::auth
