#include <components/catalog.hpp>
#include <views/localization/catalog/view.hpp>

namespace localization::views::localization::catalog {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.messages = deps.catalog->GetPublicCatalog(EnumValue(request.locale))};
}
}  // namespace localization::views::localization::catalog
