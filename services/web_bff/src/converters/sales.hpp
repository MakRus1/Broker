#pragma once
#include <clients/sales/client.hpp>
#include <openapi/models.hpp>
#include <utility>
#include <vector>

namespace web_bff::converters::sales {
inline clients::sales::models::ContactKind Convert(const models::sales::ContactKind& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::ContactKind{});
}

template <typename Result = clients::sales::models::AddSalesContactRequest>
Result Convert(const models::sales::AddSalesContactRequest& value) {
    Result result;
    result.card_id = value.card_id;
    result.contact_date = value.contact_date;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    result.kind = Convert(value.kind);
    result.next_action = value.next_action;
    result.next_action_date = value.next_action_date;
    result.result = value.result;
    return result;
}

inline clients::sales::models::TransportLocationKind Convert(
    const models::sales::TransportLocationKind& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::TransportLocationKind{});
}

inline clients::sales::models::TransportLocation Convert(
    const models::sales::TransportLocation& value) {
    clients::sales::models::TransportLocation result;
    result.code = value.code;
    result.country_code = value.country_code;
    result.id = value.id;
    result.kind = Convert(value.kind);
    result.name = value.name;
    return result;
}

inline clients::sales::models::AirDetails Convert(const models::sales::AirDetails& value) {
    clients::sales::models::AirDetails result;
    result.arrival_date = value.arrival_date;
    result.carrier_id = value.carrier_id;
    result.departure_date = value.departure_date;
    if (value.destination_airport.has_value()) {
        result.destination_airport.emplace();
        result.destination_airport.value() = Convert(value.destination_airport.value());
    }
    result.eta = value.eta;
    result.etd = value.etd;
    result.flight_number = value.flight_number;
    if (value.origin_airport.has_value()) {
        result.origin_airport.emplace();
        result.origin_airport.value() = Convert(value.origin_airport.value());
    }
    return result;
}

inline clients::sales::models::PriceMode Convert(const models::sales::PriceMode& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::PriceMode{});
}

inline clients::sales::models::VatMode Convert(const models::sales::VatMode& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::VatMode{});
}

inline clients::sales::models::ProposalPrice Convert(const models::sales::ProposalPrice& value) {
    clients::sales::models::ProposalPrice result;
    result.currency = value.currency;
    result.price_minor = value.price_minor;
    result.price_mode = Convert(value.price_mode);
    result.vat_mode = Convert(value.vat_mode);
    result.vat_rate_basis_points = value.vat_rate_basis_points;
    return result;
}

inline clients::sales::models::ProposalQuoteSource Convert(
    const models::sales::ProposalQuoteSource& value) {
    clients::sales::models::ProposalQuoteSource result;
    result.line_id = value.line_id;
    result.quote_id = value.quote_id;
    result.revision = value.revision;
    result.rfq_id = value.rfq_id;
    return result;
}

inline clients::sales::models::FinancialLabelUnit Convert(
    const models::sales::FinancialLabelUnit& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::FinancialLabelUnit{});
}

inline clients::sales::models::ProposalLine Convert(const models::sales::ProposalLine& value) {
    clients::sales::models::ProposalLine result;
    result.id = value.id;
    result.label_id = value.label_id;
    result.note = value.note;
    if (value.purchase.has_value()) {
        result.purchase.emplace();
        result.purchase.value() = Convert(value.purchase.value());
    }
    result.quantity_milli = value.quantity_milli;
    if (value.quote_source.has_value()) {
        result.quote_source.emplace();
        result.quote_source.value() = Convert(value.quote_source.value());
    }
    result.route_segment_id = value.route_segment_id;
    result.sale = Convert(value.sale);
    result.supplier_id = value.supplier_id;
    result.unit = Convert(value.unit);
    return result;
}

template <typename Result = clients::sales::models::CreateShipmentRequest>
Result Convert(const models::sales::CreateShipmentRequest& value) {
    Result result;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    return result;
}

template <typename Result = clients::sales::models::HistoryRequest>
Result Convert(const models::sales::HistoryRequest& value) {
    Result result;
    result.after_revision = value.after_revision;
    result.id = value.id;
    return result;
}

template <typename Result = clients::sales::models::IdRequest>
Result Convert(const models::sales::IdRequest& value) {
    Result result;
    result.id = value.id;
    return result;
}

