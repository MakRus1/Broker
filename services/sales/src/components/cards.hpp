#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class CardsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-cards";
    CardsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct ListResult {
        std::vector<models::SalesCard> items;
        std::optional<models::SalesRegistryCursor> next_cursor;
    };

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::vector<models::RegistryValue> values;
        std::optional<std::string> next_after;
    };

    models::SalesCard Save(
        const models::SaveSalesCardRequest&, userver::server::request::RequestContext&) const;
    models::SalesCard Get(
        const models::IdRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListSalesRequest&, userver::server::request::RequestContext&) const;
    std::vector<models::SalesHistoryEntry> History(
        const models::HistoryRequest&, userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(
        const models::SalesFilterValuesRequest&, userver::server::request::RequestContext&) const;

  private:
    struct Dependencies {
        clients::counterparties::Client counterparties;
        clients::organizations::Client organizations;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::CardsComponent> = true;
