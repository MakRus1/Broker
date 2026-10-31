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
    const char* Label;
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
        return value.As<bool>() ? "Да" : "Нет";
    if (value.IsInt64())
        return std::to_string(value.As<std::int64_t>());
    return std::nullopt;
}

inline std::string Name(const Json& value, const char* key) {
    return Scalar(At(value, key)).value_or("");
}

inline std::string TextValue(const std::string& key, std::string value) {
    if (key == "direction") {
        if (value == "income")
            return "Доход";
        if (value == "expense")
            return "Расход";
        if (value == "incoming")
            return "Входящий";
        if (value == "outgoing")
            return "Исходящий";
    }
    if (key == "kind") {
        if (value == "receipt")
            return "Поступление";
        if (value == "disbursement")
            return "Списание";
    }
    if (key == "price_mode") {
        if (value == "net")
            return "Без НДС";
        if (value == "gross")
            return "С НДС";
    }
    if (key == "vat_mode") {
        if (value == "exempt")
            return "Без НДС";
        if (value == "rate")
            return "Ставка НДС";
    }
    if (key == "unit") {
        static const std::map<std::string, std::string> units{{"container", "Контейнер"},
            {"transportation", "Перевозка"}, {"ton", "Тонна"}, {"day", "Сутки"},
            {"piece", "Штука"}};
        if (const auto found = units.find(value); found != units.end())
            return found->second;
    }
    if (key == "invoice_rate.source") {
        if (value == "identity")
            return "Одинаковые валюты";
        if (value == "cbr")
            return "ЦБ";
        if (value == "manual")
            return "Вручную";
    }
    return value;
}

inline std::optional<models::FinancialHistoryValue> Value(
    const Json& document, const Field& field) {
    auto value = Scalar(At(document, field.Key));
    if (!value.has_value())
        return std::nullopt;
    models::FinancialHistoryValue result{.format = field.ValueFormat,
        .value = field.NameKey ? Scalar(At(document, field.NameKey)).value_or(value.value())
                               : TextValue(field.Key, value.value())};
    if (field.ValueFormat == Format::Money) {
        const std::string key = field.Key;
        result.currency =
            Scalar(At(document, key.starts_with("invoice_") ? "invoice_currency" : "currency"));
    }
    return result;
}

inline void AddChange(std::vector<models::FinancialHistoryChange>& changes, const Field& field,
    const Json& before, const Json& after, const std::string& prefix = "",
    const std::string& label = "") {
    const auto old = Scalar(At(before, field.Key));
    const auto current = Scalar(At(after, field.Key));
    // Старые снимки могут не содержать недавно добавленных полей.
    if (old == current &&
        (!field.NameKey || Scalar(At(before, field.NameKey)) == Scalar(At(after, field.NameKey))))
        return;
    auto old_value = Value(before, field);
    auto new_value = Value(after, field);
    if (field.NameKey && old != current && old_value.has_value() && new_value.has_value() &&
        old_value.value().value == new_value.value().value) {
        old_value.value().value += " (" + old.value_or("") + ")";
        new_value.value().value += " (" + current.value_or("") + ")";
    }
    changes.push_back({.after = std::move(new_value),
        .before = std::move(old_value),
        .field = prefix + field.Key,
        .label = label + field.Label});
}

inline constexpr std::array kChargeFields{
    Field{"label_id", "Финансовая метка", Format::Text, "label_name"},
    Field{"direction", "Направление"},
    Field{"counterparty_id", "Контрагент", Format::Text, "counterparty_name"},
    Field{"contract_id", "Договор", Format::Text, "contract_number"},
    Field{"shipment_id", "Поставка"}, Field{"transportation_id", "Перевозка"},
    Field{"quantity_milli", "Количество", Format::Quantity}, Field{"unit", "Единица расчёта"},
    Field{"price_minor", "Цена", Format::Money}, Field{"currency", "Валюта услуги"},
    Field{"service_date", "Дата услуги", Format::Date}, Field{"invoice_currency", "Валюта счёта"},
    Field{"invoice_rate.rate_millionths", "Курс", Format::Rate},
    Field{"invoice_rate.date", "Дата курса", Format::Date},
    Field{"invoice_rate.source", "Источник курса"}, Field{"price_mode", "Цена указана"},
    Field{"vat_mode", "НДС"}, Field{"vat_rate_basis_points", "Ставка НДС", Format::Percent},
    Field{"net_minor", "Без НДС", Format::Money}, Field{"vat_minor", "НДС", Format::Money},
    Field{"total_minor", "Итого", Format::Money},
    Field{"invoice_net_minor", "Без НДС в валюте счёта", Format::Money},
    Field{"invoice_vat_minor", "НДС в валюте счёта", Format::Money},
    Field{"invoice_total_minor", "Итого в валюте счёта", Format::Money},
    Field{"note", "Примечание"}, Field{"archived", "В архиве"}};
inline constexpr std::array kInvoiceFields{Field{"number", "Номер счёта"},
    Field{"direction", "Вид счёта"}, Field{"invoice_date", "Дата счёта", Format::Date},
    Field{"due_date", "Плановая оплата", Format::Date},
    Field{"own_company_id", "Своё юрлицо", Format::Text, "own_company_name"},
    Field{"counterparty_id", "Контрагент", Format::Text, "counterparty_name"},
    Field{"own_bank_account_id", "Банковский счёт своего юрлица"},
    Field{"counterparty_bank_account_id", "Банковский счёт контрагента"},
    Field{"contract_id", "Договор", Format::Text, "contract_number"}, Field{"currency", "Валюта"},
    Field{"net_minor", "Без НДС", Format::Money}, Field{"vat_minor", "НДС", Format::Money},
    Field{"total_minor", "Итого", Format::Money}, Field{"note", "Примечание"},
    Field{"archived", "В архиве"}};
inline constexpr std::array kPaymentFields{Field{"number", "Номер платежа"},
    Field{"kind", "Вид платежа"}, Field{"payment_date", "Дата платежа", Format::Date},
    Field{"own_company_id", "Своё юрлицо", Format::Text, "own_company_name"},
    Field{"counterparty_id", "Контрагент", Format::Text, "counterparty_name"},
    Field{"own_bank_account_id", "Банковский счёт своего юрлица"},
    Field{"counterparty_bank_account_id", "Банковский счёт контрагента"},
    Field{"currency", "Валюта"}, Field{"amount_minor", "Сумма", Format::Money},
    Field{"purpose", "Назначение платежа"}, Field{"note", "Примечание"},
    Field{"archived", "В архиве"}};

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
