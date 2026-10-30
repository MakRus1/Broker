#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/financial_labels/list/view.hpp>

namespace web_bff::views::api::v1::financial_labels::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::finance::ApiV1FinancialLabelsList;
    const auto result = deps.finance.ApiV1FinancialLabelsList(
        api::Request{.body = Convert(request.body)}, session.Access);
    Response200 response{.items = Convert(result.items)};
    if (result.next_cursor.has_value())
        response.next_cursor =
            models::finance::FinancialLabelRegistryCursor{.id = result.next_cursor.value().id,
                .scope = result.next_cursor.value().scope,
                .value = result.next_cursor.value().value};
    return response;
}
}  // namespace web_bff::views::api::v1::financial_labels::list