inline clients::sales::models::SeaDetails Convert(const models::sales::SeaDetails& value) {
    clients::sales::models::SeaDetails result;
    result.arrival_terminal_id = value.arrival_terminal_id;
    result.departure_date = value.departure_date;
    result.departure_terminal_id = value.departure_terminal_id;
    result.eta = value.eta;
    if (value.pod.has_value()) {
        result.pod.emplace();
        result.pod.value() = Convert(value.pod.value());
    }
    if (value.pol.has_value()) {
        result.pol.emplace();
        result.pol.value() = Convert(value.pol.value());
    }
    result.vessel = value.vessel;
    result.voyage = value.voyage;
    return result;
}

inline clients::sales::models::ShipmentRouteSegment Convert(
    const models::sales::ShipmentRouteSegment& value) {
    clients::sales::models::ShipmentRouteSegment result;
    if (value.air.has_value()) {
        result.air.emplace();
        result.air.value() = Convert(value.air.value());
    }
    result.id = value.id;
    if (value.sea.has_value()) {
        result.sea.emplace();
        result.sea.value() = Convert(value.sea.value());
    }
    result.type = value.type;
    return result;
}

inline clients::sales::models::InquiryStatus Convert(const models::sales::InquiryStatus& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::InquiryStatus{});
}

template <typename Result = clients::sales::models::ListSalesContactsRequest>
Result Convert(const models::sales::ListSalesContactsRequest& value) {
    Result result;
    result.before = value.before;
    result.card_id = value.card_id;
    result.limit = value.limit;
    return result;
}

inline clients::sales::models::SalesRegistryCursor Convert(
    const models::sales::SalesRegistryCursor& value) {
    clients::sales::models::SalesRegistryCursor result;
    result.date_value = value.date_value;
    result.id = value.id;
    result.scope = value.scope;
    result.text_value = value.text_value;
    return result;
}

inline clients::sales::models::RegistryFilter Convert(const models::sales::RegistryFilter& value) {
    clients::sales::models::RegistryFilter result;
    result.date_from = value.date_from;
    result.date_to = value.date_to;
    result.empty = value.empty;
    result.field = value.field;
    result.query = value.query;
    result.values = value.values;
    return result;
}

template <typename Result = clients::sales::models::ListSalesRequest>
Result Convert(const models::sales::ListSalesRequest& value) {
    Result result;
    if (value.cursor.has_value()) {
        result.cursor.emplace();
        result.cursor.value() = Convert(value.cursor.value());
    }
    result.customer_id = value.customer_id;
    result.descending = value.descending;
    if (value.filters.has_value()) {
        result.filters.emplace();
        result.filters.value().reserve(value.filters.value().size());
        for (const auto& item : value.filters.value())
            result.filters.value().push_back(Convert(item));
    }
    result.limit = value.limit;
    result.shipment_id = value.shipment_id;
    result.sort = value.sort;
    return result;
}

inline clients::sales::models::ProposalActionRequest_action Convert(
    const models::sales::ProposalActionRequest_action& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::ProposalActionRequest_action{});
}

template <typename Result = clients::sales::models::ProposalActionRequest>
Result Convert(const models::sales::ProposalActionRequest& value) {
    Result result;
    result.action = Convert(value.action);
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    return result;
}

inline clients::sales::models::RfqQuoteLine Convert(const models::sales::RfqQuoteLine& value) {
    clients::sales::models::RfqQuoteLine result;
    result.id = value.id;
    result.label_id = value.label_id;
    result.label_name = value.label_name;
    result.price = Convert(value.price);
    result.quantity_milli = value.quantity_milli;
    result.unit = Convert(value.unit);
    return result;
}

inline clients::sales::models::RfqQuote Convert(const models::sales::RfqQuote& value) {
    clients::sales::models::RfqQuote result;
    result.delivery_days = value.delivery_days;
    result.id = value.id;
    result.lines.reserve(value.lines.size());
    for (const auto& item : value.lines)
        result.lines.push_back(Convert(item));
    result.note = value.note;
    result.supplier_id = value.supplier_id;
    result.supplier_name = value.supplier_name;
    result.title = value.title;
    result.valid_until = value.valid_until;
    return result;
}

inline clients::sales::models::RfqStatus Convert(const models::sales::RfqStatus& value) {
    return clients::sales::models::ParseEnumScalar(
        models::sales::EnumValue(value), clients::sales::models::RfqStatus{});
}

