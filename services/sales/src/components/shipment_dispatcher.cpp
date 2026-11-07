#include <components/shipment_dispatcher.hpp>
#include <helpers/storage.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
namespace json = userver::formats::json;

struct DeliveryResult {
    std::optional<std::string> ShipmentId;
    std::optional<std::string> Rejection;
};

ShipmentDispatcher::ShipmentDispatcher(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), deps_(ResolveDeps(context)),
      tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()) {
    context.FindComponent<platform::outbox::Component>().RegisterHandler(
        "shipments.create",
        [this](const platform::outbox::Message& message) -> DeliveryResult {
            namespace api = clients::shipments::ApiInternalV1ShipmentsEnsureCreated;
            try {
                const auto result = deps_.shipments.ApiInternalV1ShipmentsEnsureCreated(
                    {.body = api::ParseBody(message.Payload)}, tokens_.GetAccessToken());
                return {.ShipmentId = result.shipment.id, .Rejection = std::nullopt};
            } catch (const platform::http::ErrorByStatus<400>& error) {
                return {.ShipmentId = std::nullopt,
                    .Rejection = std::string{error.what()}.substr(0, 2000)};
            }
            // Недоступность, 403 и конфликты протокола требуют повтора/исправления сервиса,
            // а не изменения уже отправленной команды пользователем.
        },
        [](helpers::pg::Transaction& transaction, const platform::outbox::Message& message,
            const DeliveryResult& result) {
            using namespace helpers;
            const auto organization = message.Payload["organization_id"].As<std::string>();
            const auto id = message.Payload["source"]["id"].As<std::string>();
            const auto actor = message.Payload["actor_issuer"].As<std::string>() + "#" +
                message.Payload["actor_subject"].As<std::string>();
            transaction.Execute(sql::kLockInquiries, organization, id);
            const auto changed = result.ShipmentId.has_value()
                ? transaction.Execute(
                      sql::kCompleteConversion, organization, id, result.ShipmentId.value())
                : transaction.Execute(
                      sql::kRejectConversion, organization, id, result.Rejection.value());
            if (!changed.IsEmpty()) {
                const auto inquiry = Read<models::Inquiry>(
                    transaction.Execute(sql::kGetInquiries, organization, id));
                transaction.Execute(sql::kWriteHistory, organization, std::string{"inquiries"}, id,
                    inquiry.revision, actor, json::ValueBuilder(inquiry).ExtractValue());
            }
        });
}

userver::yaml_config::Schema ShipmentDispatcher::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: Создание поставок из согласованных запросов
additionalProperties: false
properties: {}
)");
}
}  // namespace sales
