#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparty_reconciliation/get/view.hpp>

namespace web_bff::views::api::v1::counterparty_reconciliation::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result = deps.finance.ApiV1CounterpartyReconciliationGet(
        {.body = {.contract_id = body.contract_id,
             .counterparty_id = body.counterparty_id,
             .currency = body.currency,
             .date_from = body.date_from,
             .date_to = body.date_to,
             .own_company_id = body.own_company_id}},
        session.Access);
    return Response200{.operations = Convert(result.operations), .totals = Convert(result.totals)};
}
}  // namespace web_bff::views::api::v1::counterparty_reconciliation::get
