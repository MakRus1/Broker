#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::shipments::models::CreateOperationRequest command{
        .expected_revision = request.body.expected_revision,
        .idempotency_key = request.body.idempotency_key,
        .item_id = request.body.item_id,
        .quantity = request.body.quantity};
    try {
        return Response202{.operation = ::shipments::Store{deps.pg_cluster}.CreateOperation(
                               platform::auth::GetPrincipal(context), command)};
    } catch (const ::shipments::ApiError& error) {
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
}  // namespace shipments::views::api::v1::transportation_operations::create
