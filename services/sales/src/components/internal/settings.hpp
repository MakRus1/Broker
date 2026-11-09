#pragma once
#include <components/internal/references.hpp>
#include <platform/document_number.hpp>
#include <postgresql/types.hpp>
#include <set>

namespace sales::detail {
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

inline void ValidateStages(
    const models::UpdateSalesSettingsRequest& request, const models::SalesSettings& previous) {
    std::set<std::string> ids;
    for (const auto& stage : request.stages) {
        if (!ids.insert(stage.id).second || userver::utils::text::Trim(stage.name).empty())
            throw ApiError<400>{"invalid_stage", "Этапы должны иметь уникальные коды и названия"};
    }
    // Отключение сохраняет ссылки и историю существующих карточек.
    for (const auto& stage : previous.stages)
        if (!ids.contains(stage.id))
            throw ApiError<409>{"stage_in_use", "Отключите этап вместо его удаления"};
}

inline void ValidateStage(const models::SalesSettings& settings, const std::string& id,
    const std::optional<std::string>& previous) {
    for (const auto& stage : settings.stages)
        if (stage.id == id && (stage.enabled || previous == id))
            return;
    throw ApiError<400>{"invalid_stage", "Выберите действующий этап продаж"};
}
}  // namespace sales::detail