template <typename Result = clients::sales::models::SalesFilterValuesRequest>
Result Convert(const models::sales::SalesFilterValuesRequest& value) {
    Result result;
    result.after = value.after;
    result.candidates = value.candidates;
    result.field = value.field;
    result.limit = value.limit;
    result.query = value.query;
    result.registry = Convert(value.registry);
    return result;
}

template <typename Result = clients::sales::models::SaveInquiryRequest>
Result Convert(const models::sales::SaveInquiryRequest& value) {
    Result result;
    result.cargo_description = value.cargo_description;
    result.carriage_terms = value.carriage_terms;
    result.customer_id = value.customer_id;
    result.delivery_address = value.delivery_address;
    result.delivery_city = value.delivery_city;
    result.delivery_country = value.delivery_country;
    result.desired_delivery_date = value.desired_delivery_date;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    result.incoterms = value.incoterms;
    result.loading = value.loading;
    result.manager_subject = value.manager_subject;
    result.note = value.note;
    result.ready_date = value.ready_date;
    result.route_segments.reserve(value.route_segments.size());
    for (const auto& item : value.route_segments)
        result.route_segments.push_back(Convert(item));
    result.status = Convert(value.status);
    return result;
}

template <typename Result = clients::sales::models::SaveProposalRequest>
Result Convert(const models::sales::SaveProposalRequest& value) {
    Result result;
    result.calculation_date = value.calculation_date;
    result.delivery_days = value.delivery_days;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    result.inquiry_id = value.inquiry_id;
    result.lines.reserve(value.lines.size());
    for (const auto& item : value.lines)
        result.lines.push_back(Convert(item));
    result.note = value.note;
    result.title = value.title;
    result.valid_until = value.valid_until;
    return result;
}

template <typename Result = clients::sales::models::SaveRfqRequest>
Result Convert(const models::sales::SaveRfqRequest& value) {
    Result result;
    result.answer_by = value.answer_by;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    result.inquiry_id = value.inquiry_id;
    result.note = value.note;
    result.quotes.reserve(value.quotes.size());
    for (const auto& item : value.quotes)
        result.quotes.push_back(Convert(item));
    result.responsible_subject = value.responsible_subject;
    result.route_segment_id = value.route_segment_id;
    result.status = Convert(value.status);
    result.title = value.title;
    return result;
}

template <typename Result = clients::sales::models::SaveSalesCardRequest>
Result Convert(const models::sales::SaveSalesCardRequest& value) {
    Result result;
    result.customer_id = value.customer_id;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    result.manager_subject = value.manager_subject;
    result.next_action = value.next_action;
    result.next_action_date = value.next_action_date;
    result.note = value.note;
    result.source = value.source;
    result.stage_id = value.stage_id;
    return result;
}

template <typename Result = clients::sales::models::UpdateSalesContactRequest>
Result Convert(const models::sales::UpdateSalesContactRequest& value) {
    Result result;
    result.card_id = value.card_id;
    result.contact_date = value.contact_date;
    result.expected_revision = value.expected_revision;
    result.id = value.id;
    result.kind = Convert(value.kind);
    result.next_action = value.next_action;
    result.next_action_date = value.next_action_date;
    result.result = value.result;
    return result;
}

inline models::sales::ContactKind Convert(const clients::sales::models::ContactKind& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::ContactKind{});
}

inline models::sales::TransportLocationKind Convert(
    const clients::sales::models::TransportLocationKind& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::TransportLocationKind{});
}

inline models::sales::TransportLocation Convert(
    const clients::sales::models::TransportLocation& value) {
    models::sales::TransportLocation result;
    result.code = value.code;
    result.country_code = value.country_code;
    result.id = value.id;
    result.kind = Convert(value.kind);
    result.name = value.name;
    return result;
}

inline models::sales::AirDetails Convert(const clients::sales::models::AirDetails& value) {
    models::sales::AirDetails result;
    result.arrival_date = value.arrival_date;
    result.carrier_id = value.carrier_id;
    result.departure_date = value.departure_date;
    if (value.destination_airport.has_value()) {
        result.destination_airport.emplace();
        result.destination_airport.value() = Convert(value.destination_airport.value());
    }
    result.eta = value.eta;
    result.etd = value.etd;
    result.flight_number = value.flight_number;
    if (value.origin_airport.has_value()) {
        result.origin_airport.emplace();
        result.origin_airport.value() = Convert(value.origin_airport.value());
    }
    return result;
}

