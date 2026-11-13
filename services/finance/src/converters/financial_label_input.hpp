#pragma once
#include <converters/financial_label.hpp>
#include <limits>
#include <models/errors.hpp>
#include <userver/utils/text_light.hpp>

namespace finance::converters {
inline models::pg::FinancialLabelRaw ToFinancialLabel(
    const models::SaveFinancialLabelRequest& value) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция финансовой метки"};
    const auto name = userver::utils::text::Trim(value.name);
    if (name.empty())
        throw ApiError<400>{"empty_name", "Укажите название финансовой метки"};
    if (value.vat_mode == models::VatMode::Exempt && value.vat_rate_basis_points.has_value())
        throw ApiError<400>{"unexpected_vat_rate", "Для «Без НДС» ставка не указывается"};
    if (value.vat_mode == models::VatMode::Rate && !value.vat_rate_basis_points.has_value())
        throw ApiError<400>{"vat_rate_required", "Укажите ставку НДС, включая нулевую"};
    auto note = value.note;
    if (note.has_value()) {
        note = userver::utils::text::Trim(note.value());
        if (note.value().empty())
            note.reset();
    }
    std::optional<std::int32_t> rate;
    if (value.vat_rate_basis_points.has_value())
        rate = static_cast<std::int32_t>(value.vat_rate_basis_points.value());
    return {.Id = boost::uuids::string_generator{}(value.id),
        .Name = name,
        .Direction = models::EnumValue(value.direction),
        .Unit = models::EnumValue(value.unit),
        .Currency = value.currency,
        .VatMode = models::EnumValue(value.vat_mode),
        .VatRateBasisPoints = rate,
        .Note = note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1};
}
}  // namespace finance::converters
