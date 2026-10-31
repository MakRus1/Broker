#pragma once
#include <helpers/exchange_amounts.hpp>
#include <helpers/financial_labels.hpp>
#include <helpers/service_charge_amounts.hpp>
#include <helpers/service_charge_references.hpp>
#include <limits>
#include <platform/date_validation.hpp>

namespace finance::helpers {
inline models::ServiceCharge Convert(const pgtypes::ServiceChargeV1& value) {
    models::ServiceCharge result;
    result.id = boost::uuids::to_string(value.Id);
    result.shipment_id = value.ShipmentId.has_value()
        ? std::optional{boost::uuids::to_string(value.ShipmentId.value())}
        : std::nullopt;
    result.transportation_id = value.TransportationId.has_value()
        ? std::optional{boost::uuids::to_string(value.TransportationId.value())}
        : std::nullopt;
    result.label_id = boost::uuids::to_string(value.LabelId);
    result.label_name = value.LabelName;
    result.direction = models::ParseEnumScalar(value.Direction, models::ServiceChargeDirection{});
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
    result.service_date = value.ServiceDate;
    result.invoice_currency = value.InvoiceCurrency;
    result.invoice_rate = {.date = value.InvoiceRateDate,
        .rate_millionths = value.InvoiceRateMillionths,
        .source = models::ParseEnumScalar(value.InvoiceRateSource, models::ExchangeRate_source{})};
    result.invoice_net_minor = value.InvoiceNetMinor;
    result.invoice_vat_minor = value.InvoiceVatMinor;
    result.invoice_total_minor = value.InvoiceTotalMinor;
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

inline std::vector<models::ServiceCharge> ConvertServiceCharges(const pg::ResultSet& rows) {
    std::vector<models::ServiceCharge> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<pgtypes::ServiceChargeV1>())
        result.push_back(Convert(row));
    return result;
}

inline pgtypes::ServiceChargeV1 PrepareServiceCharge(const models::SaveServiceChargeRequest& value,
    const models::FinancialLabel& label, const ServiceChargeParties& parties,
    const Rate& exchange_rate) {
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
    const auto amounts = CalculateAmounts(value);
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

inline std::vector<models::ServiceChargeInvoice> LoadServiceChargeInvoices(
    pg::Transaction& transaction, const std::string& organization,
    const std::vector<models::ServiceCharge>& items) {
    std::vector<models::ServiceChargeInvoice> result;
    std::vector<boost::uuids::uuid> service_charge_ids;
    for (const auto& charge : items)
        service_charge_ids.push_back(boost::uuids::string_generator{}(charge.id));
    const auto invoices =
        transaction.Execute(sql::kListServiceChargeInvoices, organization, service_charge_ids);
    for (const auto& row : invoices) {
        const auto [service_charge_id, invoice_id, number, direction, invoice_date, read_only] =
            row.As<boost::uuids::uuid, boost::uuids::uuid, std::string, std::string,
                userver::utils::datetime::Date, bool>();
        result.push_back(models::ServiceChargeInvoice{
            .direction = models::ParseEnumScalar(direction, models::InvoiceDirection{}),
            .invoice_date = invoice_date,
            .invoice_id = boost::uuids::to_string(invoice_id),
            .number = number,
            .read_only = read_only,
            .service_charge_id = boost::uuids::to_string(service_charge_id)});
    }
    return result;
}
}  // namespace finance::helpers
