#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <chrono>
#include <components/shipments.hpp>
#include <configs/keys.hpp>
#include <converters/route_details.hpp>
#include <converters/shipments.hpp>
#include <helpers/database.hpp>
#include <helpers/dictionaries.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_notifications.hpp>
#include <helpers/route_projection.hpp>
#include <helpers/route_usage.hpp>
#include <models/pg/shipment_data.hpp>
#include <models/pg/shipment_dictionary_value.hpp>
#include <models/pg/shipment_raw.hpp>
#include <openapi/models.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/calendar_date.hpp>
#include <platform/date_validation.hpp>
#include <platform/document_number.hpp>
#include <registry/query.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/datetime.hpp>
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
std::optional<std::string> CleanText(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    auto result = userver::utils::text::Trim(value.value());
    return result.empty() ? std::nullopt : std::optional<std::string>{result};
}

userver::utils::datetime::Date Today() {
    return userver::utils::datetime::Date{
        std::chrono::floor<std::chrono::days>(userver::utils::datetime::Now())};
}

void ValidateShipmentDate(userver::utils::datetime::Date value) {
    using namespace std::chrono;
    if (!platform::IsCalendarDateInRange(
            value, year{1900} / January / 1, year{9999} / December / 31))
        throw ApiError<400>{
            "invalid_date", "Укажите корректную дату поставки не раньше 01.01.1900"};
}

template <typename Request> void ApplyCard(const Request& request, models::pg::ShipmentData& row) {
    if (request.shipment_date.has_value())
        row.ShipmentDate = request.shipment_date.value();
    if (request.recipient_id.has_value()) {
        const auto value = request.recipient_id.value();
        row.RecipientId = value.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.value())}
            : std::nullopt;
    }
    if (request.logist.has_value())
        row.Logist = CleanText(request.logist.value());
    if (request.cargo_description.has_value())
        row.CargoDescription = CleanText(request.cargo_description.value());
    if (request.status.has_value())
        row.Status = request.status.value();
    if (request.incoterms.has_value())
        row.Incoterms = CleanText(request.incoterms.value());
    if (request.carriage_terms.has_value())
        row.CarriageTerms = CleanText(request.carriage_terms.value());
    if (request.loading.has_value())
        row.Loading = CleanText(request.loading.value());
    if (request.delivery_city.has_value())
        row.DeliveryCity = CleanText(request.delivery_city.value());
    if (request.delivery_country.has_value())
        row.DeliveryCountry = request.delivery_country.value();
    if (request.delivery_address.has_value())
        row.DeliveryAddress = CleanText(request.delivery_address.value());
    if (request.route_segments.has_value()) {
        row.RouteSegments.clear();
        std::set<std::string> ids;
        for (const auto& segment : request.route_segments.value()) {
            if (!ids.insert(segment.id).second)
                throw ApiError<400>{
                    "duplicate_segment", "Идентификаторы участков должны быть уникальными"};
            row.RouteSegments.push_back(Convert(segment));
        }
    }
    ValidateShipmentDate(row.ShipmentDate);
}

void ValidateCard(const models::pg::ShipmentData& row, const auto& settings,
    const std::vector<models::pg::ShipmentDictionaryValue>& dictionaries,
    const std::optional<models::pg::ShipmentRaw>& previous = std::nullopt) {
    if ((!previous.has_value() || previous.value().Status != row.Status) &&
        std::find(settings.Statuses.begin(), settings.Statuses.end(), row.Status) ==
            settings.Statuses.end())
        throw ApiError<400>{"invalid_status", "Статус недоступен для организации"};
    for (const auto& segment : row.RouteSegments) {
        const bool unchanged = previous.has_value() &&
            std::any_of(previous.value().RouteSegments.begin(),
                previous.value().RouteSegments.end(),
                [&](const auto& old) { return old.Id == segment.Id && old.Type == segment.Type; });
        if (!unchanged &&
            (std::find(settings.RouteTypes.begin(), settings.RouteTypes.end(), segment.Type) ==
                    settings.RouteTypes.end() ||
                std::none_of(dictionaries.begin(), dictionaries.end(), [&](const auto& entry) {
                    return ParseDictionary(entry.Dictionary) == ShipmentDictionary::kRouteType &&
                        entry.Code == segment.Type;
                })))
            throw ApiError<400>{"invalid_route_type", "Тип участка недоступен для организации"};
    }
    const auto validate = [&](const std::optional<std::string>& value,
                              ShipmentDictionary dictionary, const std::string& message,
                              const std::optional<std::string>& old) {
        if (!value.has_value() || (previous.has_value() && old == value))
            return;
        if (std::none_of(dictionaries.begin(), dictionaries.end(), [&](const auto& entry) {
                return ParseDictionary(entry.Dictionary) == dictionary &&
                    entry.Code == value.value();
            }))
            throw ApiError<400>{"invalid_" + std::string{DictionaryCode(dictionary)}, message};
    };
    validate(row.Incoterms, ShipmentDictionary::kIncoterms, "Выберите Incoterms из справочника",
        previous.has_value() ? previous.value().Incoterms : std::nullopt);
    validate(row.CarriageTerms, ShipmentDictionary::kCarriageTerms,
        "Выберите условия перевозки из справочника",
        previous.has_value() ? previous.value().CarriageTerms : std::nullopt);
    validate(row.Loading, ShipmentDictionary::kLoading, "Выберите загрузку из справочника",
        previous.has_value() ? previous.value().Loading : std::nullopt);
}

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

