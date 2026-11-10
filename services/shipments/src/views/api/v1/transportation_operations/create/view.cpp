#include <components/transportation_operations.hpp>
#include <utility>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportation_operations->Create(request.body, context);
    return Response202{.operation = std::move(result.operation)};
}
}  // namespace shipments::views::api::v1::transportation_operations::create
