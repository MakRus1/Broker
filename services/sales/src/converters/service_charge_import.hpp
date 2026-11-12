#pragma once
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <clients/finance/client.hpp>
#include <events/shipment_command.hpp>
#include <openapi/models.hpp>
#include <userver/utils/boost_uuid5.hpp>

namespace sales::converters {
inline clients::finance::ApiInternalV1ServiceChargesApplyImport::RequestBody ToServiceChargeImport(
    const ShipmentCommand& command, const std::string& shipment_id, const models::Inquiry& inquiry,
    const models::Proposal& proposal) {
    namespace fm = clients::finance::models;
    const auto namespace_id = boost::uuids::string_generator{}(proposal.input.id);
    clients::finance::ApiInternalV1ServiceChargesApplyImport::RequestBody batch;
    batch.organization_id = command.organization_id;
    batch.source = {
        .id = proposal.input.id, .revision = proposal.revision, .type = "sales.proposal"};
    batch.id = boost::uuids::to_string(userver::utils::generators::GenerateBoostUuidV5(
        namespace_id, std::to_string(proposal.revision)));
    batch.actor_subject = command.actor_subject;
    batch.actor_issuer = command.actor_issuer;
    for (const auto& line : proposal.lines) {
        const auto add = [&](const models::ProposalPrice& price, bool income) {
            fm::SaveServiceChargeRequest charge;
            charge.id = boost::uuids::to_string(
                userver::utils::generators::GenerateBoostUuidV5(namespace_id,
                    line.input.id + (income ? ":sale:" : ":purchase:") +
                        std::to_string(proposal.revision)));
            charge.shipment_id = shipment_id;
            charge.label_id = line.input.label_id;
            charge.unit =
                fm::ParseEnumScalar(models::EnumValue(line.input.unit), fm::FinancialLabelUnit{});
            charge.direction =
                income ? fm::ServiceChargeDirection::Income : fm::ServiceChargeDirection::Expense;
            charge.counterparty_id = income ? inquiry.customer_id : *line.input.supplier_id;
            charge.quantity_milli = line.input.quantity_milli;
            charge.price_minor = price.price_minor;
            charge.currency = price.currency;
            charge.invoice_currency = price.currency;
            charge.vat_mode = fm::ParseEnumScalar(models::EnumValue(price.vat_mode), fm::VatMode{});
            charge.price_mode =
                fm::ParseEnumScalar(models::EnumValue(price.price_mode), fm::PriceMode{});
            charge.vat_rate_basis_points = price.vat_rate_basis_points;
            charge.note = line.input.note;
            charge.archived = false;
            charge.expected_revision = 0;
            charge.service_date = proposal.input.calculation_date;
            batch.items.push_back({.charge = charge,
                .counterparty_name =
                    income ? proposal.customer_name : line.supplier_name.value_or(""),
                .label_name = line.label_name});
        };
        add(line.input.sale, true);
        if (line.input.purchase)
            add(*line.input.purchase, false);
    }
    return batch;
}
}  // namespace sales::converters
