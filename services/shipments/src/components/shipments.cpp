#include <boost/uuid/string_generator.hpp>
#include <components/shipments.hpp>
#include <configs/keys.hpp>
#include <converters/shipments.hpp>
#include <domain/shipment_card.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_notifications.hpp>
#include <helpers/route_projection.hpp>
#include <models/pg/shipment_data.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <models/pg/shipment_raw.hpp>
#include <models/shipment_source_record.hpp>
#include <openapi/models.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <platform/text.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
using namespace helpers;
using namespace converters;

ShipmentsComponent::ShipmentsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()),
      settings_(context.FindComponent<SettingsComponent>()),
      references_(context.FindComponent<ReferencesComponent>()) {}

userver::yaml_config::Schema ShipmentsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

namespace {
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
    auto card = domain::CreateShipmentCard(body, settings);
    const auto dictionaries = transaction.Execute(sql::kListShipmentDictionaries)
                                  .AsContainer<std::vector<models::pg::ShipmentDictionaryValue>>();
    domain::ValidateCard(card, settings, dictionaries);
    card.Number = body.number.has_value()
        ? userver::utils::text::Trim(body.number.value())
        : GenerateShipmentNumber(transaction, organization, card.ShipmentDate, settings);
    if (card.Number.empty())
        throw ApiError<400>{"invalid_number", "Укажите номер поставки"};
    const auto row = transaction.Execute(sql::kCreateShipment, organization, card)
                         .AsSingleRow<models::pg::ShipmentRaw>();
    RememberCargo(transaction, organization, row.CustomerId, row.CargoDescription);
    return Convert(row);
}

}  // namespace

std::optional<models::Shipment> ShipmentsComponent::FindCreatedShipment(
    const std::string& organization, const ShipmentSource& source, const Json& document) const {
    const auto saved = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindSourceShipment,
        organization, source.Producer, source.Type, source.Id);
    if (saved.IsEmpty())
        return std::nullopt;
    const auto stored = saved.AsSingleRow<models::ShipmentSourceRecord>(pg::kRowTag);
    if (stored.Payload != document)
        throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
    return Convert(cluster_
                       ->Execute(pg::ClusterHostType::kMaster, sql::kGetShipment, organization,
                           stored.ShipmentId)
                       .AsSingleRow<models::pg::ShipmentRaw>());
}

// Один сценарий для ручного создания и команд доверенных сервисов.
// Источник участвует только в дедупликации и связи, не выбирает бизнес-ветку.
models::Shipment ShipmentsComponent::CreateShipmentFromSource(const std::string& organization,
    const platform::auth::Identity& actor, const models::CreateShipmentRequest& body,
    const ShipmentSource& source, const Json& document) const {
    if (const auto saved = FindCreatedShipment(organization, source, document); saved.has_value())
        return saved.value();
    const auto settings = settings_.Effective(organization, body.customer_id);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(
        sql::kLockIdempotency, organization, source.Producer + ":" + source.Type, source.Id);
    const auto saved = transaction.Execute(
        sql::kFindSourceShipment, organization, source.Producer, source.Type, source.Id);
    if (!saved.IsEmpty()) {
        const auto stored = saved.AsSingleRow<models::ShipmentSourceRecord>(pg::kRowTag);
        if (stored.Payload != document)
            throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
        const auto shipment =
            Convert(transaction.Execute(sql::kGetShipment, organization, stored.ShipmentId)
                        .AsSingleRow<models::pg::ShipmentRaw>());
        transaction.Commit();
        return shipment;
    }
    const auto result = InsertShipment(transaction, organization, body, settings);
    transaction.Execute(sql::kLinkSourceShipment, organization, source.Producer, source.Type,
        source.Id, result.id, document);
    WriteAudit(transaction, organization, actor, "create_shipment", result.id, body, result);
    QueueRouteProjection(transaction, organization, result);
    QueueShipmentCreated(transaction, organization, actor, result);
    transaction.Commit();
    return result;
}

models::ShipmentsCreateResult ShipmentsComponent::CreateShipment(
    models::CreateShipmentRequest request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto saved =
            FindSavedReplay<models::Shipment>(cluster_, organization, "create_shipment", request);
        if (saved.has_value())
            return models::ShipmentsCreateResult{.shipment = saved.value()};
        const auto source = ShipmentSource::Manual(request.idempotency_key);
        if (const auto replay = FindCreatedShipment(organization, source, Document(request));
            replay.has_value())
            return models::ShipmentsCreateResult{.shipment = replay.value()};
        const auto logist = request.logist.has_value()
            ? platform::TrimToOptional(request.logist.value())
            : references_.CustomerLogist(context, request.customer_id);
        references_.ValidateLogist(context, logist);
        if (request.route_segments.has_value()) {
            references_.ResolveRoute(context, request.route_segments.value(), {});
        }
        auto body = request;
        body.logist = logist;
        const auto result =
            CreateShipmentFromSource(organization, identity, body, source, Document(request));
        return models::ShipmentsCreateResult{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"number_exists", "Номер поставки уже занят"};
    }
}

