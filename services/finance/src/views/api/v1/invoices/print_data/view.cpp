#include <components/invoices.hpp>
#include <converters/invoice_print.hpp>
#include <helpers/document_parties.hpp>
#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <utility>
#include <views/api/v1/invoices/print_data/view.hpp>

namespace finance::views::api::v1::invoices::print_data {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto snapshot = deps.invoices->ReadInvoicePrintSnapshot(organization, request.body);
    auto result = snapshot.Cached;
    if (!result) {
        models::InvoicePrintData data;
        data.invoice = snapshot.Details.invoice;
        data.charges = snapshot.Details.charges;
        const auto token = platform::auth::GetAccessToken(context);
        try {
            data.supplier = converters::ConvertPrintParty(
                deps.counterparties
                    .ApiV1CounterpartiesGet({.body = {.id = data.invoice.own_company_id}}, token)
                    .counterparty);
            data.customer = converters::ConvertPrintParty(
                deps.counterparties
                    .ApiV1CounterpartiesGet({.body = {.id = data.invoice.counterparty_id}}, token)
                    .counterparty);
            if (data.invoice.own_bank_account_id.has_value()) {
                const auto accounts = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
                    {.body = {.counterparty_id = data.invoice.own_company_id}}, token);
                const auto found = std::find_if(
                    accounts.items.begin(), accounts.items.end(), [&](const auto& account) {
                        return helpers::SameId(
                            account.id, data.invoice.own_bank_account_id.value());
                    });
                if (found == accounts.items.end() || found->currency != data.invoice.currency)
                    throw ApiError<409>{"invalid_print_account",
                        "Банковские реквизиты счёта недоступны или имеют другую валюту"};
                data.supplier_bank = converters::ConvertPrintBank(*found);
            }
        } catch (const platform::http::ErrorByStatus<403>&) {
            throw ApiError<403>{
                "forbidden_reference", "Нет доступа к реквизитам для печатной формы"};
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
        result = deps.invoices->SaveInvoicePrintSnapshot(organization, data);
    }
    return Response200{.charges = std::move(result->charges),
        .customer = std::move(result->customer),
        .invoice = std::move(result->invoice),
        .supplier = std::move(result->supplier),
        .supplier_bank = std::move(result->supplier_bank)};
}
}  // namespace finance::views::api::v1::invoices::print_data
