#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class InquiriesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-inquiries";
    InquiriesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct ListResult {
        std::vector<models::Inquiry> items;
        std::optional<models::SalesRegistryCursor> next_cursor;
    };

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::vector<models::RegistryValue> values;
        std::optional<std::string> next_after;
    };

    models::Inquiry Save(
        const models::SaveInquiryRequest&, userver::server::request::RequestContext&) const;
    models::Inquiry Get(const models::IdRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListSalesRequest&, userver::server::request::RequestContext&) const;
    std::vector<models::InquiryHistoryEntry> History(
        const models::HistoryRequest&, userver::server::request::RequestContext&) const;
    models::Inquiry CreateShipment(
        const models::CreateShipmentRequest&, userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(
        const models::SalesFilterValuesRequest&, userver::server::request::RequestContext&) const;

  private:
    struct Dependencies {
        clients::counterparties::Client counterparties;
        clients::organizations::Client organizations;
        clients::transport_locations::Client transport_locations;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::InquiriesComponent> = true;
