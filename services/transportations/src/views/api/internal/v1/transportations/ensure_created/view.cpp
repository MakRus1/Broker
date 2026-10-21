#include <helpers/transportations.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto tenant = ResolveTenant(transaction, principal, request.body.tenant_id);
        transaction.Execute(sql::kLockCommand, tenant, request.body.operation_id);
        const auto rows = transaction.Execute(sql::kFindCommand, tenant, request.body.operation_id);
        if (!rows.IsEmpty()) {
            const auto saved = rows.AsSingleRow<pgtypes::SavedCommandV1>();
            if (saved.Payload != Document(request.body))
                throw ApiError{409, "protocol_conflict", "Содержимое операции изменено"};
            const auto result = models::ParseModelJson(
                saved.Result, std::type_identity<models::CreationOperation>{});
            transaction.Commit();
            return Response200{.operation = result};
        }

        models::CreationOperation result;
        result.id = request.body.operation_id;
        result.item_id = request.body.item_id;
        result.quantity = request.body.quantity;
        result.status = models::OperationStatus::Pending;
        transaction.Execute(sql::kInsertCommand, tenant, request.body.operation_id,
            Document(request.body), Document(result));
        for (std::int64_t ordinal = 1; ordinal <= request.body.quantity; ++ordinal) {
            result.transportation_ids.push_back(
                transaction
                    .Execute(sql::kCreateTransportation, tenant, request.body.shipment_id,
                        request.body.item_id, request.body.operation_id, ordinal,
                        request.body.container_type_code,
                        models::EnumValue(request.body.container_ownership))
                    .AsSingleRow<std::string>());
        }
        result.status = models::OperationStatus::Succeeded;
        transaction.Execute(
            sql::kCompleteCommand, tenant, request.body.operation_id, Document(result));
        WriteAudit(
            transaction, tenant, principal, "ensure_created", result.id, request.body, result);
        transaction.Commit();
        return Response200{.operation = result};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::internal::v1::transportations::ensure_created
