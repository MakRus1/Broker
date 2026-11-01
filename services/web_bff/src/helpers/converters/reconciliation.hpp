#pragma once
#include <clients/finance/client.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline models::finance::ReconciliationTotal Convert(
    const clients::finance::models::ReconciliationTotal& value) {
    return {.closing_minor = value.closing_minor,
        .credit_minor = value.credit_minor,
        .currency = value.currency,
        .debit_minor = value.debit_minor,
        .opening_minor = value.opening_minor};
}

inline models::finance::ReconciliationOperation Convert(
    const clients::finance::models::ReconciliationOperation& value) {
    return {.balance_minor = value.balance_minor,
        .credit_minor = value.credit_minor,
        .currency = value.currency,
        .date = value.date,
        .debit_minor = value.debit_minor,
        .id = value.id,
        .kind = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.kind), models::finance::ReconciliationKind{}),
        .number = value.number,
        .own_company_name = value.own_company_name};
}
}  // namespace web_bff
