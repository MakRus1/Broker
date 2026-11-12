#include <components/shipment_dispatcher.hpp>
#include <converters/service_charge_import.hpp>
#include <events/shipment_command.hpp>
#include <helpers/proposal_storage.hpp>
#include <models/errors.hpp>
#include <models/shipment_delivery.hpp>
#include <sales/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
namespace json = userver::formats::json;

namespace pg = userver::storages::postgres;

void ShipmentDispatcher::TransferProposal(const ShipmentCommand& command,
    const std::string& shipment_id, const std::string& token) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetInquiries,
        command.organization_id, command.source.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запрос не найден"};
    const auto inquiry = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (!inquiry.accepted_proposal_id)
        return;
    const auto proposal = helpers::ReadProposal(cluster_->Execute(pg::ClusterHostType::kMaster,
        sql::kGetProposal, command.organization_id, *inquiry.accepted_proposal_id));
    if (proposal.status != models::ProposalStatus::Accepted ||
        proposal.revision != *inquiry.accepted_proposal_revision)
        throw ApiError<409>{"proposal_changed", "Согласованная версия предложения изменилась"};
    auto batch = converters::ToServiceChargeImport(command, shipment_id, inquiry, proposal);
    finance_.ApiInternalV1ServiceChargesApplyImport({.body = std::move(batch)}, token);
}

ShipmentDispatcher::ShipmentDispatcher(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      finance_(context.FindComponent<clients::finance::Component>().GetClient()),
      shipments_(context.FindComponent<clients::shipments::Component>().GetClient()),
      notifications_(context.FindComponent<clients::notifications::Component>().GetClient()),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()),
      tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()) {
    context.FindComponent<platform::outbox::Component>().RegisterHandler(
        kCreateShipment,
        [this](const ShipmentCommand& command) -> models::ShipmentDelivery {
            std::string shipment_id;
            try {
                const auto result = shipments_.ApiInternalV1ShipmentsEnsureCreated(
                    {.body = {.actor_issuer = command.actor_issuer,
                         .actor_subject = command.actor_subject,
                         .organization_id = command.organization_id,
                         .shipment = command.shipment,
                         .source = command.source}},
                    tokens_.GetAccessToken());
                shipment_id = result.shipment.id;
            } catch (const platform::http::ErrorByStatus<400>& error) {
                return {.ShipmentId = std::nullopt,
                    .Rejection = std::string{error.what()}.substr(0, 2000)};
            }
            TransferProposal(command, shipment_id, tokens_.GetAccessToken());
            return {.ShipmentId = shipment_id, .Rejection = std::nullopt};
            // Недоступность, 403 и конфликты протокола требуют повтора/исправления сервиса,
            // а не изменения уже отправленной команды пользователем.
        },
        [](pg::Transaction& transaction, const ShipmentCommand& command,
            const models::ShipmentDelivery& result) {
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
                if (result.Rejection.has_value()) {
                    namespace notification = clients::notifications::models;
                    const auto event_id =
                        "inquiry-shipment-failed:" + id + ":" + std::to_string(inquiry.revision);
                    const notification::PublishedEvent event{
                        .value = notification::InquiryShipmentCreationFailed{.id = event_id,
                            .organization_id = organization,
                            .payload = {.inquiry_id = inquiry.id,
                                .inquiry_number = inquiry.number,
                                .recipient_issuer = command.actor_issuer,
                                .recipient_subject = inquiry.manager_subject,
                                .revision = inquiry.revision}}};
                    platform::outbox::Enqueue(transaction, kPublishNotification, event_id, event);
                }
                transaction.Execute(sql::kWriteHistory, organization, std::string{"inquiries"}, id,
                    inquiry.revision, actor, json::ValueBuilder(inquiry).ExtractValue());
            }
        });
    context.FindComponent<platform::outbox::Component>().RegisterHandler(
        kPublishNotification, [this](const clients::notifications::models::PublishedEvent& event) {
            namespace api = clients::notifications::ApiInternalV1EventsPublish;
            notifications_.ApiInternalV1EventsPublish(
                {.body =
                        api::ParseBody(userver::formats::json::ValueBuilder(event).ExtractValue())},
                tokens_.GetAccessToken());
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
