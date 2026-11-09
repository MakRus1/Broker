#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <components/internal/database.hpp>
#include <components/internal/proposal_storage.hpp>
#include <components/shipment_dispatcher.hpp>
#include <events/shipment_command.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/boost_uuid5.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
namespace json = userver::formats::json;

using namespace detail;

void ShipmentDispatcher::TransferProposal(const ShipmentCommand& command,
    const std::string& shipment_id, const std::string& token) const {
    const auto& deps = deps_;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetInquiries,
        command.organization_id, command.source.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запрос не найден"};
    const auto inquiry = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (!inquiry.accepted_proposal_id)
        return;
    const auto proposal = ReadProposal(deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kGetProposal, command.organization_id, *inquiry.accepted_proposal_id));
    if (proposal.status != models::ProposalStatus::Accepted ||
        proposal.revision != *inquiry.accepted_proposal_revision)
        throw ApiError<409>{"proposal_changed", "Согласованная версия предложения изменилась"};
    namespace fm = clients::finance::models;
    const auto namespace_id = boost::uuids::string_generator{}(proposal.input.id);
    clients::finance::ApiInternalV1ServiceChargesApplyImport::RequestBody batch;
    batch.organization_id = command.organization_id;
    batch.source = {
        .id = proposal.input.id, .revision = proposal.revision, .type = "sales.proposal"};
    batch.id = boost::uuids::to_string(userver::utils::generators::GenerateBoostUuidV5(
        namespace_id, std::to_string(proposal.revision)));
    batch.actor_subject = command.actor_subject;
    batch.actor_issuer = command.actor_issuer;
    for (const auto& line : proposal.lines) {
        const auto add = [&](const models::ProposalPrice& price, bool income) {
            fm::SaveServiceChargeRequest charge;
            charge.id = boost::uuids::to_string(
                userver::utils::generators::GenerateBoostUuidV5(namespace_id,
                    line.input.id + (income ? ":sale:" : ":purchase:") +
                        std::to_string(proposal.revision)));
            charge.shipment_id = shipment_id;
            charge.label_id = line.input.label_id;
            charge.unit =
                fm::ParseEnumScalar(models::EnumValue(line.input.unit), fm::FinancialLabelUnit{});
            charge.direction =
                income ? fm::ServiceChargeDirection::Income : fm::ServiceChargeDirection::Expense;
            charge.counterparty_id = income ? inquiry.customer_id : *line.input.supplier_id;
            charge.quantity_milli = line.input.quantity_milli;
            charge.price_minor = price.price_minor;
            charge.currency = price.currency;
            charge.invoice_currency = price.currency;
            charge.vat_mode = fm::ParseEnumScalar(models::EnumValue(price.vat_mode), fm::VatMode{});
            charge.price_mode =
                fm::ParseEnumScalar(models::EnumValue(price.price_mode), fm::PriceMode{});
            charge.vat_rate_basis_points = price.vat_rate_basis_points;
            charge.note = line.input.note;
            charge.archived = false;
            charge.expected_revision = 0;
            charge.service_date = proposal.input.calculation_date;
            batch.items.push_back({.charge = charge,
                .counterparty_name =
                    income ? proposal.customer_name : line.supplier_name.value_or(""),
                .label_name = line.label_name});
        };
        add(line.input.sale, true);
        if (line.input.purchase)
            add(*line.input.purchase, false);
    }
    deps.finance.ApiInternalV1ServiceChargesApplyImport({.body = std::move(batch)}, token);
}

struct DeliveryResult {
    std::optional<std::string> ShipmentId;
    std::optional<std::string> Rejection;
};

ShipmentDispatcher::ShipmentDispatcher(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.finance = context.FindComponent<clients::finance::Component>().GetClient(),
          .shipments = context.FindComponent<clients::shipments::Component>().GetClient(),
          .notifications = context.FindComponent<clients::notifications::Component>().GetClient(),
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()},
      tokens_(context.FindComponent<platform::auth::ServiceTokenProvider>()) {
    context.FindComponent<platform::outbox::Component>().RegisterHandler(
        kCreateShipment,
        [this](const ShipmentCommand& command) -> DeliveryResult {
            std::string shipment_id;
            try {
                const auto result = deps_.shipments.ApiInternalV1ShipmentsEnsureCreated(
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
        [](detail::pg::Transaction& transaction, const ShipmentCommand& command,
            const DeliveryResult& result) {
            using namespace detail;
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
            deps_.notifications.ApiInternalV1EventsPublish(
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
