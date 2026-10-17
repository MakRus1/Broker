#include <views/validated/post/view.hpp>

namespace contract_service::views::validated::post {

Response View::Handle(Request&& request, userver::server::request::RequestContext&) {
    return Response201{
        .count = request.body.count,
        .count_provided = request.body.WasProvided("count"),
        .labels = request.body.labels,
        .status = status::Success,
    };
}

}  // namespace contract_service::views::validated::post
