#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/details.hpp>
#include <helpers/inherited_route.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/create/view.hpp>

namespace transportations::views::api::v1::transportations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        const auto organization = platform::auth::GetOrganization(identity);
        ValidateDetails(request.body.type, request.body.container_type_code,
            request.body.container_ownership, request.body.container_number);
        const auto sources = LoadShipmentContexts(deps, context, {request.body.shipment_id});
        const auto& source = FindShipmentContext(sources, request.body.shipment_id);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        transaction.Execute(sql::kLockCommand, organization, request.body.id);
        const auto saved = transaction.Execute(sql::kFindCommand, organization, request.body.id);
        if (!saved.IsEmpty()) {
            const auto command = saved.AsSingleRow<pgtypes::SavedCommandV1>();
            if (command.Payload != Document(request.body))
                throw ApiError{409, "idempotency_conflict", "Содержимое команды создания изменено"};
            // Снимки старых команд содержат технический номер, которого больше нет в API.
            userver::formats::json::ValueBuilder document{command.Result};
            document.Remove("number");
            auto result = models::ParseModelJson(
                document.ExtractValue(), std::type_identity<models::Transportation>{});
            transaction.Commit();
            InheritRoute(result, source);
            return Response200{.transportation = result};
        }
        auto result =
            Convert(transaction
                        .Execute(sql::kCreateManualTransportation, organization, request.body.id,
                            request.body.shipment_id, models::EnumValue(request.body.type),
                            request.body.hazardous, request.body.container_type_code,
                            Ownership(request.body.container_ownership),
                            CleanContainerNumber(request.body.container_number))
                        .AsSingleRow<pgtypes::TransportationV4>());
        InheritRoute(result, source);
        transaction.Execute(sql::kInsertCommand, organization, request.body.id,
            Document(request.body), Document(result));
        WriteAudit(transaction, organization, identity, "create_transportation", result.id,
            request.body, result);
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
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportations::create
