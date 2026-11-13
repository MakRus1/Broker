#include <components/sources.hpp>
#include <views/api/internal/localization/sources/upsert/view.hpp>

namespace localization::views::api::internal::localization::sources::upsert {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.registered = deps.sources->RegisterSourceCatalog(request.body)};
}
}  // namespace localization::views::api::internal::localization::sources::upsert
