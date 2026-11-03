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
        .display_name = value.display_name,
        .format =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.format),
                models::finance::FinancialHistoryFormat{}),
        .value = value.value};
}

inline models::finance::FinancialHistoryReference Convert(
    const clients::finance::models::FinancialHistoryReference& value) {
    return {.id = value.id,
        .name = value.name,
        .source =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.source),
                models::finance::FinancialHistorySource{})};
}

inline models::finance::FinancialHistoryEvent Convert(
    const clients::finance::models::FinancialHistoryEvent& value) {
    models::finance::FinancialHistoryEvent result{
        .action =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.action),
                models::finance::FinancialHistoryAction{}),
        .actor = value.actor,
        .actor_name = value.actor_name,
        .cursor = Convert(value.cursor),
        .document =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.document),
                models::finance::FinancialHistoryDocument{}),
        .has_previous = value.has_previous};
    if (value.reference.has_value())
        result.reference = Convert(value.reference.value());
    for (const auto& change : value.changes) {
        models::finance::FinancialHistoryChange item{
            .field =
                models::finance::ParseEnumScalar(clients::finance::models::EnumValue(change.field),
                    models::finance::FinancialHistoryField{})};
        if (change.subject.has_value())
            item.subject = Convert(change.subject.value());
        if (change.before.has_value())
            item.before = Convert(change.before.value());
        if (change.after.has_value())
            item.after = Convert(change.after.value());
        result.changes.push_back(std::move(item));
    }
    return result;
}
}  // namespace web_bff
