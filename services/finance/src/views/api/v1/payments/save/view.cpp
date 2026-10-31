#include <helpers/payments.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/payments/save/view.hpp>

namespace finance::views::api::v1::payments::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
    auto value = PreparePayment(deps, context, body);
    std::map<boost::uuids::uuid, std::int64_t> allocations;
    std::int64_t allocated = 0;
    for (const auto& item : body.allocations) {
        if (item.amount_minor > value.AmountMinor - allocated)
            throw ApiError<400>{"payment_overallocated", "Распределение превышает сумму платежа"};
        allocated += item.amount_minor;
        if (!allocations
                 .emplace(boost::uuids::string_generator{}(item.invoice_id), item.amount_minor)
                 .second)
            throw ApiError<400>{"duplicate_invoice", "Счёт указан несколько раз"};
    }
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (body.expected_revision == 0) {
        if (transaction.Execute(sql::kCreatePayment, organization, value).IsEmpty())
            throw ApiError<409>{"revision_conflict", "Платёж уже существует. Обновите карточку"};
    } else {
        if (transaction.Execute(sql::kLockPayments, organization, value.Id).IsEmpty())
            throw ApiError<404>{"payment_not_found", "Платёж не найден"};
        if (transaction.Execute(sql::kUpdatePayment, organization, value, body.expected_revision)
                .IsEmpty())
            throw ApiError<409>{"revision_conflict", "Платёж изменён. Обновите карточку"};
    }
    std::set<boost::uuids::uuid> ids;
    for (const auto& [id, amount] : allocations)
        ids.insert(id);
    for (const auto& row : transaction.Execute(sql::kGetPaymentAllocations, organization, value.Id))
        ids.insert(row[0].As<boost::uuids::uuid>());
    // Платёж блокируется первым; все счета — в едином порядке. HTTP уже завершён.
    const auto invoices = transaction.Execute(
        sql::kLockInvoices, organization, std::vector<boost::uuids::uuid>{ids.begin(), ids.end()});
    if (invoices.Size() != ids.size())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден в организации"};
    for (const auto& invoice : invoices.AsSetOf<pgtypes::InvoiceV1>()) {
        const auto found = allocations.find(invoice.Id);
        if (value.Archived || found == allocations.end())
            continue;
        const auto direction = models::EnumValue(PaymentInvoiceDirection(body.kind));
        if (invoice.Archived || invoice.Direction != direction ||
            invoice.Currency != value.Currency || invoice.OwnCompanyId != value.OwnCompanyId ||
            invoice.CounterpartyId != value.CounterpartyId)
            throw ApiError<400>{"incompatible_invoice",
                "Выберите действующий счёт с теми же сторонами, валютой и направлением"};
        const auto paid =
            transaction.Execute(sql::kInvoiceOtherPayments, organization, invoice.Id, value.Id)
                .AsSingleRow<std::int64_t>();
        if (found->second > invoice.TotalMinor - paid)
            throw ApiError<409>{
                "invoice_overpaid", "Распределение превышает остаток счёта. Обновите данные"};
    }
    transaction.Execute(sql::kDeletePaymentAllocations, organization, value.Id);
    for (const auto& [id, amount] : allocations)
        transaction.Execute(sql::kCreatePaymentAllocation, organization, value.Id, id, amount);
    const auto result = LoadPayment(transaction, organization, body.id);
    transaction.Execute(sql::kWritePaymentHistory, organization, value.Id, value.Revision,
        identity.ActorId(), HistorySnapshot(result, identity));
    transaction.Commit();
    return Response200{.allocated_minor = result.allocated_minor,
        .allocations = result.allocations,
        .payment = result.payment,
        .unallocated_minor = result.unallocated_minor};
}
}  // namespace finance::views::api::v1::payments::save
