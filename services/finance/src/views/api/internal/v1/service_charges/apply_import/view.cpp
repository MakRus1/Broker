#include <helpers/history/snapshot.hpp>
#include <helpers/service_charges.hpp>
#include <set>
#include <views/api/internal/v1/service_charges/apply_import/view.hpp>

namespace finance::views::api::internal::v1::service_charges::apply_import {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& command = request.body;
    auto actor = platform::auth::GetIdentity(context);
    if (actor.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    const auto producer = actor.ClientId;
    actor.Subject = command.actor_subject;
    const auto document = userver::formats::json::ValueBuilder(command).ExtractValue();
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inserted = transaction.Execute(
        sql::kRegisterServiceChargeImport, command.organization_id, producer, command.id, document);
    if (inserted.IsEmpty()) {
        const auto old = transaction.Execute(
            sql::kGetServiceChargeImport, command.organization_id, producer, command.id);
        if (old.AsSingleRow<userver::formats::json::Value>() != document)
            throw ApiError<409>{
                "import_conflict", "Ключ переноса уже использован для другого расчёта"};
        transaction.Commit();
        return Response200{.imported = true};
    }
    std::set<std::string> ids;
    for (const auto& item : command.items) {
        const auto& charge = item.charge;
        if (!ids.insert(charge.id).second || charge.expected_revision != 0 || !charge.shipment_id ||
            charge.transportation_id || charge.target_invoice_id || charge.archived ||
            charge.currency != charge.invoice_currency || charge.contract_id ||
            (charge.manual_invoice_rate_millionths &&
                *charge.manual_invoice_rate_millionths != 1000000))
            throw ApiError<400>{"invalid_import", "Некорректные услуги для переноса в поставку"};
        const auto labels =
            transaction.Execute(sql::kGetFinancialLabel, command.organization_id, charge.label_id);
        if (labels.IsEmpty())
            throw ApiError<404>{"label_not_found", "Метка не найдена в организации"};
        auto label = Convert(labels.AsSingleRow<pgtypes::FinancialLabelV1>());
        // Названия, суммы и НДС берём из согласованного снимка, а не текущего справочника.
        label.name = item.label_name;
        label.archived = false;
        label.direction = models::FinancialLabelDirection::Both;
        const auto value = PrepareServiceCharge(charge, label,
            {.CounterpartyName = item.counterparty_name, .ContractNumber = std::nullopt},
            {.Millionths = 1000000, .Date = charge.service_date, .Source = "identity"});
        const auto rows =
            transaction.Execute(sql::kCreateServiceCharge, command.organization_id, value);
        if (rows.IsEmpty())
            throw ApiError<409>{"charge_conflict", "Услуга уже существует вне переноса"};
        const auto result = Convert(rows.AsSingleRow<pgtypes::ServiceChargeV1>());
        transaction.Execute(sql::kWriteServiceChargeHistory, command.organization_id, result.id,
            result.revision, actor.ActorId(), HistorySnapshot(result, actor));
    }
    transaction.Commit();
    return Response200{.imported = true};
}
}  // namespace finance::views::api::internal::v1::service_charges::apply_import
