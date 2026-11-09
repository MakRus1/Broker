#include <components/contracts.hpp>
#include <utility>
#include <views/api/v1/contracts/save/view.hpp>

namespace contracts::views::api::v1::contracts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.documents->Save(request.body, context);
    return Response200{.contract = std::move(result)};
}
}  // namespace contracts::views::api::v1::contracts::save