inline models::sales::PriceMode Convert(const clients::sales::models::PriceMode& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::PriceMode{});
}

inline models::sales::VatMode Convert(const clients::sales::models::VatMode& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::VatMode{});
}

inline models::sales::ProposalPrice Convert(const clients::sales::models::ProposalPrice& value) {
    models::sales::ProposalPrice result;
    result.currency = value.currency;
    result.price_minor = value.price_minor;
    result.price_mode = Convert(value.price_mode);
    result.vat_mode = Convert(value.vat_mode);
    result.vat_rate_basis_points = value.vat_rate_basis_points;
    return result;
}

inline models::sales::ProposalQuoteSource Convert(
    const clients::sales::models::ProposalQuoteSource& value) {
    models::sales::ProposalQuoteSource result;
    result.line_id = value.line_id;
    result.quote_id = value.quote_id;
    result.revision = value.revision;
    result.rfq_id = value.rfq_id;
    return result;
}

inline models::sales::FinancialLabelUnit Convert(
    const clients::sales::models::FinancialLabelUnit& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::FinancialLabelUnit{});
}

inline models::sales::ProposalLine Convert(const clients::sales::models::ProposalLine& value) {
    models::sales::ProposalLine result;
    result.id = value.id;
    result.label_id = value.label_id;
    result.note = value.note;
    if (value.purchase.has_value()) {
        result.purchase.emplace();
        result.purchase.value() = Convert(value.purchase.value());
    }
    result.quantity_milli = value.quantity_milli;
    if (value.quote_source.has_value()) {
        result.quote_source.emplace();
        result.quote_source.value() = Convert(value.quote_source.value());
    }
    result.route_segment_id = value.route_segment_id;
    result.sale = Convert(value.sale);
    result.supplier_id = value.supplier_id;
    result.unit = Convert(value.unit);
    return result;
}

inline models::sales::ProposalAmounts Convert(
    const clients::sales::models::ProposalAmounts& value) {
    models::sales::ProposalAmounts result;
    result.net_minor = value.net_minor;
    result.total_minor = value.total_minor;
    result.vat_minor = value.vat_minor;
    return result;
}

inline models::sales::CalculatedProposalLine Convert(
    const clients::sales::models::CalculatedProposalLine& value) {
    models::sales::CalculatedProposalLine result;
    result.input = Convert(value.input);
    result.label_name = value.label_name;
    if (value.purchase.has_value()) {
        result.purchase.emplace();
        result.purchase.value() = Convert(value.purchase.value());
    }
    result.sale = Convert(value.sale);
    result.supplier_name = value.supplier_name;
    return result;
}

inline models::sales::SeaDetails Convert(const clients::sales::models::SeaDetails& value) {
    models::sales::SeaDetails result;
    result.arrival_terminal_id = value.arrival_terminal_id;
    result.departure_date = value.departure_date;
    result.departure_terminal_id = value.departure_terminal_id;
    result.eta = value.eta;
    if (value.pod.has_value()) {
        result.pod.emplace();
        result.pod.value() = Convert(value.pod.value());
    }
    if (value.pol.has_value()) {
        result.pol.emplace();
        result.pol.value() = Convert(value.pol.value());
    }
    result.vessel = value.vessel;
    result.voyage = value.voyage;
    return result;
}

inline models::sales::ShipmentRouteSegment Convert(
    const clients::sales::models::ShipmentRouteSegment& value) {
    models::sales::ShipmentRouteSegment result;
    if (value.air.has_value()) {
        result.air.emplace();
        result.air.value() = Convert(value.air.value());
    }
    result.id = value.id;
    if (value.sea.has_value()) {
        result.sea.emplace();
        result.sea.value() = Convert(value.sea.value());
    }
    result.type = value.type;
    return result;
}

inline models::sales::InquiryStatus Convert(const clients::sales::models::InquiryStatus& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::InquiryStatus{});
}

