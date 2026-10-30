#include <helpers/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;

    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_CREATE);
    const auto& identity = platform::auth::GetIdentity(context);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = request.body.organization_id;
    transaction.Execute(sql::kLockCommand, organization, request.body.operation_id);
    const auto rows =
        transaction.Execute(sql::kFindCommand, organization, request.body.operation_id);
    if (!rows.IsEmpty()) {
        const auto saved = rows.AsSingleRow<pgtypes::SavedCommandV1>();
        if (saved.Payload != Document(request.body))
            throw ApiError<409>{"protocol_conflict", "Содержимое операции изменено"};
        const auto result =
            models::ParseModelJson(saved.Result, std::type_identity<models::CreationOperation>{});
        transaction.Commit();
        return Response200{.operation = result};
    }

    models::CreationOperation result;
    result.id = request.body.operation_id;
    result.item_id = request.body.item_id;
    result.quantity = request.body.quantity;
    result.status = models::OperationStatus::Pending;
    transaction.Execute(sql::kInsertCommand, organization, request.body.operation_id,
        Document(request.body), Document(result));
    for (std::int64_t ordinal = 1; ordinal <= request.body.quantity; ++ordinal) {
        result.transportation_ids.push_back(
            transaction
                .Execute(sql::kCreateTransportation, organization, request.body.shipment_id,
                    request.body.item_id, request.body.operation_id, ordinal,
                    request.body.container_type_code,
                    models::EnumValue(request.body.container_ownership))
                .AsSingleRow<std::string>());
    }
    result.status = models::OperationStatus::Succeeded;
    transaction.Execute(
        sql::kCompleteCommand, organization, request.body.operation_id, Document(result));
    WriteAudit(
        transaction, organization, identity, "ensure_created", result.id, request.body, result);
    transaction.Commit();
    return Response200{.operation = result};
}
}  // namespace transportations::views::api::internal::v1::transportations::ensure_created
