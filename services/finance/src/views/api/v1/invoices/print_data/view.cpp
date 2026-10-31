#include <helpers/converters/invoice_print.hpp>
#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/invoices/print_data/view.hpp>

namespace finance::views::api::v1::invoices::print_data {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
    const auto response = [](const models::InvoicePrintData& value) {
        return Response200{.charges = value.charges,
            .customer = value.customer,
            .invoice = value.invoice,
            .supplier = value.supplier,
            .supplier_bank = value.supplier_bank};
    };
    auto read = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto details = LoadInvoice(read, organization, body.id);
    if (details.invoice.revision != body.revision)
        throw ApiError<409>{
            "revision_conflict", "Счёт изменён. Обновите карточку перед скачиванием"};
    if (details.invoice.direction != models::InvoiceDirection::Outgoing)
        throw ApiError<400>{"not_outgoing_invoice", "Печатная форма доступна для исходящего счёта"};
    const auto cached =
        read.Execute(sql::kGetInvoicePrintSnapshot, organization, body.id, body.revision);
    read.Commit();
    if (!cached.IsEmpty())
        return response(broker_codegen::ParseJson<models::InvoicePrintData>(
            cached.AsSingleRow<userver::formats::json::Value>()));

    // Справочники нужны только для первого снимка редакции. Транзакция на время HTTP не открыта.
    models::InvoicePrintData data;
    data.invoice = details.invoice;
    data.charges = details.charges;
    const auto token = platform::auth::GetAccessToken(context);
    try {
        data.supplier = ConvertPrintParty(
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = data.invoice.own_company_id}}, token)
                .counterparty);
        data.customer = ConvertPrintParty(
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = data.invoice.counterparty_id}}, token)
                .counterparty);
        if (data.invoice.own_bank_account_id.has_value()) {
            const auto accounts = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
                {.body = {.counterparty_id = data.invoice.own_company_id}}, token);
            const auto found = std::find_if(
                accounts.items.begin(), accounts.items.end(), [&](const auto& account) {
                    return SameId(account.id, data.invoice.own_bank_account_id.value());
                });
            if (found == accounts.items.end() || found->currency != data.invoice.currency)
                throw ApiError<409>{"invalid_print_account",
                    "Банковские реквизиты счёта недоступны или имеют другую валюту"};
            data.supplier_bank = ConvertPrintBank(*found);
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к реквизитам для печатной формы"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Реквизиты для печатной формы не найдены"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{"reference_unavailable", "Не удалось получить реквизиты счёта"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис реквизитов недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса реквизитов"};
    }
    auto write = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto current = write.Execute(sql::kLockInvoicePrintRevision, organization, body.id);
    if (current.IsEmpty() || current.AsSingleRow<std::int64_t>() != body.revision)
        throw ApiError<409>{
            "revision_conflict", "Счёт изменён. Обновите карточку перед скачиванием"};
    auto saved = write.Execute(sql::kSaveInvoicePrintSnapshot, organization, body.id, body.revision,
        userver::formats::json::ValueBuilder(data).ExtractValue());
    if (saved.IsEmpty())
        saved = write.Execute(sql::kGetInvoicePrintSnapshot, organization, body.id, body.revision);
    const auto result = broker_codegen::ParseJson<models::InvoicePrintData>(
        saved.AsSingleRow<userver::formats::json::Value>());
    write.Commit();
    return response(result);
}
}  // namespace finance::views::api::v1::invoices::print_data
