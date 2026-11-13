#include <components/proposal_calculation.hpp>
#include <components/rfq_rules.hpp>
#include <models/errors.hpp>
#include <userver/utest/utest.hpp>

UTEST(ProposalCalculation, UsesProvidedRatesAndPreservesApprovedAmounts) {
    sales::models::ProposalInput input;
    input.calculation_date = {2026, 11, 8};
    auto& line = input.lines.emplace_back();
    line.id = "sale";
    line.label_id = "freight";
    line.quantity_milli = 1000;
    line.unit = sales::models::FinancialLabelUnit::Transportation;
    line.sale.currency = "USD";
    line.sale.price_minor = 230000;
    line.sale.price_mode = sales::models::PriceMode::Gross;
    line.sale.vat_mode = sales::models::VatMode::Rate;
    line.sale.vat_rate_basis_points = 2000;
    line.purchase = line.sale;
    line.purchase->price_minor = 200000;
    line.supplier_id = "supplier";
    sales::models::ProposalCalculation data{.InquiryRevision = 7,
        .CustomerName = "Клиент",
        .ReportingCurrency = "RUB",
        .LabelNames = {{"freight", "Фрахт"}},
        .SupplierNames = {{"supplier", "Линия"}},
        .Rates = {{"USD", 90000000}}};

    const auto proposal = sales::proposal_rules::CalculateProposal(input, data);

    ASSERT_EQ(proposal.lines.size(), 1);
    EXPECT_EQ(proposal.inquiry_revision, 7);
    EXPECT_EQ(proposal.lines[0].sale.net_minor, 191667);
    EXPECT_EQ(proposal.lines[0].sale.vat_minor, 38333);
    EXPECT_EQ(proposal.lines[0].sale.total_minor, 230000);
    EXPECT_EQ(proposal.reporting.sale.net_minor, 17250030);
    EXPECT_EQ(proposal.lines[0].supplier_name, "Линия");
    EXPECT_EQ(input.lines[0].sale.price_minor, 230000);
}

UTEST(RfqQuote, AppliesStoredVersionAndRejectsChangedRoute) {
    sales::models::Inquiry inquiry;
    auto& segment = inquiry.route_segments.emplace_back();
    segment.id = "11111111-1111-4111-8111-111111111111";
    segment.type = "sea";
    sales::models::Rfq rfq;
    rfq.input.inquiry_id = "inquiry";
    rfq.input.status = sales::models::RfqStatus::Ready;
    rfq.input.route_segment_id = segment.id;
    rfq.route_segment = segment;
    auto& quote = rfq.input.quotes.emplace_back();
    quote.id = "quote";
    quote.supplier_id = "supplier";
    auto& quoted = quote.lines.emplace_back();
    quoted.id = "line";
    quoted.label_id = "freight";
    quoted.quantity_milli = 2000;
    quoted.price.price_minor = 80000;
    sales::models::ProposalInput input;
    input.inquiry_id = "inquiry";
    sales::models::ProposalLine line;
    line.quote_source.emplace();
    line.quote_source->quote_id = quote.id;
    line.quote_source->line_id = quoted.id;

    sales::rfq_rules::ApplyQuote(input, inquiry, rfq, line);

    ASSERT_TRUE(line.purchase.has_value());
    EXPECT_EQ(line.purchase->price_minor, 80000);
    EXPECT_EQ(line.quantity_milli, 2000);
    EXPECT_EQ(line.supplier_id, "supplier");
    inquiry.route_segments[0].type = "air";
    EXPECT_THROW(sales::rfq_rules::ApplyQuote(input, inquiry, rfq, line), sales::ApiError<400>);
}
