#pragma once
#include <clients/contracts/client.hpp>
#include <clients/counterparties/client.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class InvoicesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-invoices";
    InvoicesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct ListResult {
        std::vector<models::InvoiceRegistryItem> items;
        std::optional<models::InvoiceRegistryCursor> next_cursor;
    };

    struct CandidatesResult {
        std::vector<models::ServiceCharge> items;
        std::optional<std::string> next_after;
    };

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::optional<std::string> next_after;
        std::vector<models::RegistryValue> values;
    };

    models::InvoicePrintData PrintData(
        const models::InvoicePrintRequest&, userver::server::request::RequestContext&) const;
    models::InvoiceDetails Save(
        const models::SaveInvoiceRequest&, userver::server::request::RequestContext&) const;
    models::InvoiceDetails Get(
        const models::GetInvoiceRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListInvoicesRequest&, userver::server::request::RequestContext&) const;
    CandidatesResult Candidates(const models::ListInvoiceCandidatesRequest&,
        userver::server::request::RequestContext&) const;
    models::InvoiceDetails AddServiceCharge(const models::AddInvoiceServiceChargeRequest&,
        userver::server::request::RequestContext&) const;
    models::InvoiceDetails DetachServiceCharge(const models::DetachInvoiceServiceChargeRequest&,
        userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(
        const models::InvoiceFilterValuesRequest&, userver::server::request::RequestContext&) const;

  private:
    pgtypes::InvoiceV1 Prepare(
        const userver::server::request::RequestContext&, const models::SaveInvoiceRequest&) const;

    struct Dependencies {
        clients::contracts::Client contracts;
        clients::counterparties::Client counterparties;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::InvoicesComponent> = true;
