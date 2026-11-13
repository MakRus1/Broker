#pragma once
#include <boost/uuid/string_generator.hpp>
#include <converters/financial_label.hpp>
#include <helpers/exchange_amounts.hpp>
#include <models/conversion_rate.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <models/service_charge_parties.hpp>
#include <platform/charge_amounts.hpp>
#include <platform/date_validation.hpp>

namespace finance::converters {
using namespace helpers;

inline models::pg::ServiceChargeRaw ToServiceCharge(const models::SaveServiceChargeRequest& value,
    const models::FinancialLabel& label, const models::ServiceChargeParties& parties,
    const models::ConversionRate& exchange_rate) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция услуги"};
    if (label.archived && !value.archived)
        throw ApiError<409>{"archived_label", "Выберите действующую финансовую метку"};
    if (label.direction != models::FinancialLabelDirection::Both &&
        models::EnumValue(label.direction) != models::EnumValue(value.direction))
        throw ApiError<400>{"wrong_direction", "Метка не подходит для выбранного направления"};
    if (!platform::IsCalendarDateInRange(
            value.service_date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_service_date", "Укажите дату услуги не раньше 01.01.1900"};
    ValidateCurrency(value.currency);
    ValidateCurrency(value.invoice_currency);
    const auto amounts = platform::CalculateChargeAmounts(value.quantity_milli, value.price_minor,
        value.price_mode == models::PriceMode::Gross, value.vat_mode == models::VatMode::Exempt,
        value.vat_rate_basis_points);
    const auto invoice_net = ConvertAmount(amounts.Net, exchange_rate.Millionths);
    const auto invoice_total = ConvertAmount(amounts.Total, exchange_rate.Millionths);
    auto note = value.note;
    if (note.has_value()) {
        note = userver::utils::text::Trim(note.value());
        if (note.value().empty())
            note.reset();
    }
    return {
        .Id = boost::uuids::string_generator{}(value.id),
        .ShipmentId = value.shipment_id.has_value()
            ? std::optional{boost::uuids::string_generator{}(value.shipment_id.value())}
            : std::nullopt,
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
        .ServiceDate = value.service_date,
        .InvoiceCurrency = value.invoice_currency,
        .InvoiceRateMillionths = exchange_rate.Millionths,
        .InvoiceRateDate = exchange_rate.Date,
        .InvoiceRateSource = exchange_rate.Source,
        .InvoiceNetMinor = invoice_net,
        .InvoiceVatMinor = invoice_total - invoice_net,
        .InvoiceTotalMinor = invoice_total,
    };
}
}  // namespace finance::converters
