#pragma once
#include <clients/finance/client.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline clients::finance::models::FinancialHistoryCursor Convert(
    const models::finance::FinancialHistoryCursor& value) {
    return {.entity_id = value.entity_id,
        .occurred_at = value.occurred_at,
        .revision = value.revision,
        .source =
            clients::finance::models::ParseEnumScalar(models::finance::EnumValue(value.source),
                clients::finance::models::FinancialHistorySource{})};
}

inline models::finance::FinancialHistoryCursor Convert(
    const clients::finance::models::FinancialHistoryCursor& value) {
    return {.entity_id = value.entity_id,
        .occurred_at = value.occurred_at,
        .revision = value.revision,
        .source =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.source),
                models::finance::FinancialHistorySource{})};
}

inline models::finance::FinancialHistoryValue Convert(
    const clients::finance::models::FinancialHistoryValue& value) {
    return {.currency = value.currency,
        .format =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.format),
                models::finance::FinancialHistoryFormat{}),
        .value = value.value};
}

inline models::finance::FinancialHistoryEvent Convert(
    const clients::finance::models::FinancialHistoryEvent& value) {
    models::finance::FinancialHistoryEvent result{.actor = value.actor,
        .actor_name = value.actor_name,
        .cursor = Convert(value.cursor),
        .has_previous = value.has_previous,
        .title = value.title};
    for (const auto& change : value.changes) {
        models::finance::FinancialHistoryChange item{.field = change.field, .label = change.label};
        if (change.before.has_value())
            item.before = Convert(change.before.value());
        if (change.after.has_value())
            item.after = Convert(change.after.value());
        result.changes.push_back(std::move(item));
    }
    return result;
}
}  // namespace web_bff
