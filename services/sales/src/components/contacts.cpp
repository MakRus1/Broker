#include <components/contacts.hpp>
#include <components/internal/database.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace detail;

ContactsComponent::ContactsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()} {
}

userver::yaml_config::Schema ContactsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

ContactsComponent::AddResult ContactsComponent::Add(const models::AddSalesContactRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    if (body.result.find_first_not_of(" \t\r\n") == std::string::npos ||
        (body.next_action.has_value() &&
            body.next_action->find_first_not_of(" \t\r\n") == std::string::npos))
        throw ApiError<400>{
            "empty_contact", "Укажите результат общения и непустое следующее действие"};
    if (body.next_action.has_value() != body.next_action_date.has_value())
        throw ApiError<400>{"invalid_next_action", "Укажите следующее действие вместе со сроком"};
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto locked = transaction.Execute(sql::kLockCards, organization, body.card_id);
    if (locked.IsEmpty())
        throw ApiError<404>{"not_found", "Карточка клиента не найдена в организации"};
    const auto document = json::ValueBuilder(body).ExtractValue();
    const auto previous = transaction.Execute(sql::kGetContact, organization, body.id);
    auto card = models::ParseModelJson(
        transaction.Execute(sql::kGetCards, organization, body.card_id).AsSingleRow<json::Value>(),
        std::type_identity<models::SalesCard>{});
    if (!previous.IsEmpty()) {
        struct ContactReplay {
            json::Value Contact;
            json::Value Request;
        };

        const auto saved = previous.AsSingleRow<ContactReplay>(pg::kRowTag);
        if (saved.Request != document)
            throw ApiError<409>{
                "contact_conflict", "Запись контакта уже сохранена с другими данными"};
        auto contact =
            models::ParseModelJson(saved.Contact, std::type_identity<models::SalesContact>{});
        transaction.Commit();
        return AddResult{.card = card, .item = contact};
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
    return AddResult{.card = card, .item = contact};
}

ContactsComponent::ListResult ContactsComponent::List(
    const models::ListSalesContactsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    if (deps.pg_cluster
            ->Execute(pg::ClusterHostType::kMaster, sql::kGetCards, organization, request.card_id)
            .IsEmpty())
        throw ApiError<404>{"not_found", "Карточка клиента не найдена в организации"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kListContacts,
        organization, request.card_id, request.before, request.limit + 1);
    ListResult response;
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

models::SalesContact ContactsComponent::Update(const models::UpdateSalesContactRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request;
    if (body.result.find_first_not_of(" \t\r\n") == std::string::npos ||
        (body.next_action && body.next_action->find_first_not_of(" \t\r\n") == std::string::npos))
        throw ApiError<400>{
            "empty_contact", "Укажите непустой результат общения и следующее действие"};
    if (body.next_action.has_value() != body.next_action_date.has_value())
        throw ApiError<400>{"invalid_next_action", "Укажите следующее действие вместе со сроком"};
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kLockCards, organization, body.card_id);
    const auto rows = tx.Execute(sql::kGetContact, organization, body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контакт не найден в организации"};

    struct StoredContact {
        json::Value Contact;
        json::Value Request;
    };

    const auto stored = rows.AsSingleRow<StoredContact>(pg::kRowTag);
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

std::vector<models::SalesContactHistoryEntry> ContactsComponent::History(
    const models::HistoryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryContacts,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.id,
        request.after_revision.value_or(0));
    std::vector<models::SalesContactHistoryEntry> response;
    response.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesContactHistoryEntry>{}));
    return response;
}
}  // namespace sales
