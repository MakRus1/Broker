#pragma once
#include <functional>
#include <map>
#include <notifications/sql_queries.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/handler_error.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace notifications::events {
namespace pg = userver::storages::postgres;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

class Registry {
  public:
    using Effect = std::function<void(pg::Transaction&)>;

    template <typename Event, typename Validate, typename Handle>
    void Register(std::string type, std::int64_t version, std::string producer,
        std::string permission, Validate validate, Handle handle) {
        Entry entry{std::move(producer), std::move(permission),
            [validate = std::move(validate), handle = std::move(handle)](
                const platform::auth::Identity& identity,
                const models::PublishedEvent& envelope) -> Effect {
                auto event = Parse<Event>(envelope.payload);
                validate(identity, envelope, event);
                return [event = std::move(event), handle](
                           pg::Transaction& transaction) { handle(transaction, event); };
            }};
        if (!entries_.emplace(Key{std::move(type), version}, std::move(entry)).second)
            throw std::logic_error("Повторная регистрация типа и версии события");
    }

    void Accept(const pg::ClusterPtr& cluster, const platform::auth::Identity& identity,
        const models::PublishedEvent& envelope) const {
        const auto found = entries_.find({envelope.type, envelope.version});
        if (found == entries_.end())
            throw ApiError<400>{"unsupported_event", "Неизвестный тип или версия события"};
        const auto& entry = found->second;
        if (identity.ClientId != entry.Producer || !identity.Permissions.contains(entry.Permission))
            throw ApiError<403>{
                "event_producer_forbidden", "Отправителю не разрешён этот тип события"};
        // Разбор и проверки не держат транзакцию БД; effect выполняет только локальные записи.
        const auto effect = entry.Prepare(identity, envelope);
        const auto payload = userver::formats::json::ValueBuilder(envelope).ExtractValue();
        auto transaction = cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto inserted = transaction.Execute(sql::kAcceptEvent, identity.ClientId,
            envelope.organization_id, envelope.id, envelope.type, envelope.version, payload);
        if (inserted.IsEmpty()) {
            if (!transaction
                     .Execute(sql::kGetEvent, identity.ClientId, envelope.organization_id,
                         envelope.id, envelope.type, envelope.version, payload)
                     .AsSingleRow<bool>())
                throw ApiError<409>{
                    "event_conflict", "Событие с этим ключом уже принято с другими данными"};
        } else {
            effect(transaction);
        }
        transaction.Commit();
    }

  private:
    template <typename Event> static Event Parse(const userver::formats::json::Value& payload) {
        try {
            return models::ParseModelJson(payload, std::type_identity<Event>{});
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            throw ApiError<400>{"invalid_event_payload", "Некорректные данные события"};
        }
    }

    using Key = std::pair<std::string, std::int64_t>;

    struct Entry {
        std::string Producer;
        std::string Permission;
        std::function<Effect(const platform::auth::Identity&, const models::PublishedEvent&)>
            Prepare;
    };

    std::map<Key, Entry> entries_;
};
}  // namespace notifications::events
