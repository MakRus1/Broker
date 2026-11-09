#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace counterparties {
class CounterpartiesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "counterparties-cards";
    CounterpartiesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct ListResult {
        std::vector<models::Counterparty> items;
        std::optional<models::CounterpartyRegistryCursor> next_cursor;
    };

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::optional<std::string> next_after;
        std::vector<models::RegistryValue> values;
    };

    models::Counterparty Create(
        const models::CreateCounterpartyRequest&, userver::server::request::RequestContext&) const;
    models::Counterparty Update(
        const models::UpdateCounterpartyRequest&, userver::server::request::RequestContext&) const;
    models::Counterparty Get(
        const models::GetCounterpartyRequest&, userver::server::request::RequestContext&) const;
    models::Counterparty SetArchived(const models::SetArchivedCounterpartyRequest&,
        userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListCounterpartiesRequest&, userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(const models::CounterpartyFilterValuesRequest&,
        userver::server::request::RequestContext&) const;
    std::vector<models::CounterpartyEvent> History(const models::ListCounterpartyHistoryRequest&,
        userver::server::request::RequestContext&) const;

  private:
    void ValidateLogist(
        const userver::server::request::RequestContext&, const std::optional<std::string>&) const;
    clients::organizations::Client organizations_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace counterparties

template <>
inline constexpr bool userver::components::kHasValidate<counterparties::CounterpartiesComponent> =
    true;
