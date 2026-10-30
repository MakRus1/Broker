#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <clients/transportations/client.hpp>
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/outbox.hpp>
#include <helpers/revision.hpp>
#include <operations/request_transportations.hpp>
#include <platform/outbox/component.hpp>

namespace shipments::operations {
models::CreationOperation RequestTransportations(
    const userver::storages::postgres::ClusterPtr& cluster,
    const platform::auth::Identity& identity, const models::CreateOperationRequest& request) {
    using namespace ::shipments::helpers;

    auto transaction = cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
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
}  // namespace shipments::operations
