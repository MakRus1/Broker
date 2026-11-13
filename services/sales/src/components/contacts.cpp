#include <components/contacts.hpp>
#include <models/errors.hpp>
#include <models/stored_contact.hpp>
#include <sales/sql_queries.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
void ValidateContact(const std::string& result, const std::optional<std::string>& next_action,
    const std::optional<userver::utils::datetime::Date>& next_action_date) {
    if (result.find_first_not_of(" \t\r\n") == std::string::npos ||
        (next_action && next_action->find_first_not_of(" \t\r\n") == std::string::npos))
        throw sales::ApiError<400>{
            "empty_contact", "Укажите результат общения и непустое следующее действие"};
    if (next_action.has_value() != next_action_date.has_value())
        throw sales::ApiError<400>{
            "invalid_next_action", "Укажите следующее действие вместе со сроком"};
}
}  // namespace

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

ContactsComponent::ContactsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

userver::yaml_config::Schema ContactsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ContactAddition ContactsComponent::AddContact(const std::string& organization,
    const platform::auth::Identity& identity, const models::AddSalesContactRequest& request) const {
    const auto& body = request;
    ValidateContact(body.result, body.next_action, body.next_action_date);

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked = transaction.Execute(sql::kLockCards, organization, body.card_id);
    if (locked.IsEmpty())
        throw ApiError<404>{"not_found", "Карточка клиента не найдена в организации"};

    const auto document = json::ValueBuilder(body).ExtractValue();
    const auto previous = transaction.Execute(sql::kGetContact, organization, body.id);
    auto card = models::ParseModelJson(
        transaction.Execute(sql::kGetCards, organization, body.card_id).AsSingleRow<json::Value>(),
        std::type_identity<models::SalesCard>{});
    if (!previous.IsEmpty()) {
        const auto saved = previous.AsSingleRow<models::StoredContact>(pg::kRowTag);
        if (saved.Request != document)
            throw ApiError<409>{
                "contact_conflict", "Запись контакта уже сохранена с другими данными"};
        auto contact =
            models::ParseModelJson(saved.Contact, std::type_identity<models::SalesContact>{});
        transaction.Commit();
        return models::ContactAddition{.card = card, .item = contact};
    }
    if (card.revision != body.expected_revision)
        throw ApiError<409>{"revision_conflict", "Карточка клиента изменена. Откройте её заново"};
    const auto contact = models::ParseModelJson(
        transaction
            .Execute(sql::kAddContact, organization, document, identity.Subject, identity.Issuer)
            .AsSingleRow<json::Value>(),
        std::type_identity<models::SalesContact>{});

    transaction.Execute(sql::kWriteHistory, organization, std::string{"contacts"}, contact.id,
        contact.revision, identity.ActorId(), json::ValueBuilder(contact).ExtractValue());
    if (body.next_action.has_value()) {
        transaction.Execute(sql::kUpdateContactNextAction, organization, body.card_id,
            body.next_action.value(), body.next_action_date.value(), contact.id);
        card =
            models::ParseModelJson(transaction.Execute(sql::kGetCards, organization, body.card_id)
                                       .AsSingleRow<json::Value>(),
                std::type_identity<models::SalesCard>{});
        transaction.Execute(sql::kWriteHistory, organization, std::string{"cards"}, card.id,
            card.revision, identity.ActorId(), json::ValueBuilder(card).ExtractValue());
    }

    transaction.Commit();
    return models::ContactAddition{.card = card, .item = contact};
}

models::ContactsPage ContactsComponent::ListContacts(
    const std::string& organization, const models::ListSalesContactsRequest& request) const {
    if (cluster_
            ->Execute(pg::ClusterHostType::kMaster, sql::kGetCards, organization, request.card_id)
            .IsEmpty())
        throw ApiError<404>{"not_found", "Карточка клиента не найдена в организации"};
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListContacts,
        organization, request.card_id, request.before, request.limit + 1);
    models::ContactsPage response;
    for (const auto& row : rows.AsSetOf<json::Value>()) {
        if (response.items.size() == static_cast<std::size_t>(request.limit)) {
            response.next_before = response.items.back().id;
            break;
        }
        response.items.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesContact>{}));
    }

    return response;
}

models::SalesContact ContactsComponent::UpdateContact(const std::string& organization,
    const platform::auth::Identity& identity,
    const models::UpdateSalesContactRequest& request) const {
    const auto& body = request;
    ValidateContact(body.result, body.next_action, body.next_action_date);

    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockCards, organization, body.card_id);
    const auto rows = tx.Execute(sql::kGetContact, organization, body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контакт не найден в организации"};

    const auto stored = rows.AsSingleRow<models::StoredContact>(pg::kRowTag);
    const auto current =
        models::ParseModelJson(stored.Contact, std::type_identity<models::SalesContact>{});
    if (current.card_id != body.card_id)
        throw ApiError<404>{"not_found", "Контакт не найден в карточке"};
    if (current.revision != body.expected_revision)
        throw ApiError<409>{"revision_conflict", "Контакт изменён. Откройте запись заново"};
    const auto contact =
        models::ParseModelJson(tx.Execute(sql::kUpdateContact, organization, body.id,
                                     json::ValueBuilder(body).ExtractValue())
                                   .AsSingleRow<json::Value>(),
            std::type_identity<models::SalesContact>{});
    tx.Execute(sql::kWriteHistory, organization, std::string{"contacts"}, contact.id,
        contact.revision, identity.ActorId(), json::ValueBuilder(contact).ExtractValue());
    const auto changed = tx.Execute(sql::kCorrectContactNextAction, organization, contact.id,
        json::ValueBuilder(body).ExtractValue());
    if (!changed.IsEmpty()) {
        const auto card = models::ParseModelJson(
            changed.AsSingleRow<json::Value>(), std::type_identity<models::SalesCard>{});
        tx.Execute(sql::kWriteHistory, organization, std::string{"cards"}, card.id, card.revision,
            identity.ActorId(), json::ValueBuilder(card).ExtractValue());
    }

    tx.Commit();
    return contact;
}

std::vector<models::SalesContactHistoryEntry> ContactsComponent::GetContactHistory(
    const std::string& organization, const models::HistoryRequest& request) const {
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kHistoryContacts,
        organization, request.id, request.after_revision.value_or(0));
    std::vector<models::SalesContactHistoryEntry> response;
    response.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesContactHistoryEntry>{}));

    return response;
}
}  // namespace sales
