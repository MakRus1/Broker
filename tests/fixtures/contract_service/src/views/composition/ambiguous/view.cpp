#include <views/composition/ambiguous/view.hpp>

namespace contract_service::views::composition::ambiguous {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.value = std::move(request.body)};
}
}  // namespace contract_service::views::composition::ambiguous
