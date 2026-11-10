#pragma once
#include <clients/sales/client.hpp>
#include <openapi/models.hpp>
#include <vector>

namespace admin_bff::converters {
inline std::vector<models::sales::SalesStage> ToAdminSalesStages(
    const std::vector<clients::sales::models::SalesStage>& stages) {
    std::vector<models::sales::SalesStage> result;
    result.reserve(stages.size());
    for (const auto& stage : stages)
        result.push_back({.enabled = stage.enabled, .id = stage.id, .name = stage.name});
    return result;
}

inline std::vector<clients::sales::models::SalesStage> ToServiceSalesStages(
    const std::vector<models::sales::SalesStage>& stages) {
    std::vector<clients::sales::models::SalesStage> result;
    result.reserve(stages.size());
    for (const auto& stage : stages)
        result.push_back({.enabled = stage.enabled, .id = stage.id, .name = stage.name});
    return result;
}
}  // namespace admin_bff::converters
