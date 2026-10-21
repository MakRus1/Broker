#include <helpers/transportations.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportations/get/view.hpp>

namespace transportations::views::api::v1::transportations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        // Перевозка и её плечи читаются из одного снимка.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto tenant = ResolveTenant(transaction, principal);
        const auto result =
            ReadTransportation(transaction, tenant, request.body.transportation_id, false);
        transaction.Commit();
        return Response200{.transportation = result};
    } catch (const ::transportations::ApiError& error) {
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
}  // namespace transportations::views::api::v1::transportations::get
