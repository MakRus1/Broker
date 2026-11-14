#pragma once

#include <chrono>
#include <stdexcept>

#include <platform/configs/runtime.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/raw_component_base.hpp>
#include <userver/dynamic_config/storage/component.hpp>
#include <userver/dynamic_config/updater/component.hpp>
#include <userver/dynamic_config/updates_sink/find.hpp>
#include <userver/dynamic_config/value.hpp>
#include <userver/engine/single_consumer_event.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace platform::configs {

template <typename Contract>
class DeliverySink final : public userver::components::DynamicConfigUpdatesSinkBase {
  public:
    static constexpr std::string_view kName = "platform-config-delivery";
    using Base = userver::components::DynamicConfigUpdatesSinkBase;
    using Base::SetConfig;

    DeliverySink(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : Base(config, context), sink_(userver::dynamic_config::FindUpdatesSink(config, context)) {
        auto& storage = context.FindComponent<userver::components::DynamicConfig>();
        // До первого обновления компоненты могут читать значения из кеша или локальные defaults.
        if (static_cast<userver::components::RawComponentBase&>(storage).GetComponentHealth() !=
            userver::components::ComponentHealth::kOk)
            sink_.SetConfig(kName, storage.GetDefaultDocsMap());
    }

    void WaitForSnapshot() {
        if (!snapshot_received_.WaitForEventFor(std::chrono::seconds{120}))
            throw std::runtime_error("Первый снимок Configs не получен");
    }

    void SetConfig(std::string_view updater, userver::dynamic_config::DocsMap&& docs) override {
        Contract::Validate(docs);
        sink_.SetConfig(updater, std::move(docs));
        snapshot_received_.Send();
    }

    void NotifyLoadingFailed(std::string_view updater, std::string_view error) override {
        sink_.NotifyLoadingFailed(updater, error);
    }

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<Base>(R"(
type: object
description: Проверка и применение снимков Configs
additionalProperties: false
properties:
    updates-sink:
        type: string
        description: Хранилище dynamic-config
)");
    }

  private:
    Base& sink_;
    userver::engine::SingleConsumerEvent snapshot_received_{
        userver::engine::SingleConsumerEvent::NoAutoReset{}};
};

template <typename Contract> class ConfigStartup final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "platform-config-startup";

    ConfigStartup(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : ComponentBase(config, context) {
        // После создания updater уже сделал первый запрос и запустил периодические обновления.
        context.FindComponent<userver::components::DynamicConfigClientUpdater>();
        context.FindComponent<DeliverySink<Contract>>().WaitForSnapshot();
    }
};

}  // namespace platform::configs
