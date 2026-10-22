#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <helpers/shipment_card.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization = platform::auth::GetOrganization(identity);
        const auto current =
            transaction.Execute(sql::kLockShipment, organization, request.body.shipment_id);
        if (current.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto previous = current.AsSingleRow<pgtypes::ShipmentV2>();
        CheckRevision(previous.Revision, request.body.expected_revision);
        auto card = ConvertData(previous);
        card.Reference = request.body.reference;
        if (request.body.customer_id.has_value())
            card.CustomerId = boost::uuids::string_generator{}(request.body.customer_id.value());
        if (request.body.number.has_value()) {
            card.Number = userver::utils::text::Trim(request.body.number.value());
            if (card.Number.empty())
                throw ApiError{400, "invalid_number", "Укажите номер поставки"};
        }
        ApplyCard(request.body, card);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<pgtypes::ShipmentDictionaryValueV1>>();
        ValidateCard(card,
            GetShipmentSettings(deps, organization, boost::uuids::to_string(card.CustomerId)),
            dictionaries, previous);
        const auto result =
            Convert(transaction
                        .Execute(sql::kUpdateShipment, organization, request.body.shipment_id,
                            request.body.expected_revision, card)
                        .AsSingleRow<pgtypes::ShipmentV2>());
        RememberCargo(transaction, organization, card.CargoDescription);
        WriteAudit(transaction, organization, identity, "update_shipment", result.id, request.body,
            result);
        transaction.Commit();
        return Response200{.shipment = result};
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
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace shipments::views::api::v1::shipments::update
