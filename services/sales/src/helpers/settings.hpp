#pragma once
#include <helpers/references.hpp>
#include <platform/document_number.hpp>
#include <set>
#include <tuple>

namespace sales::helpers {
inline models::SalesSettings Settings(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.revision = 0, .stages = {}};
    const auto [revision, stages, number_template, yearly] =
        rows.AsSingleRow<std::tuple<std::int64_t, json::Value, std::string, bool>>(pg::kRowTag);
    json::ValueBuilder value;
    value["revision"] = revision;
    value["stages"] = stages;
    value["inquiry_number_template"] = number_template;
    value["reset_number_yearly"] = yearly;
    return models::ParseModelJson(
        value.ExtractValue(), std::type_identity<models::SalesSettings>{});
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
}  // namespace sales::helpers
