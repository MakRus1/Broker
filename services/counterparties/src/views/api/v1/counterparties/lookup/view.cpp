#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/lookup/view.hpp>

namespace counterparties::views::api::v1::counterparties::lookup {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    try {
        return Response200{.items = deps.dadata.Lookup(request.body.tax_id)};
    } catch (const ::counterparties::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            throw;
        }
    }
}
}  // namespace counterparties::views::api::v1::counterparties::lookup
