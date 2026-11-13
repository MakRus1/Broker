#pragma once
#include <converters/financial_label.hpp>
#include <models/pg/service_charge_raw.hpp>

namespace finance::converters {
inline models::ServiceCharge Convert(const models::pg::ServiceChargeRaw& value) {
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
}  // namespace finance::converters
