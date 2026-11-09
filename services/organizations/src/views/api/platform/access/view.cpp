#include <components/access.hpp>
#include <utility>
#include <views/api/platform/access/view.hpp>

namespace organizations::views::api::platform::access {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->ResolvePlatform(request.body, context);
    return Response200{
        .permissions = std::move(result.permissions), .revision = std::move(result.revision)};
}
}  // namespace organizations::views::api::platform::access
