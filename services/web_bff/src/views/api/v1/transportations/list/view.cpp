#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/list/view.hpp>

namespace web_bff::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1TransportationsList;
    const auto result = deps.transportations.ApiV1TransportationsList(
        api::Request{.body = Convert(request.body)}, session.Access);
    std::optional<models::transportations::RegistryCursor> cursor;
    if (result.next_cursor.has_value())
        cursor = models::transportations::RegistryCursor{.id = result.next_cursor.value().id,
            .scope = result.next_cursor.value().scope,
            .value = result.next_cursor.value().value};
    return Response200{.items = Convert(result.items), .next_cursor = cursor};
}
}  // namespace web_bff::views::api::v1::transportations::list
