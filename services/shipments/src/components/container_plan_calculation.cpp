#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <components/container_plan_calculation.hpp>
#include <converters/shipments.hpp>
#include <map>
#include <set>

namespace {
using shipments::ContainerKey;
using shipments::ContainerPlan;
using shipments::ContainerPlanSnapshot;
namespace models = shipments::models;
namespace clients = shipments::clients;

ContainerPlan ReconcileContainerPlan(const ContainerPlanSnapshot& snapshot,
    const clients::transportations::ApiV1TransportationsContainerFact::Response& facts) {
    const auto& items = snapshot.Items;
    const auto& pending = snapshot.Pending;
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

}  // namespace

namespace shipments::container_plan {
models::ContainerPlanListResult BuildPlanPage(const ContainerPlanSnapshot& snapshot,
    const clients::transportations::ApiV1TransportationsContainerFact::Response& facts,
    const std::optional<std::string>& after_id, std::int64_t limit) {
    auto plan = ReconcileContainerPlan(snapshot, facts);
    models::ContainerPlanListResult result;
    result.unplanned_items = std::move(plan.Unplanned);
    const auto after = after_id.has_value()
        ? std::optional{boost::uuids::string_generator{}(after_id.value())}
        : std::nullopt;
    for (const auto& row : snapshot.Items) {
        if (after.has_value() && row.Id <= after.value())
            continue;
        if (result.items.size() >= static_cast<std::size_t>(limit))
            break;
        result.items.push_back(
            converters::Convert(row, plan.Totals[{row.ContainerType, row.Ownership}]));
    }
    return result;
}

}  // namespace shipments::container_plan
