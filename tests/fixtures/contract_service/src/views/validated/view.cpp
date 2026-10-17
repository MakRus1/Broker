#include <views/validated/view.hpp>

namespace contract_service::views::validated {

Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response200{
        .limit = request.limit,
        .provided = request.WasProvided("limit"),
        .status = request.status,
    };
}

}  // namespace contract_service::views::validated
