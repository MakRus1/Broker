#pragma once
#include <helpers/finance_settings.hpp>
#include <helpers/service_charges.hpp>
#include <map>
#include <set>

namespace finance::helpers {
inline models::ReportingSummary BuildReporting(const Deps& deps, const std::string& currency,
    const pg::ResultSet& rows, bool transportation_scope) {
    struct Totals {
        WideAmount Income = 0;
        WideAmount Expense = 0;
        bool Present = false;
    } direct, transportations, combined;

    models::ReportingSummary result{.currency = currency, .missing_rates = {}, .summary = {}};
    using Date = userver::utils::datetime::Date;
    std::map<std::pair<Date, std::string>, std::optional<Rate>> rates;
    std::set<std::pair<Date, std::string>> missing;
    for (const auto& row : rows) {
        const auto [date, from, invoice_currency, total, invoice_total, direction, transportation] =
            row.As<Date, std::string, std::string, std::int64_t, std::int64_t, std::string,
                std::optional<boost::uuids::uuid>>();
        WideAmount amount;
        if (currency == invoice_currency)
            amount = invoice_total;
        else {
            const auto key = std::pair{date, from};
            if (!rates.contains(key)) {
                try {
                    rates.emplace(key, deps.exchange_rates.Get(from, currency, date));
                } catch (const ApiError<503>&) {
                    rates.emplace(key, std::nullopt);
                }
            }
            if (!rates.at(key).has_value()) {
                missing.insert(key);
                continue;
            }
            amount = ConvertAmount(WideAmount{total}, rates.at(key).value().Millionths);
        }
        auto& group =
            transportation.has_value() && !transportation_scope ? transportations : direct;
        group.Present = combined.Present = true;
        if (models::ParseEnumScalar(direction, models::ServiceChargeDirection{}) ==
            models::ServiceChargeDirection::Income) {
            group.Income += amount;
            combined.Income += amount;
        } else {
            group.Expense += amount;
            combined.Expense += amount;
        }
    }
    for (const auto& [date, from] : missing)
        result.missing_rates.push_back({.currency = from, .date = date});
    const auto append = [&](auto& target, const Totals& totals) {
        if (totals.Present)
            target.push_back({.currency = currency,
                .expense_minor = totals.Expense.str(),
                .income_minor = totals.Income.str()});
    };
    append(result.summary.own, direct);
    append(result.summary.transportations, transportations);
    append(result.summary.combined, combined);
    return result;
}
}  // namespace finance::helpers
