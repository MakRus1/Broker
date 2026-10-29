#include <boost/uuid/string_generator.hpp>
#include <components/transportation_dispatcher.hpp>
#include <postgresql/types.hpp>
#include <set>
#include <shipments/sql_queries.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

TransportationDispatcher::TransportationDispatcher(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), deps_(ResolveDeps(context)),
      tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()) {
    task_.Start(
        "transportation-dispatcher", {},
        [this]() -> std::optional<pgtypes::DeliveryV2> {
            const auto rows =
                deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kClaimOperation);
            if (rows.IsEmpty())
                return std::nullopt;
            return rows.AsSingleRow<pgtypes::DeliveryV2>();
        },
        [this](const auto& delivery) { return SendOperation(delivery); },
        [this](const auto& delivery, const pgtypes::CreationOperationV1& result) {
            deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kCompleteOperation,
                delivery.Organization, result, delivery.Lease);
        },
        [this](const auto& delivery) {
            // Ошибка HTTP не является доменным отказом: резерв остаётся до результата операции.
            deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kRetryOperation,
                delivery.Organization, delivery.Id, delivery.Lease);
        });
    route_task_.Start(
        "route-projection-dispatcher", {},
        [this]() -> std::optional<RouteDelivery> {
            const auto rows =
                deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kClaimRouteProjection);
            if (rows.IsEmpty())
                return std::nullopt;
            return rows.AsSingleRow<RouteDelivery>(pg::kRowTag);
        },
        [this](const auto& delivery) { SendRouteProjection(delivery); },
        [this](const auto& delivery) {
            deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kCompleteRouteProjection,
                delivery.Organization, delivery.Shipment, delivery.Lease);
        },
        [this](const auto& delivery) {
            deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kRetryRouteProjection,
                delivery.Organization, delivery.Shipment, delivery.Lease);
        });
}

TransportationDispatcher::~TransportationDispatcher() {
    route_task_.Stop();
    task_.Stop();
}

pgtypes::CreationOperationV1 TransportationDispatcher::SendOperation(
    const pgtypes::DeliveryV2& delivery) {
    namespace api = clients::transportations::ApiInternalV1TransportationsEnsureCreated;
    const auto request = api::Request{.body = api::ParseBody(delivery.Payload)};
    const auto result = deps_.transportations.ApiInternalV1TransportationsEnsureCreated(
        request, tokens_.GetAccessToken());
    const auto& value = result.operation;
    // Ответ обязан относиться к той же операции и количеству, иначе резерв сохраняется.
    if (boost::uuids::string_generator{}(value.id) != delivery.Id ||
        value.item_id != request.body.item_id || value.quantity != request.body.quantity)
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
    return pgtypes::CreationOperationV1{.Id = delivery.Id,
        .ItemId = boost::uuids::string_generator{}(value.item_id),
        .Quantity = value.quantity,
        .Status = status,
        .Result = document};
}

void TransportationDispatcher::SendRouteProjection(const RouteDelivery& delivery) {
    namespace api = clients::transportations::ApiInternalV1ShipmentsRouteProjection;
    json::ValueBuilder body;
    body["organization_id"] = delivery.Organization;
    body["shipment_id"] = delivery.Shipment;
    body["revision"] = delivery.Revision;
    body["route_segments"] = delivery.Segments;
    deps_.transportations.ApiInternalV1ShipmentsRouteProjection(
        api::Request{.body = api::ParseBody(body.ExtractValue())}, tokens_.GetAccessToken());
}

userver::yaml_config::Schema TransportationDispatcher::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Надежная доставка операций создания перевозок
additionalProperties: false
properties: {}
)");
}
}  // namespace shipments
