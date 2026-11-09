#include <components/bank_accounts.hpp>
#include <utility>
#include <views/api/v1/counterparties/bank_accounts/save/view.hpp>

namespace counterparties::views::api::v1::counterparties::bank_accounts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.bank_accounts->Save(request.body, context);
    return Response200{.account = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::bank_accounts::save
