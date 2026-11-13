#include <boost/uuid/string_generator.hpp>
#include <helpers/contract_validation.hpp>
#include <limits>
#include <models/errors.hpp>
#include <platform/date_validation.hpp>
#include <userver/utils/text_light.hpp>

namespace contracts::helpers {
void NormalizeAndValidateContract(models::SaveContractRequest& value) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция договора"};
    const auto number = userver::utils::text::Trim(value.number.value_or(""));
    if (number.empty() && (value.number.has_value() || value.expected_revision != 0))
        throw ApiError<400>{"empty_number", "Укажите номер договора"};
    for (const auto& date : {std::optional{value.contract_date}, value.valid_until}) {
        if (date.has_value() &&
            !platform::IsCalendarDateInRange(
                date.value(), std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    }
    if (value.valid_until.has_value() && value.valid_until.value() < value.contract_date)
        throw ApiError<400>{
            "invalid_valid_until", "Срок действия не может быть раньше даты договора"};

    const auto own = boost::uuids::string_generator{}(value.own_company_id);
    const auto counterparty = boost::uuids::string_generator{}(value.counterparty_id);
    if (own == counterparty)
        throw ApiError<400>{"same_parties", "Выберите разные стороны договора"};

    value.number = number;
    if (value.note.has_value()) {
        value.note = userver::utils::text::Trim(value.note.value());
        if (value.note.value().empty())
            value.note.reset();
    }
}

}  // namespace contracts::helpers
