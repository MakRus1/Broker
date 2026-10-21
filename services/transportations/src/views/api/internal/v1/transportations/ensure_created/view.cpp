#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/internal/v1/transportations/ensure_created/view.hpp>

namespace transportations::views::api::internal::v1::transportations::ensure_created {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal, request.body.tenant_id};
        session.Tx.Execute(sql::kLockCommand, session.Tenant, request.body.operation_id);
        const auto rows =
            session.Tx.Execute(sql::kFindCommand, session.Tenant, request.body.operation_id);
        if (!rows.IsEmpty()) {
            const auto saved = rows.AsSingleRow<SavedCommand>(pg::kRowTag);
            if (saved.Payload != Document(request.body))
                throw ApiError{409, "protocol_conflict", "Содержимое операции изменено"};
            const auto result = models::ParseModelJson(
                saved.Result, std::type_identity<models::CreationOperation>{});
            session.Tx.Commit();
            return Response200{.operation = result};
        }

        models::CreationOperation result;
        result.id = request.body.operation_id;
        result.item_id = request.body.item_id;
        result.quantity = request.body.quantity;
        result.status = models::OperationStatus::Pending;
        session.Tx.Execute(sql::kInsertCommand, session.Tenant, request.body.operation_id,
            Document(request.body), Document(result));
        for (std::int64_t ordinal = 1; ordinal <= request.body.quantity; ++ordinal) {
            result.transportation_ids.push_back(
                session.Tx
                    .Execute(sql::kCreateTransportation, session.Tenant, request.body.shipment_id,
                        request.body.item_id, request.body.operation_id, ordinal,
                        request.body.container_type_code,
                        models::EnumValue(request.body.container_ownership))
                    .AsSingleRow<std::string>());
        }
        result.status = models::OperationStatus::Succeeded;
        session.Tx.Execute(
            sql::kCompleteCommand, session.Tenant, request.body.operation_id, Document(result));
        session.Audit("ensure_created", result.id, request.body, result);
        session.Tx.Commit();
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
