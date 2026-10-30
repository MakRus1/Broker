#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/lookup/view.hpp>

namespace counterparties::views::api::v1::counterparties::lookup {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);

    return Response200{.items = deps.dadata.Lookup(request.body.tax_id)};
}
}  // namespace counterparties::views::api::v1::counterparties::lookup
