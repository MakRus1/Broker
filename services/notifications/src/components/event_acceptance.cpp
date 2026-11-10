#include <components/event_acceptance.hpp>
#include <converters/notification_delivery.hpp>
#include <notifications/sql_queries.hpp>
#include <platform/handler_error.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <variant>

namespace notifications {
namespace pg = userver::storages::postgres;

namespace {
void ValidateRecipient(
    const platform::auth::Identity& identity, const NotificationDelivery& delivery) {
    if (delivery.RecipientIssuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

void ApplyNotification(userver::storages::postgres::Transaction& transaction,
    const std::string& organization, const NotificationDelivery& delivery,
    const std::string& event_type) {
    if (!delivery.RecipientSubject.has_value())
        return;
    transaction.Execute(sql::kCreateNotification, organization, delivery.RecipientIssuer,
        delivery.RecipientSubject.value(), delivery.EntityId, delivery.EntityNumber,
        delivery.Revision, event_type);
}
}  // namespace

EventAcceptanceComponent::EventAcceptanceComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()) {
}

userver::yaml_config::Schema EventAcceptanceComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void EventAcceptanceComponent::Accept(
    const models::PublishedEvent& message, const platform::auth::Identity& identity) const {
    std::visit(
        [&](const auto& event) {
            // Кодген проверил форму события. Проверки отправителя и контекста — до транзакции.
            const auto delivery = converters::Convert(event.payload);
            ValidateRecipient(identity, delivery);
            const auto document = userver::formats::json::ValueBuilder(event).ExtractValue();
            const auto type = models::EnumValue(event.type);
            const auto version = models::EnumValue(event.version);
            auto transaction =
                cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            const auto inserted = transaction.Execute(sql::kAcceptEvent, identity.ClientId,
                event.organization_id, event.id, type, version, document);
            if (inserted.IsEmpty()) {
                if (!transaction
                         .Execute(sql::kGetEvent, identity.ClientId, event.organization_id,
                             event.id, type, version, document)
                         .template AsSingleRow<bool>())
                    throw platform::http::HandlerError<409>{
                        "event_conflict", "Событие с этим ключом уже принято с другими данными"};
            } else {
                ApplyNotification(transaction, event.organization_id, delivery, type);
            }
            transaction.Commit();
        },
        message.value);
}
}  // namespace notifications
