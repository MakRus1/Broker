#include <components/cargo_suggestions_cleanup.hpp>
#include <configs/keys.hpp>
#include <mutex>
#include <shipments/sql_queries.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
CargoSuggestionsCleanup::CargoSuggestionsCleanup(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), deps_(ResolveDeps(context)),
      registration_(
          context.FindComponent<userver::components::TestsuiteSupport>().GetPeriodicTaskControl(),
          std::string{kName}, task_),
      retry_registration_(
          context.FindComponent<userver::components::TestsuiteSupport>().GetPeriodicTaskControl(),
          "cargo-suggestions-cleanup-backlog", retry_task_) {
    task_.Start(std::string{kName}, std::chrono::hours{24}, [this] {
        pending_ = true;
        Cleanup();
    });
    retry_task_.Start("cargo-suggestions-cleanup-backlog", std::chrono::minutes{1}, [this] {
        if (pending_)
            Cleanup();
    });
}

CargoSuggestionsCleanup::~CargoSuggestionsCleanup() {
    retry_task_.Stop();
    task_.Stop();
}

void CargoSuggestionsCleanup::Cleanup() {
    const std::lock_guard lock(mutex_);
    const auto settings = deps_.configs.GetSnapshot().Get(configs::CARGO_SUGGESTIONS_RETENTION);
    if (!settings.Enabled) {
        pending_ = false;
        return;
    }
    // Ограничиваем работу за один проход. Несколько подов безопасно разбирают разные записи.
    for (int batch = 0; batch < 10; ++batch) {
        const auto removed = deps_.pg_cluster->Execute(
            userver::storages::postgres::ClusterHostType::kMaster, sql::kCleanupCargoSuggestions,
            settings.ActiveDays, settings.RemovedDays, settings.BatchSize);
        if (removed.Size() < static_cast<std::size_t>(settings.BatchSize)) {
            pending_ = false;
            return;
        }
    }
}

userver::yaml_config::Schema CargoSuggestionsCleanup::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Ежедневное удаление устаревших описаний груза
additionalProperties: false
properties: {}
)");
}
}  // namespace shipments
