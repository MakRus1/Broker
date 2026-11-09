#include <components/access.hpp>
#include <utility>
#include <views/api/admin/platform_access/get/view.hpp>

namespace organizations::views::api::admin::platform_access::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->GetPlatform(request.body, context);
    return Response200{.access = std::move(result.access), .catalog = std::move(result.catalog)};
}
}  // namespace organizations::views::api::admin::platform_access::get
