#include <components/event_acceptance.hpp>
#include <converters/notification_delivery.hpp>
#include <notifications/sql_queries.hpp>
#include <platform/handler_error.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <variant>

namespace {
void ValidateRecipient(
    const platform::auth::Identity& identity, const notifications::NotificationDelivery& delivery) {
    if (delivery.RecipientIssuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}
}  // namespace

namespace notifications {
namespace pg = userver::storages::postgres;

EventAcceptanceComponent::EventAcceptanceComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()) {
}

userver::yaml_config::Schema EventAcceptanceComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void EventAcceptanceComponent::AcceptEvent(
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
                const auto saved = transaction.Execute(sql::kGetEvent, identity.ClientId,
                    event.organization_id, event.id, type, version);
                using Event = std::decay_t<decltype(event)>;
                if (saved.IsEmpty() ||
                    userver::formats::json::ValueBuilder(
                        models::ParseModelJson(
                            saved.template AsSingleRow<userver::formats::json::Value>(),
                            std::type_identity<Event>{}))
                            .ExtractValue() != document)
                    throw platform::http::HandlerError<409>{
                        "event_conflict", "Событие с этим ключом уже принято с другими данными"};
            } else if (delivery.RecipientSubject.has_value()) {
                transaction.Execute(sql::kCreateNotification, event.organization_id,
                    delivery.RecipientIssuer, delivery.RecipientSubject.value(), delivery.EntityId,
                    delivery.EntityNumber, delivery.Revision, type);
            }

            transaction.Commit();
        },
        message.value);
}
}  // namespace notifications
