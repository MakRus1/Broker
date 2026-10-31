#pragma once
#include <deps.hpp>
#include <helpers/financial_labels.hpp>
#include <helpers/history/events.hpp>
#include <permissions/permissions.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace finance::helpers::history {
inline models::FinancialHistoryResponse Load(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const models::FinancialHistoryRequest& body, Source source) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto permission = source == Source::ServiceCharge
        ? permissions::FINANCE_READ_SERVICE_CHARGES
        : source == Source::Invoice ? permissions::FINANCE_READ_INVOICES
                                    : permissions::FINANCE_READ_PAYMENTS;
    platform::auth::CheckPermission(context, permission);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto exists = source == Source::ServiceCharge ? sql::kGetServiceCharge
        : source == Source::Invoice                     ? sql::kGetInvoice
                                                        : sql::kGetPayment;
    if (transaction.Execute(exists, organization, body.id).IsEmpty())
        throw ApiError<404>{"not_found", "Документ не найден в организации"};
    std::optional<pg::TimePointTz> before_time;
    std::optional<std::string> before_source, before_id;
    std::optional<std::int64_t> before_revision;
    if (body.before.has_value()) {
        const auto& before = body.before.value();
        before_time = pg::TimePointTz{before.occurred_at};
        before_source = models::EnumValue(before.source);
        before_id = before.entity_id;
        before_revision = before.revision;
    }
    const bool related = source == Source::ServiceCharge
        ? platform::auth::HasPermission(context, permissions::FINANCE_READ_INVOICES)
        : source == Source::Invoice
        ? platform::auth::HasPermission(context, permissions::FINANCE_READ_PAYMENTS)
        : true;
    const auto query = source == Source::ServiceCharge ? sql::kListServiceChargeHistory
        : source == Source::Invoice                    ? sql::kListInvoiceHistory
                                                       : sql::kListPaymentHistory;
    const auto rows = transaction.Execute(query, organization, body.id, before_time, before_source,
        before_id, before_revision, body.limit + 1, related);
    using Row = std::tuple<std::string, boost::uuids::uuid, std::int64_t, std::string,
        pg::TimePointTz, Json, std::optional<Json>>;
    const auto history = rows.AsContainer<std::vector<Row>>(pg::kRowTag);
    std::set<boost::uuids::uuid> invoice_ids;
    if (source == Source::Payment &&
        platform::auth::HasPermission(context, permissions::FINANCE_READ_INVOICES)) {
        for (const auto& row : history) {
            for (const auto& doc : {std::get<5>(row), std::get<6>(row).value_or(Json{})}) {
                for (const auto& [id, allocation] : Indexed(At(doc, "allocations"), "invoice_id"))
                    invoice_ids.insert(boost::uuids::string_generator{}(id));
            }
        }
    }
    std::map<std::string, std::string> invoice_names;
    if (!invoice_ids.empty()) {
        const auto names = transaction.Execute(sql::kHistoryInvoiceNames, organization,
            std::vector<boost::uuids::uuid>{invoice_ids.begin(), invoice_ids.end()});
        for (const auto& row : names) {
            const auto [id, name] = row.As<boost::uuids::uuid, std::string>();
            invoice_names.emplace(boost::uuids::to_string(id), name);
        }
    }
    transaction.Commit();
    models::FinancialHistoryResponse result;
    for (const auto& [type, id, revision, actor, time, snapshot, previous] : history) {
        if (result.items.size() == static_cast<std::size_t>(body.limit))
            break;
        result.items.push_back(Event({.entity_id = boost::uuids::to_string(id),
                                         .occurred_at = time.GetUnderlying(),
                                         .revision = revision,
                                         .source = models::ParseEnumScalar(type, Source{})},
            actor, previous.value_or(Json{}), snapshot, source, body.id, invoice_names));
    }
    if (history.size() > static_cast<std::size_t>(body.limit))
        result.next_before = result.items.back().cursor;
    return result;
}
}  // namespace finance::helpers::history
