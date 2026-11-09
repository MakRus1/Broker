#include <components/access.hpp>
#include <utility>
#include <views/api/organizations/employees/list/view.hpp>

namespace organizations::views::api::organizations::employees::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->Employees(request.body, context);
    return Response200{.items = std::move(result.items)};
}
}  // namespace organizations::views::api::organizations::employees::list
