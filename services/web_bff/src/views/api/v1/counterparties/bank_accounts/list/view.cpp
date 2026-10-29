#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/bank_accounts/list/view.hpp>

namespace web_bff::views::api::v1::counterparties::bank_accounts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesBankAccountsList;
    const auto result = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
        api::Request{.body = {.counterparty_id = request.body.counterparty_id}}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::v1::counterparties::bank_accounts::list
