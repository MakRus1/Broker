#include <components/transportation_operations.hpp>
#include <utility>
#include <views/api/v1/transportation_operations/get/view.hpp>

namespace shipments::views::api::v1::transportation_operations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.transportation_operations->Get(request.body, context);
    return Response200{.operation = std::move(result.operation)};
}
}  // namespace shipments::views::api::v1::transportation_operations::get
