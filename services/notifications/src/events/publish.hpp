#pragma once
#include <events/shipment_created.hpp>
#include <events/shipment_route_changed.hpp>
#include <notifications/sql_queries.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <variant>

namespace notifications::events {
namespace pg = userver::storages::postgres;

inline void Accept(const pg::ClusterPtr& cluster, const platform::auth::Identity& identity,
    const models::PublishedEvent& message) {
    std::visit(
        [&](const auto& event) {
            // Кодген проверил форму события. Проверки отправителя и контекста — до транзакции.
            Validate(identity, event);
            const auto document = userver::formats::json::ValueBuilder(event).ExtractValue();
            const auto type = models::EnumValue(event.type);
            const auto version = models::EnumValue(event.version);
            auto transaction =
                cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
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
                Apply(transaction, event);
            }
            transaction.Commit();
        },
        message.value);
}
}  // namespace notifications::events
