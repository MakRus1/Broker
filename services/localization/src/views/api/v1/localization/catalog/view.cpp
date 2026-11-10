#include <components/catalog.hpp>
#include <views/api/v1/localization/catalog/view.hpp>

namespace localization::views::api::v1::localization::catalog {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    return Response200{.messages = deps.catalog->Get(request.body, context)};
}
}  // namespace localization::views::api::v1::localization::catalog
