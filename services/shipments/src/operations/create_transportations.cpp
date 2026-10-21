#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <clients/transportations/client.hpp>
#include <helpers/rows.hpp>
#include <operations/create_transportations.hpp>

namespace shipments::operations {
models::CreationOperation CreateTransportations(
    const userver::storages::postgres::ClusterPtr& cluster,
    const platform::auth::Principal& principal, const models::CreateOperationRequest& request) {
    using namespace ::shipments::helpers;

    Session session{cluster, principal};
    const auto replay = FindReplay<models::CreationOperation>(session, "create_operation", request);
    if (replay.has_value()) {
        session.Tx.Commit();
        return replay.value();
    }
    // Эта блокировка сериализует резервирование и изменение плана только одной позиции.
    const auto rows = session.Tx.Execute(sql::kLockItem, session.Tenant, request.item_id);
    if (rows.IsEmpty())
        throw ApiError{404, "not_found", "Позиция не найдена"};
    const auto item = rows.AsSingleRow<ItemRow>(pg::kRowTag);
    CheckRevision(item.Revision, request.expected_revision);
    const auto totals = ReadTotals(session, request.item_id);
    if (totals.Created + totals.Pending + request.quantity > item.Planned)
        throw ApiError{409, "quantity_exceeded", "Превышено плановое количество"};

    models::CreationOperation result;
    result.id = boost::uuids::to_string(boost::uuids::random_generator{}());
    result.item_id = item.Id;
    result.quantity = request.quantity;
    result.status = models::OperationStatus::Pending;

    namespace api = clients::transportations::ApiInternalV1TransportationsEnsureCreated;
    api::RequestBody payload;
    payload.operation_id = result.id;
    payload.tenant_id = session.Tenant;
    payload.shipment_id = item.ShipmentId;
    payload.item_id = item.Id;
    payload.quantity = request.quantity;
    payload.schema_version =
        clients::transportations::models::EnsureCreatedRequest_schema_version::Value1;
    payload.container_type_code = item.ContainerType;
    payload.container_ownership = clients::transportations::models::ParseEnumScalar(
        item.Ownership, clients::transportations::models::ContainerOwnership{});

    // Payload сохраняется один раз и не пересобирается из изменяемой позиции при повторах.
    const auto document = userver::formats::json::FromString(api::ToJson(payload));
    session.Tx.Execute(
        sql::kCreateOperation, session.Tenant, result.id, item.Id, request.quantity, document);
    SaveReplay(session, "create_operation", request, result);
    session.Audit("create_operation", result.id, request, result);
    session.Tx.Commit();
    return result;
}
}  // namespace shipments::operations
