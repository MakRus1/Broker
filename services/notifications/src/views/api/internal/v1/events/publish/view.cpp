#include <events/catalog.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/internal/v1/events/publish/view.hpp>

namespace notifications::views::api::internal::v1::events::publish {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    ::notifications::events::Catalog().Accept(
        deps.pg_cluster, platform::auth::GetIdentity(context), request.body);
    return Response200{.accepted = true};
}
}  // namespace notifications::views::api::internal::v1::events::publish
