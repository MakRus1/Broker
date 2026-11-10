#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <clients/transportations/client.hpp>
#include <components/transportation_operations.hpp>
#include <converters/shipments.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/outbox.hpp>
#include <helpers/revision.hpp>
#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/outbox/component.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
using namespace helpers;
using namespace converters;

TransportationOperationsComponent::TransportationOperationsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()) {
}

userver::yaml_config::Schema TransportationOperationsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::TransportationOperationsCreateResult TransportationOperationsComponent::Create(
    const models::CreateOperationRequest& request,
    userver::server::request::RequestContext& context) const {

    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto result = RequestTransportations(platform::auth::GetIdentity(context), request);
    return models::TransportationOperationsCreateResult{.operation = result};
}

models::TransportationOperationsGetResult TransportationOperationsComponent::Get(
    const models::GetOperationRequest& request,
    userver::server::request::RequestContext& context) const {

    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetOperation, organization, request.operation_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Операция не найдена"};
    const auto result = Convert(rows.AsSingleRow<pgtypes::CreationOperationV1>());
    return models::TransportationOperationsGetResult{.operation = result};
}

models::CreationOperation TransportationOperationsComponent::RequestTransportations(
    const platform::auth::Identity& identity, const models::CreateOperationRequest& request) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(identity);
    const auto replay = FindReplay<models::CreationOperation>(
        transaction, organization, "create_operation", request);
    if (replay.has_value()) {
        transaction.Commit();
        return replay.value();
    }
    // Блокировка защищает снимок позиции при создании операции.
    const auto rows = transaction.Execute(sql::kLockItem, organization, request.item_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Позиция не найдена"};
    const auto item = rows.AsSingleRow<pgtypes::ShipmentItemV1>();
    CheckRevision(item.Revision, request.expected_revision);

    models::CreationOperation result;
    result.id = boost::uuids::to_string(boost::uuids::random_generator{}());
    result.item_id = boost::uuids::to_string(item.Id);
    result.quantity = request.quantity;
    result.status = models::OperationStatus::Pending;

    namespace api = clients::transportations::ApiInternalV1TransportationsEnsureCreated;
    api::RequestBody payload;
    payload.operation_id = result.id;
    payload.organization_id = organization;
    payload.shipment_id = boost::uuids::to_string(item.ShipmentId);
    payload.item_id = boost::uuids::to_string(item.Id);
    payload.quantity = request.quantity;
    payload.schema_version =
        clients::transportations::models::EnsureCreatedRequest_schema_version::Value1;
    payload.container_type_code = item.ContainerType;
    payload.container_ownership = clients::transportations::models::ParseEnumScalar(
        item.Ownership, clients::transportations::models::ContainerOwnership{});

    // Payload сохраняется один раз и не пересобирается из изменяемой позиции при повторах.
    const auto document = userver::formats::json::FromString(api::ToJson(payload));
    transaction.Execute(sql::kCreateOperation, organization, result.id, item.Id, request.quantity);
    platform::outbox::Enqueue(transaction, kCreateTransportations, result.id, document);
    SaveReplay(transaction, organization, "create_operation", request, result);
    WriteAudit(transaction, organization, identity, "create_operation", result.id, request, result);
    transaction.Commit();
    return result;
}

}  // namespace shipments
