#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace counterparties {
class CounterpartiesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "counterparties-cards";
    CounterpartiesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

    void ValidateLogist(const boost::uuids::uuid& organization, std::string_view access_token,
        const std::optional<std::string>& subject) const;
    models::Counterparty CreateCounterparty(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, const models::CreateCounterpartyRequest&,
        const models::Counterparty& value) const;
    models::Counterparty UpdateCounterparty(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, const models::UpdateCounterpartyRequest&) const;
    models::Counterparty FindCounterparty(
        const boost::uuids::uuid& organization, const models::GetCounterpartyRequest&) const;
    models::Counterparty SetCounterpartyArchived(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, const models::SetArchivedCounterpartyRequest&) const;
    std::vector<models::CounterpartyEvent> GetCounterpartyHistory(
        const boost::uuids::uuid& organization,
        const models::ListCounterpartyHistoryRequest&) const;

  private:
    clients::organizations::Client organizations_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace counterparties

template <>
inline constexpr bool userver::components::kHasValidate<counterparties::CounterpartiesComponent> =
    true;
