#include <models/errors.hpp>
#include <operations/create_transportations.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    try {
        const auto result = ::shipments::operations::CreateTransportations(
            deps.pg_cluster, platform::auth::GetPrincipal(context), request.body);
        return Response202{.operation = result};
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
