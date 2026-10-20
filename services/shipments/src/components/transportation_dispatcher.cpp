#include <components/transportation_dispatcher.hpp>
#include <set>
#include <shipments/sql_queries.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/http/url.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

TransportationDispatcher::TransportationDispatcher(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), deps_(ResolveDeps(context)),
      http_(context.FindComponent<userver::components::HttpClient>("auth-http-client")
                .GetHttpClient()),
      token_url_(config["token-url"].As<std::string>()),
      client_id_(config["client-id"].As<std::string>()),
      client_secret_(config["client-secret"].As<std::string>()) {
    if (config["enabled"].As<bool>(false)) {
        if (client_id_.empty() || client_secret_.empty() ||
            !(token_url_.starts_with("https://") ||
                (config["allow-http"].As<bool>(false) && token_url_.starts_with("http://"))))
            throw std::invalid_argument(
                "Настройте учетную запись dispatcher и защищенный token endpoint");
        task_.Start("transportation-dispatcher", std::chrono::seconds{1}, [this] { Step(); });
    }
}

TransportationDispatcher::~TransportationDispatcher() {
    task_.Stop();
}

void TransportationDispatcher::Step() {
    const auto rows = deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kClaimOperation);
    if (rows.IsEmpty())
        return;
    const auto operation = rows.AsSingleRow<json::Value>();
    const auto tenant = operation["tenant_id"].As<std::string>();
    const auto id = operation["id"].As<std::string>();
    const auto lease = operation["lease_token"].As<std::string>();
    try {
        // Секрет и access token не попадают в URL, диагностические сообщения или бизнес-аудит.
        const auto token_response =
            http_.CreateRequest()
                .post(token_url_,
                    userver::http::MakeQuery({{"grant_type", "client_credentials"},
                        {"client_id", client_id_}, {"client_secret", client_secret_}}))
                .headers({{"Content-Type", "application/x-www-form-urlencoded"}})
                .timeout(std::chrono::seconds{2})
                .retry(0)
                .follow_redirects(false)
                .perform();
        token_response->raise_for_status();
        const auto token =
            json::FromString(token_response->body())["access_token"].As<std::string>();
        namespace api = clients::transportations::ApiInternalV1TransportationsEnsureCreated;
        const auto result = deps_.transportations.ApiInternalV1TransportationsEnsureCreated(
            api::Request{.body = api::ParseBody(operation["payload"])}, token);
        const auto& value = result.operation;
        // Ответ обязан относиться к той же операции и количеству, иначе резерв сохраняется.
        if (value.id != id || value.item_id != operation["payload"]["item_id"].As<std::string>() ||
            value.quantity != operation["payload"]["quantity"].As<std::int64_t>())
            throw std::runtime_error("Несогласованный результат операции");
        const auto status = clients::transportations::models::EnumValue(value.status);
        if (status != "succeeded" && status != "rejected")
            throw std::runtime_error("Операция не завершена");
        if (status == "succeeded" &&
            value.transportation_ids.size() != static_cast<std::size_t>(value.quantity))
            throw std::runtime_error("Несогласованное количество перевозок");
        const std::set<std::string> unique_ids(
            value.transportation_ids.begin(), value.transportation_ids.end());
        if (unique_ids.size() != value.transportation_ids.size())
            throw std::runtime_error("Повторяющиеся ID в результате операции");
        const auto document = json::ValueBuilder(value).ExtractValue();
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kCompleteOperation, tenant, id,
            lease, status, document);
    } catch (const std::exception&) {
        // Даже 4xx не является подтвержденным доменным отказом: резерв не освобождается по
        // таймауту.
        LOG_WARNING() << "Transportation delivery deferred";
        deps_.pg_cluster->Execute(
            pg::ClusterHostType::kMaster, sql::kRetryOperation, tenant, id, lease);
    }
}

userver::yaml_config::Schema TransportationDispatcher::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Надежная доставка операций создания перевозок
additionalProperties: false
properties:
    enabled:
        type: boolean
        description: Включить worker
        defaultDescription: false
    token-url:
        type: string
        description: Endpoint клиентской авторизации
    client-id:
        type: string
        description: Служебный OIDC client
    client-secret:
        type: string
        description: Секрет из окружения
    allow-http:
        type: boolean
        description: Только для локального тестирования
        defaultDescription: false
)");
}
}  // namespace shipments
