#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/employees.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/shipment_card.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto saved =
            FindSavedReplay<models::Shipment>(deps, organization, "create_shipment", request.body);
        if (saved.has_value())
            return Response201{.shipment = saved.value()};
        const auto settings = GetShipmentSettings(deps, organization, request.body.customer_id);
        const auto logist = request.body.logist.has_value() ? CleanText(request.body.logist.value())
                                                            : settings.DefaultLogist;
        ValidateLogist(deps, context, logist);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto replay = FindReplay<models::Shipment>(
            transaction, organization, "create_shipment", request.body);
        if (replay.has_value()) {
            transaction.Commit();
            return Response201{.shipment = replay.value()};
        }
        pgtypes::ShipmentDataV1 card;
        card.CustomerId = boost::uuids::string_generator{}(request.body.customer_id);
        card.Reference = request.body.reference.value_or("");
        card.ShipmentDate = Today();
        card.Status = settings.Statuses.front();
        card.Logist = settings.DefaultLogist;
        ApplyCard(request.body, card);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<pgtypes::ShipmentDictionaryValueV1>>();
        ValidateCard(card, settings, dictionaries);
        card.Number = request.body.number.has_value()
            ? userver::utils::text::Trim(request.body.number.value())
            : GenerateShipmentNumber(transaction, organization, card.ShipmentDate, settings);
        if (card.Number.empty())
            throw ApiError{400, "invalid_number", "Укажите номер поставки"};
        const auto row = transaction.Execute(sql::kCreateShipment, organization, card)
                             .AsSingleRow<pgtypes::ShipmentV2>();
        RememberCargo(transaction, organization, row.CargoDescription);
        const auto result = Convert(row);
        SaveReplay(transaction, organization, "create_shipment", request.body, result);
        WriteAudit(transaction, organization, identity, "create_shipment", result.id, request.body,
            result);
        transaction.Commit();
        return Response201{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "number_exists", .message = "Номер поставки уже занят"};
    } catch (const ::shipments::ApiError& error) {
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
}  // namespace shipments::views::api::v1::shipments::create
