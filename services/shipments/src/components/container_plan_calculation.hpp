#pragma once
#include <clients/transportations/client.hpp>
#include <models/container_plan.hpp>

namespace shipments::container_plan {
models::ContainerPlanListResult BuildPlanPage(const ContainerPlanSnapshot& snapshot,
    const clients::transportations::ApiV1TransportationsContainerFact::Response& facts,
    const std::optional<std::string>& after_id, std::int64_t limit);
}  // namespace shipments::container_plan
