#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/contracts/list/view.hpp>

namespace web_bff::views::api::v1::contracts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::contracts::ApiV1ContractsList;
    const auto result = deps.contracts.ApiV1ContractsList(
        api::Request{.body = Convert(request.body)}, session.Access);
    Response200 response{.items = Convert(result.items)};
    if (result.next_cursor.has_value())
        response.next_cursor = models::contracts::ContractRegistryCursor{
            .date_value = result.next_cursor.value().date_value,
            .id = result.next_cursor.value().id,
            .scope = result.next_cursor.value().scope,
            .text_value = result.next_cursor.value().text_value};
    return response;
}
}  // namespace web_bff::views::api::v1::contracts::list
