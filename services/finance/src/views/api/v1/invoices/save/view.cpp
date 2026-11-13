#include <components/invoices.hpp>
#include <converters/invoice_input.hpp>
#include <helpers/document_parties.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <utility>
#include <views/api/v1/invoices/save/view.hpp>

namespace finance::views::api::v1::invoices::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);

    const auto& body = request.body;
    auto value = converters::ToInvoice(body);
    const auto& token = platform::auth::GetAccessToken(context);
    try {
        for (const auto& id : {body.own_company_id, body.counterparty_id}) {
            const bool own_company = id == body.own_company_id;
            const auto party =
                deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = id}}, token)
                    .counterparty;
            helpers::ValidateDocumentParty(body, party, own_company);
            (own_company ? value.OwnCompanyName : value.CounterpartyName) = party.name;

            const auto& account =
                own_company ? body.own_bank_account_id : body.counterparty_bank_account_id;
            if (account) {
                const auto accounts = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
                    {.body = {.counterparty_id = id}}, token);
                helpers::ValidateDocumentAccount(body, accounts.items, *account);
            }
        }
        if (body.contract_id) {
            const auto contract =
                deps.contracts.ApiV1ContractsGet({.body = {.id = *body.contract_id}}, token)
                    .contract;
            helpers::ValidateDocumentContract(body, contract);
            value.ContractNumber = contract.number;
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к сторонам или договору документа"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Сторона или договор не найдены в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{"reference_unavailable", "Не удалось проверить стороны документа"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }

    auto result =
        deps.invoices->SaveInvoice(organization, identity, request.body, std::move(value));
    return Response200{.charges = std::move(result.charges), .invoice = std::move(result.invoice)};
}
}  // namespace finance::views::api::v1::invoices::save
