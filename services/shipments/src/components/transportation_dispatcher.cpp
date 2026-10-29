#include <boost/uuid/string_generator.hpp>
#include <components/transportation_dispatcher.hpp>
#include <postgresql/types.hpp>
#include <set>
#include <shipments/sql_queries.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
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
      tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()) {
    route_task_.Start("route-projection-dispatcher", std::chrono::milliseconds{100}, [this] {
        // Ограниченная пачка без искусственной паузы между записями backfill.
        for (int count = 0; count < 50 && DeliverRouteProjection(); ++count) {
        }
    });
    task_.Start("transportation-dispatcher", std::chrono::seconds{1}, [this] { Step(); });
}

TransportationDispatcher::~TransportationDispatcher() {
    route_task_.Stop();
    task_.Stop();
}

void TransportationDispatcher::Step() {
    const auto rows = deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kClaimOperation);
    if (rows.IsEmpty())
        return;
    const auto delivery = rows.AsSingleRow<pgtypes::DeliveryV2>();
    try {
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
        const pgtypes::CreationOperationV1 completed{.Id = delivery.Id,
            .ItemId = boost::uuids::string_generator{}(value.item_id),
            .Quantity = value.quantity,
            .Status = status,
            .Result = document};
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kCompleteOperation,
            delivery.Organization, completed, delivery.Lease);
    } catch (const std::exception&) {
        // Даже 4xx не является подтвержденным доменным отказом: резерв не освобождается по
        // таймауту.
        LOG_WARNING() << "Transportation delivery deferred";
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kRetryOperation,
            delivery.Organization, delivery.Id, delivery.Lease);
    }
}

bool TransportationDispatcher::DeliverRouteProjection() {
    struct Delivery {
        std::string Organization;
        std::string Shipment;
        std::int64_t Revision;
        json::Value Segments;
        std::string Lease;
    };

    const auto rows =
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kClaimRouteProjection);
    if (rows.IsEmpty())
        return false;
    const auto delivery = rows.AsSingleRow<Delivery>(pg::kRowTag);
    try {
        namespace api = clients::transportations::ApiInternalV1ShipmentsRouteProjection;
        json::ValueBuilder body;
        body["organization_id"] = delivery.Organization;
        body["shipment_id"] = delivery.Shipment;
        body["revision"] = delivery.Revision;
        body["route_segments"] = delivery.Segments;
        deps_.transportations.ApiInternalV1ShipmentsRouteProjection(
            api::Request{.body = api::ParseBody(body.ExtractValue())}, tokens_.GetAccessToken());
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kCompleteRouteProjection,
            delivery.Organization, delivery.Shipment, delivery.Lease);
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const std::exception& error) {
        LOG_WARNING() << "Route projection delivery deferred: " << error.what();
        deps_.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kRetryRouteProjection,
            delivery.Organization, delivery.Shipment, delivery.Lease);
    }
    return true;
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
