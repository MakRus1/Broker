#include <boost/uuid/string_generator.hpp>
#include <components/transportation_dispatcher.hpp>
#include <helpers/outbox.hpp>
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
    auto& outbox = context.FindComponent<platform::outbox::Component>();
    outbox.RegisterHandler(
        std::string{helpers::kCreateTransportations},
        [this](const auto& message) { return SendOperation(message); },
        [](pg::Transaction& transaction, const platform::outbox::Message& message,
            const pgtypes::CreationOperationV1& result) {
            const auto organization = message.Payload["organization_id"].As<std::string>();
            transaction.Execute(sql::kCompleteOperation, organization, result);
        });
    outbox.RegisterHandler(std::string{helpers::kProjectShipmentRoute},
        [this](const auto& message) { SendRouteProjection(message); });
}

pgtypes::CreationOperationV1 TransportationDispatcher::SendOperation(
    const platform::outbox::Message& message) {
    namespace api = clients::transportations::ApiInternalV1TransportationsEnsureCreated;
    const auto request = api::Request{.body = api::ParseBody(message.Payload)};
    const auto result = deps_.transportations.ApiInternalV1TransportationsEnsureCreated(
        request, tokens_.GetAccessToken());
    const auto& value = result.operation;
    // Ответ обязан относиться к той же операции и количеству, иначе резерв сохраняется.
    if (value.id != request.body.operation_id || value.item_id != request.body.item_id ||
        value.quantity != request.body.quantity)
        throw std::runtime_error("Несогласованный результат операции");
    using Status = clients::transportations::models::OperationStatus;
    if (value.status != Status::Succeeded && value.status != Status::Rejected)
        throw std::runtime_error("Операция не завершена");
    if (value.status == Status::Succeeded &&
        value.transportation_ids.size() != static_cast<std::size_t>(value.quantity))
        throw std::runtime_error("Несогласованное количество перевозок");
    const std::set<std::string> unique_ids(
        value.transportation_ids.begin(), value.transportation_ids.end());
    if (unique_ids.size() != value.transportation_ids.size())
        throw std::runtime_error("Повторяющиеся ID в результате операции");
    const auto document = json::ValueBuilder(value).ExtractValue();
    return pgtypes::CreationOperationV1{
        .Id = boost::uuids::string_generator{}(request.body.operation_id),
        .ItemId = boost::uuids::string_generator{}(value.item_id),
        .Quantity = value.quantity,
        .Status = clients::transportations::models::EnumValue(value.status),
        .Result = document};
}

void TransportationDispatcher::SendRouteProjection(const platform::outbox::Message& message) {
    namespace api = clients::transportations::ApiInternalV1ShipmentsRouteProjection;
    deps_.transportations.ApiInternalV1ShipmentsRouteProjection(
        api::Request{.body = api::ParseBody(message.Payload)}, tokens_.GetAccessToken());
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
