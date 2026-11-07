#pragma once
#include <helpers/converters.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/route_notifications.hpp>
#include <helpers/route_projection.hpp>
#include <helpers/shipment_card.hpp>
#include <tuple>

namespace shipments::helpers {
template <typename Request>
models::Shipment InsertShipment(pg::Transaction& transaction, const std::string& organization,
    const Request& body, const auto& settings, const std::optional<std::string>& logist) {
    pgtypes::ShipmentDataV3 card;
    card.CustomerId = boost::uuids::string_generator{}(body.customer_id);
    card.Reference = body.reference.value_or("");
    card.ShipmentDate = Today();
    card.Status = settings.Statuses.front();
    card.Logist = logist;
    ApplyCard(body, card);
    const auto dictionaries = transaction.Execute(sql::kListShipmentDictionaries)
                                  .AsContainer<std::vector<pgtypes::ShipmentDictionaryValueV1>>();
    ValidateCard(card, settings, dictionaries);
    card.Number = body.number.has_value()
        ? userver::utils::text::Trim(body.number.value())
        : GenerateShipmentNumber(transaction, organization, card.ShipmentDate, settings);
    if (card.Number.empty())
        throw ApiError<400>{"invalid_number", "Укажите номер поставки"};
    const auto row = transaction.Execute(sql::kCreateShipment, organization, card)
                         .AsSingleRow<pgtypes::ShipmentV4>();
    RememberCargo(transaction, organization, row.CustomerId, row.CargoDescription);
    const auto result = Convert(row);
    return result;
}

struct ShipmentSource {
    std::string Producer;
    std::string Type;
    std::string Id;
};

inline std::optional<models::Shipment> FindCreatedShipment(const Deps& deps,
    const std::string& organization, const ShipmentSource& source, const Json& document) {
    const auto saved = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kFindSourceShipment, organization, source.Producer, source.Type, source.Id);
    if (saved.IsEmpty())
        return std::nullopt;
    const auto [id, original] = saved.AsSingleRow<std::tuple<std::string, Json>>(pg::kRowTag);
    if (original != document)
        throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
    return Convert(
        deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetShipment, organization, id)
            .AsSingleRow<pgtypes::ShipmentV4>());
}

// Один сценарий для ручного создания и команд доверенных сервисов.
// Источник участвует только в дедупликации и связи, не выбирает бизнес-ветку.
inline models::Shipment CreateShipment(const Deps& deps, const std::string& organization,
    const platform::auth::Identity& actor, const models::CreateShipmentRequest& body,
    const ShipmentSource& source, const Json& document) {
    if (const auto saved = FindCreatedShipment(deps, organization, source, document))
        return *saved;
    const auto settings = GetShipmentSettings(deps, organization, body.customer_id);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(
        sql::kLockIdempotency, organization, source.Producer + ":" + source.Type, source.Id);
    const auto saved = transaction.Execute(
        sql::kFindSourceShipment, organization, source.Producer, source.Type, source.Id);
    if (!saved.IsEmpty()) {
        const auto [id, original] = saved.AsSingleRow<std::tuple<std::string, Json>>(pg::kRowTag);
        if (original != document)
            throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
        const auto shipment = Convert(transaction.Execute(sql::kGetShipment, organization, id)
                                          .AsSingleRow<pgtypes::ShipmentV4>());
        transaction.Commit();
        return shipment;
    }
    const auto result = InsertShipment(
        transaction, organization, body, settings, body.logist.value_or(std::nullopt));
    transaction.Execute(sql::kLinkSourceShipment, organization, source.Producer, source.Type,
        source.Id, result.id, document);
    WriteAudit(transaction, organization, actor, "create_shipment", result.id, body, result);
    QueueRouteProjection(transaction, organization, result);
    QueueShipmentCreated(transaction, organization, actor, result);
    transaction.Commit();
    return result;
}
}  // namespace shipments::helpers
