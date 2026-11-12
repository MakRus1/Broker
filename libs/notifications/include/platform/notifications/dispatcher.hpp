#pragma once
#include <platform/auth/service_token.hpp>
#include <platform/outbox/component.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <utility>

namespace platform::notifications {
inline constexpr std::string_view kPublishEventTopic = "notifications.publish";

// Типы берутся из клиента сервиса: библиотека не зависит от namespace его кодогенерации.
template <typename ClientComponent, typename PublishRequest>
class Dispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "notification-dispatcher";

    Dispatcher(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : ComponentBase(config, context),
          client_(context.FindComponent<ClientComponent>().GetClient()),
          tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()) {
        auto& outbox = context.FindComponent<platform::outbox::Component>();
        outbox.RegisterHandler(
            std::string{kPublishEventTopic}, [this](const platform::outbox::Message& message) {
                auto event = ParseModelJson(message.Payload, std::type_identity<Event>{});
                client_.ApiInternalV1EventsPublish(
                    PublishRequest{.body = std::move(event)}, tokens_.GetAccessToken());
            });
    }

    static userver::yaml_config::Schema GetStaticConfigSchema() {
        return ComponentBase::GetStaticConfigSchema();
    }

  private:
    using Client = std::remove_cvref_t<decltype(std::declval<ClientComponent>().GetClient())>;
    using Event = decltype(std::declval<PublishRequest>().body);
    Client client_;
    platform::auth::ServiceTokenProvider& tokens_;
};
}  // namespace platform::notifications

template <typename ClientComponent, typename PublishRequest>
inline constexpr bool userver::components::kHasValidate<
    platform::notifications::Dispatcher<ClientComponent, PublishRequest>> = true;
