#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportation_operations/get/view.hpp>

namespace shipments::views::api::v1::transportation_operations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal, std::nullopt, kReadSnapshot};
        const auto rows =
            session.Tx.Execute(sql::kGetOperation, session.Tenant, request.body.operation_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Операция не найдена"};
        const auto result = ToModel(rows.AsSingleRow<OperationRow>(pg::kRowTag));
        session.Tx.Commit();
        return Response200{.operation = result};
    } catch (const ::shipments::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace shipments::views::api::v1::transportation_operations::get
