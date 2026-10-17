#include <views/validated/create/view.hpp>

namespace contract_service::views::validated::create {

Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps) {
    return Response201{
        .count = request.body.count,
        .count_provided = request.body.WasProvided("count"),
        .labels = request.body.labels,
        .status = status::Success,
    };
}

}  // namespace contract_service::views::validated::create