inline models::sales::Inquiry Convert(const clients::sales::models::Inquiry& value) {
    models::sales::Inquiry result;
    result.accepted_proposal_id = value.accepted_proposal_id;
    result.accepted_proposal_revision = value.accepted_proposal_revision;
    result.cargo_description = value.cargo_description;
    result.carriage_terms = value.carriage_terms;
    result.conversion_error = value.conversion_error;
    result.converting = value.converting;
    result.customer_id = value.customer_id;
    result.delivery_address = value.delivery_address;
    result.delivery_city = value.delivery_city;
    result.delivery_country = value.delivery_country;
    result.desired_delivery_date = value.desired_delivery_date;
    result.id = value.id;
    result.incoterms = value.incoterms;
    result.loading = value.loading;
    result.manager_subject = value.manager_subject;
    result.note = value.note;
    result.number = value.number;
    result.organization_id = value.organization_id;
    result.ready_date = value.ready_date;
    result.revision = value.revision;
    result.route_segments.reserve(value.route_segments.size());
    for (const auto& item : value.route_segments)
        result.route_segments.push_back(Convert(item));
    result.shipment_id = value.shipment_id;
    result.status = Convert(value.status);
    return result;
}

inline models::sales::InquiryHistoryEntry Convert(
    const clients::sales::models::InquiryHistoryEntry& value) {
    models::sales::InquiryHistoryEntry result;
    result.actor = value.actor;
    result.created_at = value.created_at;
    result.revision = value.revision;
    result.snapshot = Convert(value.snapshot);
    return result;
}

inline models::sales::SalesRegistryCursor Convert(
    const clients::sales::models::SalesRegistryCursor& value) {
    models::sales::SalesRegistryCursor result;
    result.date_value = value.date_value;
    result.id = value.id;
    result.scope = value.scope;
    result.text_value = value.text_value;
    return result;
}

inline models::sales::ProposalInput Convert(const clients::sales::models::ProposalInput& value) {
    models::sales::ProposalInput result;
    result.calculation_date = value.calculation_date;
    result.delivery_days = value.delivery_days;
    result.id = value.id;
    result.inquiry_id = value.inquiry_id;
    result.lines.reserve(value.lines.size());
    for (const auto& item : value.lines)
        result.lines.push_back(Convert(item));
    result.note = value.note;
    result.title = value.title;
    result.valid_until = value.valid_until;
    return result;
}

inline models::sales::ProposalRate Convert(const clients::sales::models::ProposalRate& value) {
    models::sales::ProposalRate result;
    result.currency = value.currency;
    result.rate_millionths = value.rate_millionths;
    return result;
}

inline models::sales::ProposalTotal Convert(const clients::sales::models::ProposalTotal& value) {
    models::sales::ProposalTotal result;
    result.currency = value.currency;
    result.purchase = Convert(value.purchase);
    result.sale = Convert(value.sale);
    return result;
}

inline models::sales::ProposalStatus Convert(const clients::sales::models::ProposalStatus& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::ProposalStatus{});
}

inline models::sales::Proposal Convert(const clients::sales::models::Proposal& value) {
    models::sales::Proposal result;
    result.customer_name = value.customer_name;
    result.input = Convert(value.input);
    result.inquiry_revision = value.inquiry_revision;
    result.lines.reserve(value.lines.size());
    for (const auto& item : value.lines)
        result.lines.push_back(Convert(item));
    result.rates.reserve(value.rates.size());
    for (const auto& item : value.rates)
        result.rates.push_back(Convert(item));
    result.reporting = Convert(value.reporting);
    result.reporting_currency = value.reporting_currency;
    result.revision = value.revision;
    result.status = Convert(value.status);
    result.totals.reserve(value.totals.size());
    for (const auto& item : value.totals)
        result.totals.push_back(Convert(item));
    return result;
}

inline models::sales::ProposalHistoryEntry Convert(
    const clients::sales::models::ProposalHistoryEntry& value) {
    models::sales::ProposalHistoryEntry result;
    result.actor = value.actor;
    result.created_at = value.created_at;
    result.snapshot = Convert(value.snapshot);
    return result;
}

inline models::sales::RegistryField_kind Convert(
    const clients::sales::models::RegistryField_kind& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::RegistryField_kind{});
}

inline models::sales::RegistryField Convert(const clients::sales::models::RegistryField& value) {
    models::sales::RegistryField result;
    result.case_insensitive = value.case_insensitive;
    result.id = value.id;
    result.kind = Convert(value.kind);
    result.lookup = value.lookup;
    result.sortable = value.sortable;
    return result;
}

inline models::sales::RegistryValue Convert(const clients::sales::models::RegistryValue& value) {
    models::sales::RegistryValue result;
    result.label = value.label;
    result.value = value.value;
    return result;
}

