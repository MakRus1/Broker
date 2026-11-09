#include <components/permission_releases.hpp>
#include <utility>
#include <views/api/internal/permissions/releases/upsert/view.hpp>

namespace organizations::views::api::internal::permissions::releases::upsert {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.permission_releases->Upsert(request.body, context);
    return Response200{.success = std::move(result.success)};
}
}  // namespace organizations::views::api::internal::permissions::releases::upsert
