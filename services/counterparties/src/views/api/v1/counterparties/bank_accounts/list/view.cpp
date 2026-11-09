#include <components/bank_accounts.hpp>
#include <utility>
#include <views/api/v1/counterparties/bank_accounts/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::bank_accounts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.bank_accounts->List(request.body, context);
    return Response200{.items = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::bank_accounts::list
