#pragma once
#include <models/pg/sales_settings.hpp>
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace sales::converters {
inline models::SalesSettings ToSalesSettings(const models::pg::SalesSettings& settings) {
    models::SalesSettings result{.inquiry_number_template = settings.InquiryNumberTemplate,
        .reset_number_yearly = settings.ResetNumberYearly,
        .revision = settings.Revision,
        .stages = {}};
    result.stages.reserve(settings.Stages.GetSize());
    for (const auto& stage : settings.Stages)
        result.stages.push_back(
            models::ParseModelJson(stage, std::type_identity<models::SalesStage>{}));
    return result;
}

}  // namespace sales::converters