struct CreatedShipmentRow {
    std::string Id;
    Json Document;
};

template <typename Request>
models::Shipment InsertShipment(pg::Transaction& transaction, const std::string& organization,
    const Request& body, const auto& settings, const std::optional<std::string>& logist) {
    models::pg::ShipmentData card;
    card.CustomerId = boost::uuids::string_generator{}(body.customer_id);
    card.Reference = body.reference.value_or("");
    card.ShipmentDate = Today();
    card.Status = settings.Statuses.front();
    card.Logist = logist;
    ApplyCard(body, card);
    const auto dictionaries = transaction.Execute(sql::kListShipmentDictionaries)
                                  .AsContainer<std::vector<models::pg::ShipmentDictionaryValue>>();
    ValidateCard(card, settings, dictionaries);
    card.Number = body.number.has_value()
        ? userver::utils::text::Trim(body.number.value())
        : GenerateShipmentNumber(transaction, organization, card.ShipmentDate, settings);
    if (card.Number.empty())
        throw ApiError<400>{"invalid_number", "Укажите номер поставки"};
    const auto row = transaction.Execute(sql::kCreateShipment, organization, card)
                         .AsSingleRow<models::pg::ShipmentRaw>();
    RememberCargo(transaction, organization, row.CustomerId, row.CargoDescription);
    const auto result = Convert(row);
    return result;
}

}  // namespace

std::optional<models::Shipment> ShipmentsComponent::FindCreatedShipment(
    const std::string& organization, const ShipmentSource& source, const Json& document) const {
    const auto saved = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kFindSourceShipment,
        organization, source.Producer, source.Type, source.Id);
    if (saved.IsEmpty())
        return std::nullopt;
    const auto stored = saved.AsSingleRow<CreatedShipmentRow>(pg::kRowTag);
    if (stored.Document != document)
        throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
    return Convert(
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetShipment, organization, stored.Id)
            .AsSingleRow<models::pg::ShipmentRaw>());
}

// Один сценарий для ручного создания и команд доверенных сервисов.
// Источник участвует только в дедупликации и связи, не выбирает бизнес-ветку.
models::Shipment ShipmentsComponent::CreateShipment(const std::string& organization,
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
        const auto stored = saved.AsSingleRow<CreatedShipmentRow>(pg::kRowTag);
        if (stored.Document != document)
            throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другими данными"};
        const auto shipment =
            Convert(transaction.Execute(sql::kGetShipment, organization, stored.Id)
                        .AsSingleRow<models::pg::ShipmentRaw>());
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

models::ShipmentsFilterValuesResult ShipmentsComponent::FilterValues(
    const models::ShipmentsFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    using namespace ::shipments::registry_query;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        models::ShipmentsFilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ShipmentsCreateResult ShipmentsComponent::Create(models::CreateShipmentRequest request,
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
            ? CleanText(request.logist.value())
            : references_.CustomerLogist(context, request.customer_id);
        references_.ValidateLogist(context, logist);
        if (request.route_segments.has_value()) {
            const auto locations =
                references_.LoadRouteLocations(context, request.route_segments.value());
            domain::ResolveRouteDetails(request.route_segments.value(), {}, locations);
            references_.ValidateRouteCounterparties(context, request.route_segments.value(), {});
        }
        auto body = request;
        body.logist = logist;
        const auto result = CreateShipment(organization, identity, body, source, Document(request));
        return models::ShipmentsCreateResult{.shipment = result};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"number_exists", "Номер поставки уже занят"};
    }
}

