#include <components/shipment_dispatcher.hpp>
#include <helpers/database.hpp>
#include <helpers/shipment_command.hpp>
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
        kCreateShipment,
        [this](const ShipmentCommand& command) -> DeliveryResult {
            try {
                const auto result = deps_.shipments.ApiInternalV1ShipmentsEnsureCreated(
                    {.body = {.actor_issuer = command.actor_issuer,
                         .actor_subject = command.actor_subject,
                         .organization_id = command.organization_id,
                         .shipment = command.shipment,
                         .source = command.source}},
                    tokens_.GetAccessToken());
                return {.ShipmentId = result.shipment.id, .Rejection = std::nullopt};
            } catch (const platform::http::ErrorByStatus<400>& error) {
                return {.ShipmentId = std::nullopt,
                    .Rejection = std::string{error.what()}.substr(0, 2000)};
            }
            // Недоступность, 403 и конфликты протокола требуют повтора/исправления сервиса,
            // а не изменения уже отправленной команды пользователем.
        },
        [](helpers::pg::Transaction& transaction, const ShipmentCommand& command,
            const DeliveryResult& result) {
            using namespace helpers;
            const auto organization = command.organization_id;
            const auto id = command.source.id;
            const auto actor = command.actor_issuer + "#" + command.actor_subject;
            transaction.Execute(sql::kLockInquiries, organization, id);
            const auto changed = result.ShipmentId.has_value()
                ? transaction.Execute(
                      sql::kCompleteConversion, organization, id, result.ShipmentId.value())
                : transaction.Execute(
                      sql::kRejectConversion, organization, id, result.Rejection.value());
            if (!changed.IsEmpty()) {
                const auto rows = transaction.Execute(sql::kGetInquiries, organization, id);
                if (rows.IsEmpty())
                    throw ApiError<404>{"not_found", "Запись не найдена в организации"};
                const auto inquiry = models::ParseModelJson(
                    rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
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
