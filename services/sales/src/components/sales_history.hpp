#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class SalesHistoryComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-history";

    SalesHistoryComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);

    std::vector<models::SalesHistoryEntry> GetCardHistory(
        const boost::uuids::uuid& organization, const models::HistoryRequest& request) const;
    std::vector<models::SalesContactHistoryEntry> GetContactHistory(
        const boost::uuids::uuid& organization, const models::HistoryRequest& request) const;
    std::vector<models::InquiryHistoryEntry> GetInquiryHistory(
        const boost::uuids::uuid& organization, const models::HistoryRequest& request) const;
    std::vector<models::ProposalHistoryEntry> GetProposalHistory(
        const boost::uuids::uuid& organization, const models::HistoryRequest& request) const;
    std::vector<models::RfqHistoryEntry> GetRfqHistory(
        const boost::uuids::uuid& organization, const models::HistoryRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::SalesHistoryComponent> = true;
