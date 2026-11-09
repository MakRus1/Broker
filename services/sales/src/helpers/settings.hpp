#pragma once
#include <helpers/database.hpp>
#include <postgresql/types.hpp>

namespace sales::helpers {
inline models::SalesSettings Settings(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.revision = 0, .stages = {}};
    const auto settings = rows.AsSingleRow<pgtypes::SalesSettingsV1>();
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

}  // namespace sales::helpers
