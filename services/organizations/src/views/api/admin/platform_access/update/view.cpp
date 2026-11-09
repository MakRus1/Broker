#include <components/access.hpp>
#include <utility>
#include <views/api/admin/platform_access/update/view.hpp>

namespace organizations::views::api::admin::platform_access::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->UpdatePlatform(request.body, context);
    return Response200{.access = std::move(result.access), .catalog = std::move(result.catalog)};
}
}  // namespace organizations::views::api::admin::platform_access::update
