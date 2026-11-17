#include <components/event_acceptance.hpp>
#include <converters/notification_delivery.hpp>
#include <notifications/sql_queries.hpp>
#include <platform/handler_error.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <variant>

namespace {
void ValidateRecipient(
    const platform::auth::Identity& identity, const notifications::NotificationDelivery& delivery) {
    if (delivery.RecipientIssuer != identity.Issuer)
        throw platform::http::HandlerError<400>{"invalid_recipient_issuer",
            "Получатель должен принадлежать провайдеру пользователей отправителя"};
}

bool MatchesStoredEvent(const userver::storages::postgres::ResultSet& saved,
    const userver::formats::json::Value& document) {
    namespace json = userver::formats::json;
    namespace models = notifications::models;
    if (saved.IsEmpty())
        return false;
    const auto payload = saved.AsSingleRow<json::Value>();
    if (payload == document)
        return true;

    const auto event =
        models::ParseModelJson(payload, std::type_identity<models::PublishedEvent>{});
    return json::ValueBuilder(event).ExtractValue() == document;
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
                if (!MatchesStoredEvent(saved, document))
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
