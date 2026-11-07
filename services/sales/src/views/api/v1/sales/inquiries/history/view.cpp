#include <helpers/settings.hpp>
#include <views/api/v1/sales/inquiries/history/view.hpp>

namespace sales::views::api::v1::sales::inquiries::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    return Response200{
        .items = ReadList<models::InquiryHistoryEntry>(
            deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryInquiries,
                Organization(context), request.body.id, request.body.after_revision.value_or(0)))};
}
}  // namespace sales::views::api::v1::sales::inquiries::history
