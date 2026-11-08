#include <helpers/database.hpp>
#include <views/api/v1/sales/contacts/update/view.hpp>

namespace sales::views::api::v1::sales::contacts::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
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
    return Response200{.item = contact};
}
}  // namespace sales::views::api::v1::sales::contacts::update
