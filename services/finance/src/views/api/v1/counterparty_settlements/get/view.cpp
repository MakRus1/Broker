#include <finance/sql_queries.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <views/api/v1/counterparty_settlements/get/view.hpp>

namespace finance::views::api::v1::counterparty_settlements::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
    // Один SQL-запрос даёт согласованный снимок счетов и распределений платежей.
    const auto rows = deps.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kSlave,
        sql::kCounterpartySettlements, organization, body.counterparty_id, body.own_company_id,
        body.contract_id, body.currency);
    Response200 response;
    for (const auto& row : rows) {
        const auto [currency, receivable, payable, overdue_receivable, overdue_payable,
            unallocated_receipts, unallocated_disbursements] = row.As<std::string, std::string,
            std::string, std::string, std::string, std::string, std::string>();
        response.totals.push_back({.currency = currency,
            .overdue_payable_minor = overdue_payable,
            .overdue_receivable_minor = overdue_receivable,
            .payable_minor = payable,
            .receivable_minor = receivable,
            .unallocated_disbursements_minor = unallocated_disbursements,
            .unallocated_receipts_minor = unallocated_receipts});
    }
    return response;
}
}  // namespace finance::views::api::v1::counterparty_settlements::get
