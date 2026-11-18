#include <components/sales_history.hpp>
#include <sales/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace {
namespace models = sales::models;
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

template <typename Entry>
std::vector<Entry> ReadHistory(pg::Cluster& cluster, const pg::Query& query,
    const boost::uuids::uuid& organization, const models::HistoryRequest& request) {
    const auto rows = cluster.Execute(pg::ClusterHostType::kMaster, query, organization, request.id,
        request.after_revision.value_or(0));

    std::vector<Entry> history;
    history.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        history.push_back(models::ParseModelJson(row, std::type_identity<Entry>{}));

    return history;
}
}  // namespace

namespace sales {
SalesHistoryComponent::SalesHistoryComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

std::vector<models::SalesHistoryEntry> SalesHistoryComponent::GetCardHistory(
    const boost::uuids::uuid& organization, const models::HistoryRequest& request) const {
    return ReadHistory<models::SalesHistoryEntry>(
        *cluster_, sql::kHistoryCards, organization, request);
}

std::vector<models::SalesContactHistoryEntry> SalesHistoryComponent::GetContactHistory(
    const boost::uuids::uuid& organization, const models::HistoryRequest& request) const {
    return ReadHistory<models::SalesContactHistoryEntry>(
        *cluster_, sql::kHistoryContacts, organization, request);
}

std::vector<models::InquiryHistoryEntry> SalesHistoryComponent::GetInquiryHistory(
    const boost::uuids::uuid& organization, const models::HistoryRequest& request) const {
    return ReadHistory<models::InquiryHistoryEntry>(
        *cluster_, sql::kHistoryInquiries, organization, request);
}

std::vector<models::ProposalHistoryEntry> SalesHistoryComponent::GetProposalHistory(
    const boost::uuids::uuid& organization, const models::HistoryRequest& request) const {
    return ReadHistory<models::ProposalHistoryEntry>(
        *cluster_, sql::kHistoryProposals, organization, request);
}

std::vector<models::RfqHistoryEntry> SalesHistoryComponent::GetRfqHistory(
    const boost::uuids::uuid& organization, const models::HistoryRequest& request) const {
    return ReadHistory<models::RfqHistoryEntry>(
        *cluster_, sql::kHistoryRfqs, organization, request);
}

}  // namespace sales
