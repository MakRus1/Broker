#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/bank_accounts/save/view.hpp>

namespace web_bff::views::api::v1::counterparties::bank_accounts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesBankAccountsSave;
    const auto result = deps.counterparties.ApiV1CounterpartiesBankAccountsSave(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.account = Convert(result.account)};
}
}  // namespace web_bff::views::api::v1::counterparties::bank_accounts::save
