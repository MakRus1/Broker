#include <boost/uuid/string_generator.hpp>
#include <components/shipments.hpp>
#include <configs/keys.hpp>
#include <converters/shipments.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_notifications.hpp>
#include <helpers/route_projection.hpp>
#include <helpers/shipment_card.hpp>
#include <models/pg/shipment_data.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <models/pg/shipment_raw.hpp>
#include <models/shipment_source_record.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = shipments::models;
namespace sql = shipments::sql;
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
using shipments::ApiError;

std::string GenerateShipmentNumber(pg::Transaction& transaction, const std::string& organization,
    userver::utils::datetime::Date date, const auto& settings) {
    const auto period = platform::DocumentNumberPeriod(date, settings.ResetNumberYearly);
    const auto sequence = transaction.Execute(sql::kNextShipmentNumber, organization, period)
                              .template AsSingleRow<std::int64_t>();
    const auto result = platform::FormatDocumentNumber(settings.NumberTemplate, date, sequence);
    if (result.size() > 128)
        throw ApiError<400>{"invalid_number_template", "Слишком длинный номер поставки"};
    return result;
}

void RememberCargo(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& customer, const std::optional<std::string>& description) {
    if (description.has_value())
        transaction.Execute(sql::kRememberCargo, organization, customer, description.value(),
            userver::utils::text::ToLower(description.value()));
}

models::Shipment InsertShipment(pg::Transaction& transaction, const std::string& organization,
    const models::CreateShipmentRequest& body, const configs::ShipmentSettings& settings) {
    auto card = shipments::helpers::CreateShipmentCard(body, settings);
    const auto dictionaries = transaction.Execute(sql::kListShipmentDictionaries)
                                  .AsContainer<std::vector<models::pg::ShipmentDictionaryValue>>();
    shipments::helpers::ValidateCard(card, settings, dictionaries);

    card.Number = body.number.has_value()
        ? userver::utils::text::Trim(body.number.value())
        : GenerateShipmentNumber(transaction, organization, card.ShipmentDate, settings);
    if (card.Number.empty())
        throw ApiError<400>{"invalid_number", "Укажите номер поставки"};

    const auto row = transaction.Execute(sql::kCreateShipment, organization, card)
                         .AsSingleRow<models::pg::ShipmentRaw>();
    RememberCargo(transaction, organization, row.CustomerId, row.CargoDescription);
    return shipments::converters::Convert(row);
}

std::optional<boost::uuids::uuid> ValidateShipmentReplay(
    const pg::ResultSet& saved, const json::Value& document) {
    if (saved.IsEmpty())
        return std::nullopt;
    const auto stored = saved.AsSingleRow<models::ShipmentSourceRecord>(pg::kRowTag);
    if (stored.Payload != document)
        throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
    return stored.ShipmentId;
}

}  // namespace

namespace shipments {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

ShipmentsComponent::ShipmentsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()),
      settings_(context.FindComponent<SettingsComponent>()),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()) {}

userver::yaml_config::Schema ShipmentsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

std::optional<std::string> ShipmentsComponent::GetCustomerLogist(
    std::string_view access_token, const std::string& customer) const try {
    namespace api = clients::counterparties::ApiV1CounterpartiesGet;
    const auto result = counterparties_.ApiV1CounterpartiesGet(
        api::Request{.body = {.id = customer}}, access_token);

    return result.counterparty.logist.value_or(std::nullopt);
} catch (const platform::http::ErrorByStatus<404>&) {

    throw ApiError<400>{"invalid_customer", "Клиент не найден в организации"};
} catch (const platform::http::ApiError&) {
    throw ApiError<503>{"customer_directory_unavailable", "Не удалось получить карточку клиента"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{"customer_directory_unavailable", "Не удалось получить карточку клиента"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{
        "customer_directory_unavailable", "Некорректный ответ справочника контрагентов"};
}

void ShipmentsComponent::ValidateLogist(const std::string& organization,
    std::string_view access_token, const std::optional<std::string>& subject) const try {
    if (!subject)
        return;

    const auto employees = organizations_.ApiOrganizationsEmployeesList(
        {.body = {.limit = 1, .organization_id = organization, .subject = subject}}, access_token);
    if (employees.items.empty())
        throw ApiError<400>{"invalid_logist", "Выберите действующего сотрудника организации"};
} catch (const platform::http::ApiError&) {

    throw ApiError<503>{"employee_directory_unavailable",
        "Не удалось проверить сотрудника. Повторите сохранение позже"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{"employee_directory_unavailable",
        "Не удалось проверить сотрудника. Повторите сохранение позже"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{
        "employee_directory_unavailable", "Некорректный ответ справочника сотрудников"};
}

std::optional<models::Shipment> ShipmentsComponent::FindCreatedShipment(
    const std::string& organization, const ShipmentSource& source,
    const json::Value& document) const {
    const auto saved = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindSourceShipment,
        organization, source.Producer, source.Type, source.Id);
    const auto shipment_id = ValidateShipmentReplay(saved, document);
    if (!shipment_id.has_value())
        return std::nullopt;
    return converters::Convert(cluster_
                                   ->Execute(pg::ClusterHostType::kMaster, sql::kGetShipment,
                                       organization, shipment_id.value())
                                   .AsSingleRow<models::pg::ShipmentRaw>());
}

// Один сценарий для ручного создания и команд доверенных сервисов.
// Источник участвует только в дедупликации и связи, не выбирает бизнес-ветку.
models::Shipment ShipmentsComponent::CreateShipmentFromSource(const std::string& organization,
    const platform::auth::Identity& actor, const models::CreateShipmentRequest& body,
    const ShipmentSource& source, const json::Value& document) const {
    const auto settings = settings_.Effective(organization, body.customer_id);

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(
        sql::kLockIdempotency, organization, source.Producer + ":" + source.Type, source.Id);
    const auto saved = transaction.Execute(
        sql::kFindSourceShipment, organization, source.Producer, source.Type, source.Id);
    if (const auto shipment_id = ValidateShipmentReplay(saved, document); shipment_id.has_value()) {
        const auto shipment = converters::Convert(
            transaction.Execute(sql::kGetShipment, organization, shipment_id.value())
                .AsSingleRow<models::pg::ShipmentRaw>());
        transaction.Commit();
        return shipment;
    }

    const auto result = InsertShipment(transaction, organization, body, settings);
    transaction.Execute(sql::kLinkSourceShipment, organization, source.Producer, source.Type,
        source.Id, result.id, document);

    helpers::WriteAudit(
        transaction, organization, actor, "create_shipment", result.id, body, result);
    helpers::QueueRouteProjection(transaction, organization, result);
    helpers::QueueShipmentCreated(transaction, organization, actor, result);
    transaction.Commit();
    return result;
}

std::optional<models::Shipment> ShipmentsComponent::FindManualShipmentReplay(
    const std::string& organization, const models::CreateShipmentRequest& request,
    const ShipmentSource& source, const json::Value& document) const {
    if (const auto saved = helpers::FindSavedReplay<models::Shipment>(
            cluster_, organization, "create_shipment", request);
        saved.has_value())
        return saved;
    return FindCreatedShipment(organization, source, document);
}

models::Shipment ShipmentsComponent::GetShipment(
    const std::string& organization, const std::string& shipment_id) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetShipment, organization, shipment_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    return converters::Convert(rows.AsSingleRow<models::pg::ShipmentRaw>());
}

std::int64_t ShipmentsComponent::GetRouteUsageVersion(
    const std::string& organization, const std::string& shipment_id) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetRouteUsageVersion, organization, shipment_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};

    return rows.AsSingleRow<std::int64_t>();
}

