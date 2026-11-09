#include <components/contracts.hpp>
#include <utility>
#include <views/api/v1/contracts/get/view.hpp>

namespace contracts::views::api::v1::contracts::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.documents->Get(request.body.id, context);
    return Response200{.contract = std::move(result)};
}
}  // namespace contracts::views::api::v1::contracts::get
