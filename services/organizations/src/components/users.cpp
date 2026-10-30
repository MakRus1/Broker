#include <components/users.hpp>
#include <helpers/errors.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
std::optional<std::string> ReadMiddleName(const userver::formats::json::Value& user) {
    const auto attribute = user["attributes"]["middle_name"];
    if (attribute.IsMissing() || attribute.IsNull())
        return std::nullopt;
    if (!attribute.IsArray() || attribute.GetSize() > 1)
        throw std::runtime_error("Keycloak вернул некорректный атрибут middle_name");
    if (attribute.IsEmpty())
        return std::nullopt;
    return attribute[0].As<std::string>();
}

organizations::User Convert(const userver::formats::json::Value& value) {
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
}  // namespace

namespace organizations {
Users::Users(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      http_(context.FindComponent<userver::components::HttpClient>("auth-http-client")
                .GetHttpClient()),
      tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()),
      base_url_(config["base-url"].As<std::string>()), timeout_(config["timeout-ms"].As<int>()) {
    if (timeout_.count() <= 0 ||
        !(base_url_.starts_with("https://") ||
            (config["allow-http"].As<bool>() && base_url_.starts_with("http://"))))
        throw std::invalid_argument("Некорректные настройки Keycloak Admin API");
}

userver::clients::http::Request Users::MakeRequest() const {
    return http_.CreateRequest()
        .headers({{"Authorization", "Bearer " + tokens_.GetAccessToken()},
            {"Content-Type", "application/json"}})
        .timeout(timeout_)
        .follow_redirects(false)
        .SetLoggedUrl("keycloak/users");
}

userver::formats::json::Value Users::Lookup(const std::string& username) const {
    const auto response =
        MakeRequest()
            .get(base_url_ + "/users?" +
                userver::http::MakeQuery({{"username", username}, {"exact", "true"}}))
            .retry(1)
            .perform();
    if (response->status_code() != 200)
        throw ApiError<503>{
            "identity_provider_unavailable", "Не удалось обратиться к провайдеру пользователей"};
    const auto items = userver::formats::json::FromString(response->body());
    if (items.GetSize() != 1)
        throw ApiError<404>{"user_not_found", "Пользователь с таким логином не найден"};
    return items[0];
}

User Users::Find(const std::string& username) const try {
    const auto value = Lookup(username);
    if (!value["enabled"].As<bool>(false) || !value["serviceAccountClientId"].IsMissing())
        throw ApiError<400>{
            "invalid_user", "Нельзя добавить отключённую или служебную учётную запись"};
    return Convert(value);
} catch (const platform::http::HandlerErrorBase&) {

    throw;
} catch (const std::bad_alloc&) {
    throw;
} catch (const std::exception&) {
    throw ApiError<503>{
        "identity_provider_unavailable", "Провайдер пользователей временно недоступен"};
}

User Users::Create(const std::string& request_id, const std::string& username,
    const std::string& email, const std::string& last_name, const std::string& first_name,
    const std::optional<std::string>& middle_name, const std::string& temporary_password) const
    try {
    // Адаптер внешнего API; пароль не сохраняется в нашей БД и не возвращается клиенту.
    userver::formats::json::ValueBuilder body;
    body["username"] = username;
    body["email"] = email;
    body["emailVerified"] = false;
    body["firstName"] = first_name;
    body["lastName"] = last_name;
    if (middle_name.has_value())
        body["attributes"]["middle_name"].PushBack(middle_name.value());
    body["enabled"] = true;
    body["attributes"]["broker-provisioning-id"].PushBack(request_id);
    userver::formats::json::ValueBuilder credential;
    credential["type"] = "password";
    credential["value"] = temporary_password;
    credential["temporary"] = true;
    body["credentials"].PushBack(credential.ExtractValue());
    const auto response =
        MakeRequest()
            .post(base_url_ + "/users", userver::formats::json::ToString(body.ExtractValue()))
            .retry(0)
            .perform();
    if (response->status_code() != 201 && response->status_code() != 409)
        throw ApiError<503>{"identity_provider_unavailable",
            "Создание пользователя не завершено. Повторите с тем же идентификатором запроса"};
    // Keycloak возвращает 201 без тела. Чтение также проверяет владельца запроса при 409.
    const auto value = Lookup(username);
    const auto provisioning = value["attributes"]["broker-provisioning-id"];
    if (provisioning.IsMissing() || provisioning.GetSize() != 1 ||
        provisioning[0].As<std::string>() != request_id)
        throw ApiError<409>{
            "username_taken", "Логин уже занят. Используйте добавление существующего пользователя"};
    return Convert(value);
} catch (const platform::http::HandlerErrorBase&) {

    throw;
} catch (const std::bad_alloc&) {
    throw;
} catch (const std::exception&) {
    throw ApiError<503>{
        "identity_provider_unavailable", "Создание пользователя не завершено. Повторите запрос"};
}

User Users::UpdateProfile(const std::string& subject, const std::string& last_name,
    const std::string& first_name, const std::optional<std::string>& middle_name) const try {
    const auto response = MakeRequest().get(base_url_ + "/users/" + subject).retry(0).perform();
    if (response->status_code() == 404)
        throw ApiError<404>{"user_not_found", "Пользователь не найден"};
    if (response->status_code() != 200)
        throw std::runtime_error("Не удалось прочитать профиль");
    const auto current = userver::formats::json::FromString(response->body());
    if (!current["serviceAccountClientId"].IsMissing())
        throw ApiError<400>{"invalid_user", "Нельзя менять профиль служебной учётной записи"};
    userver::formats::json::ValueBuilder profile(current);
    profile["lastName"] = last_name;
    profile["firstName"] = first_name;
    if (middle_name.has_value()) {
        profile["attributes"]["middle_name"] =
            userver::formats::json::ValueBuilder(userver::formats::common::Type::kArray);
        profile["attributes"]["middle_name"].PushBack(middle_name.value());
    } else if (!current["attributes"].IsMissing() && !current["attributes"].IsNull()) {
        profile["attributes"].Remove("middle_name");
    }
    profile.Remove("credentials");
    const auto document = profile.ExtractValue();
    const auto updated =
        MakeRequest()
            .put(base_url_ + "/users/" + subject, userver::formats::json::ToString(document))
            .retry(0)
            .perform();
    if (updated->status_code() != 204)
        throw std::runtime_error("Не удалось обновить профиль");
    return Convert(document);
} catch (const platform::http::HandlerErrorBase&) {

    throw;
} catch (const std::bad_alloc&) {
    throw;
} catch (const std::exception&) {
    throw ApiError<503>{
        "identity_provider_unavailable", "Не удалось обновить ФИО. Повторите запрос"};
}

userver::yaml_config::Schema Users::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Создание пользователей через Keycloak Admin API
additionalProperties: false
properties:
    base-url: {type: string, description: Адрес Admin API доверенного realm}
    timeout-ms: {type: integer, minimum: 1, description: Таймаут обращения}
    allow-http: {type: boolean, description: HTTP только для локального контура}
)");
}
}  // namespace organizations
