#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["item_id"] = request.body.item_id;
    body["expected_revision"] = request.body.expected_revision;
    body["quantity"] = request.body.quantity;
    body["idempotency_key"] = request.body.idempotency_key;
    try {
        const auto result = ::shipments::Store{deps.pg_cluster}.CreateOperation(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        return Response202{.operation = ::shipments::models::ParseModelJson(result,
                               std::type_identity<::shipments::models::CreationOperation>{})};
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
