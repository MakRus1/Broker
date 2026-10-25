#pragma once
#include <helpers/converters.hpp>

namespace counterparties::helpers {
inline models::CounterpartySnapshot ReadSnapshot(const userver::formats::json::Value& document) {
    // Старые записи содержат только известные на тот момент поля.
    userver::formats::json::ValueBuilder snapshot(document);
    snapshot.Remove("id");
    snapshot.Remove("revision");
    return models::ParseModelJson(
        snapshot.ExtractValue(), std::type_identity<models::CounterpartySnapshot>{});
}

inline models::CounterpartyEvent Convert(const pgtypes::CounterpartyEventV2& row) {
    models::CounterpartyEvent event;
    event.id = row.Id;
    event.actor = row.Actor;
    event.action = row.Action;
    event.occurred_at = row.OccurredAt;
    const auto& document = row.Document;
    event.actor_name = document["actor_name"].As<std::optional<std::string>>();
    const auto& result = document["result"];
    event.revision = result["revision"].As<std::optional<std::int64_t>>();
    event.after = ReadSnapshot(result);
    if (document.HasMember("before")) {
        event.before = ReadSnapshot(document["before"]);
    } else if (row.Previous.has_value() && event.action != "create_counterparty") {
        const auto& previous = row.Previous.value();
        const auto revision = previous["revision"].As<std::optional<std::int64_t>>();
        // Не выдаём снимок через пропуск редакций за достоверное состояние «до».
        if (revision.has_value() && event.revision.has_value() &&
            revision.value() + 1 == event.revision.value())
            event.before = ReadSnapshot(previous);
    }
    return event;
}

inline std::vector<models::CounterpartyEvent> Convert(
    const std::vector<pgtypes::CounterpartyEventV2>& rows) {
    std::vector<models::CounterpartyEvent> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row));
    return result;
}
}  // namespace counterparties::helpers
