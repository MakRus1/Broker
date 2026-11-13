#pragma once
#include <models/finance_settings_row.hpp>
#include <openapi/models.hpp>

namespace finance::converters {
inline models::FinanceSettings ToFinanceSettings(const models::FinanceSettingsRow& row) {
    return {.incoming_number_template = row.IncomingNumberTemplate,
        .outgoing_number_template = row.OutgoingNumberTemplate,
        .reporting_currency = row.Currency,
        .reset_numbers_yearly = row.ResetNumbersYearly,
        .revision = row.Revision};
}
}  // namespace finance::converters
