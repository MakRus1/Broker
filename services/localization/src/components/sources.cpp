#include <components/sources.hpp>
#include <converters/sources.hpp>
#include <helpers/parameters.hpp>
#include <localization/sql_queries.hpp>
#include <map>
#include <platform/handler_error.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace localization {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
template <int Status> using ApiError = platform::http::HandlerError<Status>;
using helpers::Parameters;

SourcesComponent::SourcesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()) {
}

userver::yaml_config::Schema SourcesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

int SourcesComponent::RegisterSourceCatalog(const models::EditableCatalog& catalog) const {
    const auto collect = [](const auto& entries) {
        static const std::regex key_pattern{"[A-Za-z0-9_.-]{1,200}"};
        std::map<std::string, std::string> result;
        for (const auto& entry : entries) {
            if (!std::regex_match(entry.key, key_pattern) || entry.value.empty() ||
                entry.value.size() > 10000 || !result.emplace(entry.key, entry.value).second)
                throw ApiError<400>{"invalid_source", "Некорректный исходный каталог"};
        }
        return result;
    };
    const auto ru = collect(catalog.ru);
    const auto en = collect(catalog.en);
    if (ru.empty() || ru.size() != en.size())
        throw ApiError<400>{"invalid_source", "Наборы ключей ru/en должны совпадать"};

    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockSource);
    const auto current = tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>();
    std::vector<SourceTranslation> rows;
    for (const auto& [key, text] : ru) {
        const auto english = en.find(key);
        const auto names = Parameters(text);
        if (english == en.end() || names != Parameters(english->second))
            throw ApiError<400>{"invalid_parameters", "Не совпадают параметры ru/en: " + key};
        const std::vector<std::string> parameters{names.begin(), names.end()};
        if (current["ru"].HasMember(key) &&
            Parameters(current["ru"][key].As<std::string>()) != names)
            throw ApiError<409>{"source_parameters_changed",
                "Параметры ключа изменены. Используйте новый ключ: " + key};
        rows.push_back({.Key = key, .Ru = text, .En = english->second, .Parameters = parameters});
    }

    const auto values = converters::Convert(rows);
    if (json::ToString(values).size() > 2 * 1024 * 1024)
        throw ApiError<400>{"catalog_too_large", "Каталог превышает 2 МБ"};

    tx.Execute(sql::kRegisterSource, values);
    tx.Commit();
    return static_cast<int>(ru.size());
}
}  // namespace localization
