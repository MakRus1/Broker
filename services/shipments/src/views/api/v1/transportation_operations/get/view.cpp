#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportation_operations/get/view.hpp>

namespace shipments::views::api::v1::transportation_operations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto tenant = ResolveTenant(transaction, principal);
        const auto rows =
            transaction.Execute(sql::kGetOperation, tenant, request.body.operation_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Операция не найдена"};
        const auto result = Convert(rows.AsSingleRow<pgtypes::CreationOperationV1>());
        transaction.Commit();
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
