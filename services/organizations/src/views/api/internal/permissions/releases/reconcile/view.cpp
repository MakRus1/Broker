#include <components/permission_releases.hpp>
#include <utility>
#include <views/api/internal/permissions/releases/reconcile/view.hpp>

namespace organizations::views::api::internal::permissions::releases::reconcile {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.permission_releases->Reconcile(request.body, context);
    return Response200{.success = std::move(result.success)};
}
}  // namespace organizations::views::api::internal::permissions::releases::reconcile
