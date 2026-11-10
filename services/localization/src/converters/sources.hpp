#pragma once
#include <models/source_translation.hpp>
#include <userver/formats/json.hpp>

namespace localization::converters {
inline userver::formats::json::Value Convert(const std::vector<SourceTranslation>& entries) {
    namespace json = userver::formats::json;
    json::ValueBuilder rows{json::Type::kArray};
    for (const auto& entry : entries) {
        json::ValueBuilder row;
        row["key"] = entry.Key;
        row["ru"] = entry.Ru;
        row["en"] = entry.En;
        row["parameters"] = entry.Parameters;
        rows.PushBack(row.ExtractValue());
    }
    return rows.ExtractValue();
}
}  // namespace localization::converters
