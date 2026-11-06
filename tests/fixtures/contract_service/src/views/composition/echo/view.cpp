#include <views/composition/echo/view.hpp>

namespace contract_service::views::composition::echo {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{.value = std::move(request.body)};
}
}  // namespace contract_service::views::composition::echo
