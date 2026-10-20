#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    if (request.body.shipment_id)
        body["shipment_id"] = *request.body.shipment_id;
    if (request.body.item_id)
        body["item_id"] = *request.body.item_id;
    body["limit"] = request.body.limit;
    if (request.body.after)
        body["after"] = *request.body.after;
    try {
        const auto result = ::transportations::Store{deps.pg_cluster}.ListTransportations(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        std::vector<::transportations::models::Transportation> items;
        for (const auto& item : result["items"])
            items.push_back(::transportations::models::ParseModelJson(
                item, std::type_identity<::transportations::models::Transportation>{}));
        return Response200{.items = std::move(items)};
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
}  // namespace transportations::views::api::v1::transportations::list