models::ShipmentsGetResult ShipmentsComponent::GetShipment(
    const models::GetShipmentRequest& request,
    userver::server::request::RequestContext& context) const {

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetShipment, organization, request.shipment_id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Поставка не найдена"};
    const auto result = Convert(rows.AsSingleRow<models::pg::ShipmentRaw>());
    return models::ShipmentsGetResult{.shipment = result};
}

models::ShipmentsUpdateResult ShipmentsComponent::UpdateShipment(
    models::UpdateShipmentRequest request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        if (request.logist.has_value())
            references_.ValidateLogist(context, platform::TrimToOptional(request.logist.value()));
        const auto organization = platform::auth::GetOrganization(identity);
        if (request.route_segments.has_value()) {
            const auto rows = cluster_->Execute(
                pg::ClusterHostType::kMaster, sql::kGetShipment, organization, request.shipment_id);
            if (rows.IsEmpty())
                throw ApiError<404>{"not_found", "Поставка не найдена"};
            const auto previous = rows.AsSingleRow<models::pg::ShipmentRaw>();
            CheckRevision(previous.Revision, request.expected_revision);
            const auto previous_route = Convert(previous).route_segments;
            references_.ResolveRoute(context, request.route_segments.value(), previous_route);
            references_.CheckExistingRouteUsage(context, request.shipment_id,
                domain::ChangedRouteSegments(previous, request.route_segments.value()));
        }
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto current =
            transaction.Execute(sql::kLockShipment, organization, request.shipment_id);
        if (current.IsEmpty())
            throw ApiError<404>{"not_found", "Поставка не найдена"};
        const auto previous = current.AsSingleRow<models::pg::ShipmentRaw>();
        CheckRevision(previous.Revision, request.expected_revision);
        const auto card = domain::UpdateShipmentCard(request, previous);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<models::pg::ShipmentDictionaryValue>>();
        const auto protected_ids =
            transaction.Execute(sql::kGetRouteUsage, organization, request.shipment_id)
                .AsContainer<std::vector<std::string>>();
        domain::ValidateRouteUsage(previous, card, protected_ids);
        domain::ValidateCard(card,
            settings_.Effective(organization, boost::uuids::to_string(card.CustomerId)),
            dictionaries, &previous);
        const auto result = Convert(transaction
                                        .Execute(sql::kUpdateShipment, organization,
                                            request.shipment_id, request.expected_revision, card)
                                        .AsSingleRow<models::pg::ShipmentRaw>());
        if (card.CargoDescription != previous.CargoDescription ||
            card.CustomerId != previous.CustomerId)
            RememberCargo(transaction, organization, card.CustomerId, card.CargoDescription);
        WriteAudit(
            transaction, organization, identity, "update_shipment", result.id, request, result);
        QueueRouteProjection(transaction, organization, result);
        QueueRouteChanged(transaction, organization, identity, Convert(previous), result);
        transaction.Commit();
        return models::ShipmentsUpdateResult{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"number_exists", "Номер поставки уже занят"};
    }
}

models::ShipmentsTransportationContextsResult ShipmentsComponent::GetTransportationContexts(
    const models::TransportationContextsRequest& request,
    userver::server::request::RequestContext& context) const {

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = cluster_
                          ->Execute(pg::ClusterHostType::kMaster, sql::kTransportationContexts,
                              organization, request.shipment_ids)
                          .AsContainer<std::vector<models::pg::ShipmentRaw>>();
    std::vector<models::TransportationContext> items;
    for (const auto& row : rows) {
        const auto shipment = Convert(row);
        items.push_back(models::TransportationContext{
            .carriage_terms = shipment.carriage_terms.value_or(std::nullopt),
            .incoterms = shipment.incoterms.value_or(std::nullopt),
            .loading = shipment.loading.value_or(std::nullopt),
            .revision = shipment.revision,
            .route_segments = shipment.route_segments,
            .shipment_id = shipment.id});
    }
    return models::ShipmentsTransportationContextsResult{.items = std::move(items)};
}

models::ShipmentsRegisterRouteUsageResult ShipmentsComponent::RegisterRouteUsage(
    const models::RegisterRouteUsageRequest& request,
    userver::server::request::RequestContext& context) const {

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
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
    return models::ShipmentsRegisterRouteUsageResult{.registered = true};
}

models::ShipmentsEnsureCreatedResult ShipmentsComponent::EnsureShipmentCreated(
    const models::EnsureShipmentRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& command = request;
    const auto& sender = platform::auth::GetIdentity(context);
    if (sender.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    auto actor = sender;
    actor.Subject = command.actor_subject;
    actor.Issuer = command.actor_issuer;
    const auto result = CreateShipmentFromSource(command.organization_id, actor, command.shipment,
        {.Producer = sender.ClientId, .Type = command.source.type, .Id = command.source.id},
        Document(command));
    return models::ShipmentsEnsureCreatedResult{.shipment = result};
}
}  // namespace shipments
