#include <views/validated/get/view.hpp>

namespace contract_service::views::validated::get {

Response View::Handle(Request&& request, userver::server::request::RequestContext&) {
    return Response200{
        .limit = request.limit,
        .provided = request.WasProvided("limit"),
        .status = request.status,
    };
}

}  // namespace contract_service::views::validated::get
