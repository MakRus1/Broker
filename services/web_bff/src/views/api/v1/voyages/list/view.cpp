#include <platform/browser/session.hpp>
#include <views/api/v1/voyages/list/view.hpp>

namespace web_bff::views::api::v1::voyages::list {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto data = deps.shipments.ApiV1VoyagesList({.body = {}}, session.Access);
    Response200 result{};
    for (const auto& row : data.items)
        result.items.push_back({.id = row.id, .number = row.number});
    return result;
}
}  // namespace web_bff::views::api::v1::voyages::list
