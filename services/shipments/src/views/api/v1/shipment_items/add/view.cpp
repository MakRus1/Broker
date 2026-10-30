#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <helpers/shipment_card.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace shipments::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;

    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(identity);
    const auto replay =
        FindReplay<models::ShipmentItem>(transaction, organization, "add_item", request.body);
    if (replay.has_value()) {
        transaction.Commit();
        return Response201{.item = replay.value()};
    }
    const auto shipment =
        transaction.Execute(sql::kLockShipment, organization, request.body.shipment_id);
    if (shipment.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    CheckRevision(
        shipment.AsSingleRow<pgtypes::ShipmentV4>().Revision, request.body.expected_revision);
    const auto settings = GetShipmentSettings(deps, organization,
        boost::uuids::to_string(shipment.AsSingleRow<pgtypes::ShipmentV4>().CustomerId));
    if (!settings.PlannedContainers)
        throw ApiError<400>{
            "planned_containers_disabled", "Плановые контейнеры отключены для организации"};
    if (std::find(settings.ContainerTypes.begin(), settings.ContainerTypes.end(),
            request.body.container_type_code) == settings.ContainerTypes.end())
        throw ApiError<400>{"invalid_container_type", "Тип контейнера недоступен для организации"};
    const auto row =
        transaction
            .Execute(sql::kAddItem, organization, request.body.shipment_id,
                request.body.container_type_code,
                models::EnumValue(request.body.container_ownership), request.body.planned_quantity)
            .AsSingleRow<pgtypes::ShipmentItemV1>();
    transaction.Execute(sql::kBumpShipmentRevision, organization, request.body.shipment_id);
    const auto result = Convert(row);
    SaveReplay(transaction, organization, "add_item", request.body, result);
    WriteAudit(transaction, organization, identity, "add_item", result.id, request.body, result);
    transaction.Commit();
    return Response201{.item = result};
}
}  // namespace shipments::views::api::v1::shipment_items::add
