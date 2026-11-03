#pragma once
#include <array>
#include <map>
#include <openapi/models.hpp>
#include <set>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>

namespace finance::helpers::history {
using Json = userver::formats::json::Value;
using Format = models::FinancialHistoryFormat;

struct Field {
    const char* Key;
    Format ValueFormat = Format::Text;
    const char* NameKey = nullptr;
};

inline Json At(const Json& value, std::string_view key) {
    if (!value.IsObject())
        return Json{};
    const auto dot = key.find('.');
    if (dot == std::string_view::npos)
        return value[std::string{key}];
    return At(value[std::string{key.substr(0, dot)}], key.substr(dot + 1));
}

inline std::optional<std::string> Scalar(const Json& value) {
    if (value.IsMissing() || value.IsNull())
        return std::nullopt;
    if (value.IsString())
        return value.As<std::string>();
    if (value.IsBool())
        return value.As<bool>() ? "true" : "false";
    if (value.IsInt64())
        return std::to_string(value.As<std::int64_t>());
    return std::nullopt;
}

inline std::string Name(const Json& value, const char* key) {
    return Scalar(At(value, key)).value_or("");
}

inline std::optional<models::FinancialHistoryValue> Value(
    const Json& document, const Field& field) {
    auto value = Scalar(At(document, field.Key));
    if (!value.has_value())
        return std::nullopt;
    models::FinancialHistoryValue result{.format = field.ValueFormat, .value = value.value()};
    if (field.ValueFormat == Format::Reference && field.NameKey)
        result.display_name = Scalar(At(document, field.NameKey));
    if (field.ValueFormat == Format::Money) {
        const std::string key = field.Key;
        result.currency =
            Scalar(At(document, key.starts_with("invoice_") ? "invoice_currency" : "currency"));
    }
    return result;
}

inline void AddChange(std::vector<models::FinancialHistoryChange>& changes, const Field& field,
    const Json& before, const Json& after,
    const std::optional<models::FinancialHistoryReference>& subject = std::nullopt) {
    const auto old = Scalar(At(before, field.Key));
    const auto current = Scalar(At(after, field.Key));
    // Старые снимки могут не содержать недавно добавленных полей.
    if (old == current &&
        (!field.NameKey || Scalar(At(before, field.NameKey)) == Scalar(At(after, field.NameKey))))
        return;
    changes.push_back({.after = Value(after, field),
        .before = Value(before, field),
        .field = models::ParseEnumScalar(field.Key, models::FinancialHistoryField{}),
        .subject = subject});
}

inline constexpr std::array kChargeFields{Field{"label_id", Format::Reference, "label_name"},
    Field{"direction", Format::Enum},
    Field{"counterparty_id", Format::Reference, "counterparty_name"},
    Field{"contract_id", Format::Reference, "contract_number"},
    Field{"shipment_id", Format::Reference}, Field{"transportation_id", Format::Reference},
    Field{"quantity_milli", Format::Quantity}, Field{"unit", Format::Enum},
    Field{"price_minor", Format::Money}, Field{"currency"}, Field{"service_date", Format::Date},
    Field{"invoice_currency"}, Field{"invoice_rate.rate_millionths", Format::Rate},
    Field{"invoice_rate.date", Format::Date}, Field{"invoice_rate.source", Format::Enum},
    Field{"price_mode", Format::Enum}, Field{"vat_mode", Format::Enum},
    Field{"vat_rate_basis_points", Format::Percent}, Field{"net_minor", Format::Money},
    Field{"vat_minor", Format::Money}, Field{"total_minor", Format::Money},
    Field{"invoice_net_minor", Format::Money}, Field{"invoice_vat_minor", Format::Money},
    Field{"invoice_total_minor", Format::Money}, Field{"note"}, Field{"archived", Format::Boolean}};
inline constexpr std::array kInvoiceFields{Field{"number"}, Field{"direction", Format::Enum},
    Field{"invoice_date", Format::Date}, Field{"due_date", Format::Date},
    Field{"own_company_id", Format::Reference, "own_company_name"},
    Field{"counterparty_id", Format::Reference, "counterparty_name"},
    Field{"own_bank_account_id", Format::Reference},
    Field{"counterparty_bank_account_id", Format::Reference},
    Field{"contract_id", Format::Reference, "contract_number"}, Field{"currency"},
    Field{"net_minor", Format::Money}, Field{"vat_minor", Format::Money},
    Field{"total_minor", Format::Money}, Field{"note"}, Field{"archived", Format::Boolean}};
inline constexpr std::array kPaymentFields{Field{"number"}, Field{"kind", Format::Enum},
    Field{"payment_date", Format::Date},
    Field{"own_company_id", Format::Reference, "own_company_name"},
    Field{"counterparty_id", Format::Reference, "counterparty_name"},
    Field{"own_bank_account_id", Format::Reference},
    Field{"counterparty_bank_account_id", Format::Reference}, Field{"currency"},
    Field{"amount_minor", Format::Money}, Field{"purpose"}, Field{"note"},
    Field{"archived", Format::Boolean}};

inline std::map<std::string, Json> Indexed(const Json& rows, const char* key) {
    std::map<std::string, Json> result;
    if (!rows.IsArray())
        return result;
    for (const auto& row : rows)
        result.emplace(Name(row, key), row);
    return result;
}

inline Json Empty() {
    return userver::formats::json::Value{};
}

inline Json Find(const std::map<std::string, Json>& rows, const std::string& id) {
    const auto found = rows.find(id);
    return found == rows.end() ? Empty() : found->second;
}
}  // namespace finance::helpers::history
