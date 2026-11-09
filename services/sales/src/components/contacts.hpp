#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class ContactsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-contacts";
    ContactsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct AddResult {
        models::SalesCard card;
        models::SalesContact item;
    };

    struct ListResult {
        std::vector<models::SalesContact> items;
        std::optional<std::string> next_before;
    };

    AddResult Add(
        const models::AddSalesContactRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListSalesContactsRequest&, userver::server::request::RequestContext&) const;
    models::SalesContact Update(
        const models::UpdateSalesContactRequest&, userver::server::request::RequestContext&) const;
    std::vector<models::SalesContactHistoryEntry> History(
        const models::HistoryRequest&, userver::server::request::RequestContext&) const;

  private:
    struct Dependencies {
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ContactsComponent> = true;