models::ShipmentsGetResult ShipmentsComponent::Get(const models::GetShipmentRequest& request,
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

models::ShipmentsListResult ShipmentsComponent::List(const models::ListShipmentsRequest& request,
    userver::server::request::RequestContext& context) const {
    using namespace ::shipments::registry_query;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (body.after.has_value() && (body.cursor.has_value() || !order.ById || body.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::ShipmentRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ShipmentRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Shipment.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return models::ShipmentsListResult{.items = Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ShipmentsUpdateResult ShipmentsComponent::Update(models::UpdateShipmentRequest request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
        if (request.logist.has_value())
            references_.ValidateLogist(context, CleanText(request.logist.value()));
        const auto organization = platform::auth::GetOrganization(identity);
        if (request.route_segments.has_value()) {
            const auto rows = cluster_->Execute(
                pg::ClusterHostType::kMaster, sql::kGetShipment, organization, request.shipment_id);
            if (rows.IsEmpty())
                throw ApiError<404>{"not_found", "Поставка не найдена"};
            const auto previous = rows.AsSingleRow<models::pg::ShipmentRaw>();
            CheckRevision(previous.Revision, request.expected_revision);
            const auto locations =
                references_.LoadRouteLocations(context, request.route_segments.value());
            domain::ResolveRouteDetails(
                request.route_segments.value(), Convert(previous).route_segments, locations);
            references_.ValidateRouteCounterparties(
                context, request.route_segments.value(), Convert(previous).route_segments);
            references_.CheckExistingRouteUsage(context, request.shipment_id,
                ChangedRouteSegments(previous, request.route_segments.value()));
        }
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto current =
            transaction.Execute(sql::kLockShipment, organization, request.shipment_id);
        if (current.IsEmpty())
            throw ApiError<404>{"not_found", "Поставка не найдена"};
        const auto previous = current.AsSingleRow<models::pg::ShipmentRaw>();
        CheckRevision(previous.Revision, request.expected_revision);
        auto card = ConvertData(previous);
        if (request.reference.has_value())
            card.Reference = request.reference.value();
        if (request.customer_id.has_value())
            card.CustomerId = boost::uuids::string_generator{}(request.customer_id.value());
        if (request.number.has_value()) {
            card.Number = userver::utils::text::Trim(request.number.value());
            if (card.Number.empty())
                throw ApiError<400>{"invalid_number", "Укажите номер поставки"};
        }
        ApplyCard(request, card);
        const auto dictionaries =
            transaction.Execute(sql::kListShipmentDictionaries)
                .AsContainer<std::vector<models::pg::ShipmentDictionaryValue>>();
        const auto protected_ids =
            transaction.Execute(sql::kGetRouteUsage, organization, request.shipment_id)
                .AsContainer<std::vector<std::string>>();
        for (const auto& id : protected_ids) {
            const auto before =
                std::find_if(previous.RouteSegments.begin(), previous.RouteSegments.end(),
                    [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
            const auto after = std::find_if(card.RouteSegments.begin(), card.RouteSegments.end(),
                [&](const auto& segment) { return boost::uuids::to_string(segment.Id) == id; });
            if (before != previous.RouteSegments.end() &&
                (after == card.RouteSegments.end() || after->Type != before->Type))
                throw ApiError<409>{"route_segment_in_use",
                    "Нельзя удалить или изменить тип участка с данными перевозок"};
        }
        ValidateCard(card,
            settings_.Effective(organization, boost::uuids::to_string(card.CustomerId)),
            dictionaries, previous);
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

models::ShipmentsTransportationContextsResult ShipmentsComponent::TransportationContexts(
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

models::ShipmentsEnsureCreatedResult ShipmentsComponent::EnsureCreated(
    const models::EnsureShipmentRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& command = request;
    const auto& sender = platform::auth::GetIdentity(context);
    if (sender.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    auto actor = sender;
    actor.Subject = command.actor_subject;
    actor.Issuer = command.actor_issuer;
    const auto result = CreateShipment(command.organization_id, actor, command.shipment,
        {.Producer = sender.ClientId, .Type = command.source.type, .Id = command.source.id},
        Document(command));
    return models::ShipmentsEnsureCreatedResult{.shipment = result};
}
}  // namespace shipments
