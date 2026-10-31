#include <platform/browser/session.hpp>
#include <views/api/v1/counterparty_settlements/get/view.hpp>

namespace web_bff::views::api::v1::counterparty_settlements::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result =
        deps.finance.ApiV1CounterpartySettlementsGet({.body = {.contract_id = body.contract_id,
                                                          .counterparty_id = body.counterparty_id,
                                                          .currency = body.currency,
                                                          .own_company_id = body.own_company_id}},
            session.Access);
    Response200 response;
    for (const auto& total : result.totals)
        response.totals.push_back({.currency = total.currency,
            .overdue_payable_minor = total.overdue_payable_minor,
            .overdue_receivable_minor = total.overdue_receivable_minor,
            .payable_minor = total.payable_minor,
            .receivable_minor = total.receivable_minor,
            .unallocated_disbursements_minor = total.unallocated_disbursements_minor,
            .unallocated_receipts_minor = total.unallocated_receipts_minor});
    return response;
}
}  // namespace web_bff::views::api::v1::counterparty_settlements::get
