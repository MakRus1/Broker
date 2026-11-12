#pragma once
#include <models/contact_results.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class ContactsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-contacts";
    ContactsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::ContactAddition AddContact(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::AddSalesContactRequest& request) const;
    models::ContactsPage ListContacts(
        const std::string& organization, const models::ListSalesContactsRequest& request) const;
    models::SalesContact UpdateContact(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::UpdateSalesContactRequest& request) const;
    std::vector<models::SalesContactHistoryEntry> GetContactHistory(
        const std::string& organization, const models::HistoryRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ContactsComponent> = true;
