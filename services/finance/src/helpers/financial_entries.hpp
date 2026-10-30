#pragma once
#include <helpers/entry_amounts.hpp>
#include <helpers/entry_references.hpp>
#include <helpers/financial_labels.hpp>
#include <limits>

namespace finance::helpers {
inline models::FinancialEntry Convert(const pgtypes::FinancialEntryV1& value) {
    models::FinancialEntry result;
    result.id = boost::uuids::to_string(value.Id);
    result.shipment_id = boost::uuids::to_string(value.ShipmentId);
    result.transportation_id = value.TransportationId.has_value()
        ? std::optional{boost::uuids::to_string(value.TransportationId.value())}
        : std::nullopt;
    result.label_id = boost::uuids::to_string(value.LabelId);
    result.label_name = value.LabelName;
    result.direction = models::ParseEnumScalar(value.Direction, models::EntryDirection{});
    result.unit = models::ParseEnumScalar(value.Unit, models::FinancialLabelUnit{});
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.counterparty_name = value.CounterpartyName;
    result.contract_id = value.ContractId.has_value()
        ? std::optional{boost::uuids::to_string(value.ContractId.value())}
        : std::nullopt;
    result.contract_number = value.ContractNumber;
    result.quantity_milli = value.QuantityMilli;
    result.price_minor = value.PriceMinor;
    result.currency = value.Currency;
    result.vat_mode = models::ParseEnumScalar(value.VatMode, models::VatMode{});
    result.vat_rate_basis_points = value.VatRateBasisPoints;
    result.net_minor = value.NetMinor;
    result.vat_minor = value.VatMinor;
    result.total_minor = value.TotalMinor;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    result.price_mode = models::ParseEnumScalar(value.PriceMode, models::PriceMode{});
    return result;
}

inline std::vector<models::FinancialEntry> ConvertEntries(const pg::ResultSet& rows) {
    std::vector<models::FinancialEntry> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<pgtypes::FinancialEntryV1>())
        result.push_back(Convert(row));
    return result;
}

inline pgtypes::FinancialEntryV1 PrepareEntry(const models::SaveFinancialEntryRequest& value,
    const models::FinancialLabel& label, const EntryParties& parties) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция начисления"};
    if (label.archived && !value.archived)
        throw ApiError<409>{"archived_label", "Выберите действующую финансовую метку"};
    if (label.direction != models::FinancialLabelDirection::Both &&
        models::EnumValue(label.direction) != models::EnumValue(value.direction))
        throw ApiError<400>{"wrong_direction", "Метка не подходит для выбранного направления"};
    const auto amounts = CalculateAmounts(value);
    auto note = value.note;
    if (note.has_value()) {
        note = userver::utils::text::Trim(note.value());
        if (note.value().empty())
            note.reset();
    }
    return {
        .Id = boost::uuids::string_generator{}(value.id),
        .ShipmentId = boost::uuids::string_generator{}(value.shipment_id),
        .TransportationId = value.transportation_id.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.transportation_id.value())}
            : std::nullopt,
        .LabelId = boost::uuids::string_generator{}(value.label_id),
        .LabelName = label.name,
        .Direction = models::EnumValue(value.direction),
        .Unit = models::EnumValue(value.unit),
        .CounterpartyId = boost::uuids::string_generator{}(value.counterparty_id),
        .CounterpartyName = parties.CounterpartyName,
        .ContractId = value.contract_id.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.contract_id.value())}
            : std::nullopt,
        .ContractNumber = parties.ContractNumber,
        .QuantityMilli = value.quantity_milli,
        .PriceMinor = value.price_minor,
        .Currency = value.currency,
        .VatMode = models::EnumValue(value.vat_mode),
        .VatRateBasisPoints = value.vat_rate_basis_points.has_value()
            ? std::optional{static_cast<std::int32_t>(value.vat_rate_basis_points.value())}
            : std::nullopt,
        .NetMinor = amounts.Net,
        .VatMinor = amounts.Vat,
        .TotalMinor = amounts.Total,
        .Note = note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1,
        .PriceMode = models::EnumValue(value.price_mode),
    };
}
}  // namespace finance::helpers
