#pragma once
#include <algorithm>
#include <clients/transportations/client.hpp>
#include <deps.hpp>
#include <helpers/converters.hpp>
#include <map>
#include <platform/auth/identity.hpp>
#include <set>

namespace shipments::helpers {
using ContainerKey = std::pair<std::string, std::string>;

struct ContainerPlan {
    std::map<ContainerKey, ContainerTotals> Totals;
    std::vector<models::ContainerFact> Unplanned;
};

inline ContainerPlan ReadContainerPlan(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& shipment,
    const std::vector<pgtypes::ShipmentItemV1>& items,
    const std::vector<pgtypes::PendingContainerCreationV1>& pending) {
    namespace api = clients::transportations::ApiV1TransportationsContainerFact;
    api::RequestBody request;
    request.shipment_id = shipment;
    for (const auto& operation : pending)
        request.pending_operation_ids.push_back(boost::uuids::to_string(operation.Id));
    const auto facts = deps.transportations.ApiV1TransportationsContainerFact(
        api::Request{.body = request}, platform::auth::GetAccessToken(context));
    ContainerPlan result;
    std::set<ContainerKey> planned;
    for (const auto& item : items)
        planned.emplace(item.ContainerType, item.Ownership);
    for (const auto& group : facts.groups) {
        std::optional<std::string> ownership;
        if (group.container_ownership.has_value())
            ownership =
                clients::transportations::models::EnumValue(group.container_ownership.value());
        if (group.container_type_code.has_value() && ownership.has_value()) {
            const ContainerKey key{group.container_type_code.value(), ownership.value()};
            result.Totals[key].Created += group.quantity;
            if (planned.contains(key))
                continue;
        }
        models::ContainerFact unplanned;
        unplanned.container_type_code = group.container_type_code;
        if (ownership.has_value())
            unplanned.container_ownership =
                models::ParseEnumScalar(ownership.value(), models::ContainerOwnership{});
        unplanned.quantity = group.quantity;
        result.Unplanned.push_back(std::move(unplanned));
    }
    std::map<std::string, std::int64_t> materialized;
    for (const auto& operation : facts.operations)
        materialized.emplace(operation.operation_id, operation.created_quantity);
    for (const auto& operation : pending)
        result.Totals[{operation.ContainerType, operation.Ownership}].Pending +=
            std::max<std::int64_t>(
                0, operation.Quantity - materialized[boost::uuids::to_string(operation.Id)]);
    return result;
}
}  // namespace shipments::helpers
