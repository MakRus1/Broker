#pragma once
#include <clients/counterparties/client.hpp>
#include <models/pg/payment_raw.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class PaymentsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-payments";
    PaymentsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct ListResult {
        std::vector<models::Payment> items;
        std::optional<models::InvoiceRegistryCursor> next_cursor;
    };

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::optional<std::string> next_after;
        std::vector<models::RegistryValue> values;
    };

    struct InvoiceCandidatesResult {
        std::vector<models::PaymentInvoice> items;
        std::optional<std::string> next_after;
    };

    struct PaymentCandidatesResult {
        std::vector<models::PaymentAvailable> items;
        std::optional<std::string> next_after;
    };

    models::PaymentDetails Save(
        const models::SavePaymentRequest&, userver::server::request::RequestContext&) const;
    models::PaymentDetails Get(
        const models::InvoiceBalanceRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListPaymentsRequest&, userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(
        const models::PaymentFilterValuesRequest&, userver::server::request::RequestContext&) const;
    InvoiceCandidatesResult InvoiceCandidates(const models::PaymentInvoiceCandidatesRequest&,
        userver::server::request::RequestContext&) const;
    models::PaymentInvoice InvoiceBalance(
        const models::InvoiceBalanceRequest&, userver::server::request::RequestContext&) const;
    PaymentCandidatesResult PaymentCandidates(
        const models::PaymentCandidatesRequest&, userver::server::request::RequestContext&) const;

  private:
    models::pg::PaymentRaw Prepare(
        const userver::server::request::RequestContext&, const models::SavePaymentRequest&) const;

    struct Dependencies {
        clients::counterparties::Client counterparties;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::PaymentsComponent> = true;