models::Shipment ShipmentsComponent::UpdateShipment(const std::string& organization,
    const platform::auth::Identity& actor, const models::UpdateShipmentRequest& request,
    const std::optional<std::int64_t>& route_usage_version) const {
    try {
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto current =
            transaction.Execute(sql::kLockShipment, organization, request.shipment_id);
        if (current.IsEmpty())
            throw ApiError<404>{"not_found", "Поставка не найдена"};

        const auto previous = current.AsSingleRow<models::pg::ShipmentRaw>();
        helpers::CheckRevision(previous.Revision, request.expected_revision);
        const auto card = helpers::UpdateShipmentCard(request, previous);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<models::pg::ShipmentDictionaryValue>>();

        if (route_usage_version.has_value()) {
            const auto current_usage =
                transaction.Execute(sql::kGetRouteUsageVersion, organization, request.shipment_id)
                    .AsSingleRow<std::int64_t>();
            if (current_usage != route_usage_version.value())
                throw ApiError<409>{"route_usage_changed",
                    "Данные участков изменились. Обновите поставку и повторите сохранение"};
        }

        helpers::ValidateCard(card,
            settings_.Effective(
                transaction, organization, boost::uuids::to_string(card.CustomerId)),
            dictionaries, &previous);

        const auto result =
            converters::Convert(transaction
                                    .Execute(sql::kUpdateShipment, organization,
                                        request.shipment_id, request.expected_revision, card)
                                    .AsSingleRow<models::pg::ShipmentRaw>());
        if (card.CargoDescription != previous.CargoDescription ||
            card.CustomerId != previous.CustomerId)
            RememberCargo(transaction, organization, card.CustomerId, card.CargoDescription);
        helpers::WriteAudit(
            transaction, organization, actor, "update_shipment", result.id, request, result);
        helpers::QueueRouteProjection(transaction, organization, result);
        helpers::QueueRouteChanged(
            transaction, organization, actor, converters::Convert(previous), result);
        transaction.Commit();
        return result;
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"number_exists", "Номер поставки уже занят"};
    }
}

std::vector<models::Shipment> ShipmentsComponent::GetShipments(
    const std::string& organization, const std::vector<std::string>& shipment_ids) const {
    return converters::Convert(cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kTransportationContexts, organization, shipment_ids));
}

void ShipmentsComponent::RegisterRouteUsage(
    const std::string& organization, const models::RegisterRouteUsageRequest& request) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto rows = transaction.Execute(sql::kLockShipment, organization, request.shipment_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    const auto shipment = rows.AsSingleRow<models::pg::ShipmentRaw>();
    if (shipment.Revision != request.expected_revision)
        throw ApiError<409>{"shipment_revision_conflict",
            "Поставка изменилась. Обновите перевозку и повторите сохранение"};
    for (const auto& id : request.segment_ids) {
        const auto found =
            std::find_if(shipment.RouteSegments.begin(), shipment.RouteSegments.end(),
                [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
        if (found == shipment.RouteSegments.end())
            throw ApiError<409>{"route_changed", "Маршрут поставки изменён. Обновите перевозку"};
    }
    transaction.Execute(
        sql::kRegisterRouteUsage, organization, request.shipment_id, request.segment_ids);
    transaction.Commit();
}
}  // namespace shipments