inline models::sales::RfqQuoteLine Convert(const clients::sales::models::RfqQuoteLine& value) {
    models::sales::RfqQuoteLine result;
    result.id = value.id;
    result.label_id = value.label_id;
    result.label_name = value.label_name;
    result.price = Convert(value.price);
    result.quantity_milli = value.quantity_milli;
    result.unit = Convert(value.unit);
    return result;
}

inline models::sales::RfqQuote Convert(const clients::sales::models::RfqQuote& value) {
    models::sales::RfqQuote result;
    result.delivery_days = value.delivery_days;
    result.id = value.id;
    result.lines.reserve(value.lines.size());
    for (const auto& item : value.lines)
        result.lines.push_back(Convert(item));
    result.note = value.note;
    result.supplier_id = value.supplier_id;
    result.supplier_name = value.supplier_name;
    result.title = value.title;
    result.valid_until = value.valid_until;
    return result;
}

inline models::sales::RfqStatus Convert(const clients::sales::models::RfqStatus& value) {
    return models::sales::ParseEnumScalar(
        clients::sales::models::EnumValue(value), models::sales::RfqStatus{});
}

inline models::sales::RfqInput Convert(const clients::sales::models::RfqInput& value) {
    models::sales::RfqInput result;
    result.answer_by = value.answer_by;
    result.id = value.id;
    result.inquiry_id = value.inquiry_id;
    result.note = value.note;
    result.quotes.reserve(value.quotes.size());
    for (const auto& item : value.quotes)
        result.quotes.push_back(Convert(item));
    result.responsible_subject = value.responsible_subject;
    result.route_segment_id = value.route_segment_id;
    result.status = Convert(value.status);
    result.title = value.title;
    return result;
}

inline models::sales::Rfq Convert(const clients::sales::models::Rfq& value) {
    models::sales::Rfq result;
    result.input = Convert(value.input);
    result.inquiry_number = value.inquiry_number;
    result.revision = value.revision;
    result.route_segment = Convert(value.route_segment);
    return result;
}

inline models::sales::RfqHistoryEntry Convert(
    const clients::sales::models::RfqHistoryEntry& value) {
    models::sales::RfqHistoryEntry result;
    result.actor = value.actor;
    result.created_at = value.created_at;
    result.snapshot = Convert(value.snapshot);
    return result;
}

inline models::sales::SalesCard Convert(const clients::sales::models::SalesCard& value) {
    models::sales::SalesCard result;
    result.customer_id = value.customer_id;
    result.id = value.id;
    result.manager_subject = value.manager_subject;
    result.next_action = value.next_action;
    result.next_action_date = value.next_action_date;
    result.note = value.note;
    result.organization_id = value.organization_id;
    result.revision = value.revision;
    result.source = value.source;
    result.stage_id = value.stage_id;
    return result;
}

inline models::sales::SalesContact Convert(const clients::sales::models::SalesContact& value) {
    models::sales::SalesContact result;
    result.author_issuer = value.author_issuer;
    result.author_subject = value.author_subject;
    result.card_id = value.card_id;
    result.contact_date = value.contact_date;
    result.created_at = value.created_at;
    result.id = value.id;
    result.kind = Convert(value.kind);
    result.next_action = value.next_action;
    result.next_action_date = value.next_action_date;
    result.result = value.result;
    result.revision = value.revision;
    return result;
}

inline models::sales::SalesContactHistoryEntry Convert(
    const clients::sales::models::SalesContactHistoryEntry& value) {
    models::sales::SalesContactHistoryEntry result;
    result.actor = value.actor;
    result.created_at = value.created_at;
    result.revision = value.revision;
    result.snapshot = Convert(value.snapshot);
    return result;
}

inline models::sales::SalesHistoryEntry Convert(
    const clients::sales::models::SalesHistoryEntry& value) {
    models::sales::SalesHistoryEntry result;
    result.actor = value.actor;
    result.created_at = value.created_at;
    result.revision = value.revision;
    result.snapshot = Convert(value.snapshot);
    return result;
}

inline models::sales::SalesStage Convert(const clients::sales::models::SalesStage& value) {
    models::sales::SalesStage result;
    result.enabled = value.enabled;
    result.id = value.id;
    result.name = value.name;
    return result;
}

template <typename T>
auto Convert(
    const std::vector<T>& values) -> std::vector<decltype(Convert(std::declval<const T&>()))> {
    std::vector<decltype(Convert(std::declval<const T&>()))> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert(value));
    return result;
}
}  // namespace web_bff::converters::sales
