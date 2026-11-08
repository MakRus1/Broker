#include <helpers/database.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <views/api/v1/sales/contacts/add/view.hpp>

namespace sales::views::api::v1::sales::contacts::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto& body = request.body;
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
        return Response200{.card = card, .item = contact};
    }
    if (card.revision != body.expected_revision)
        throw ApiError<409>{"revision_conflict", "Карточка клиента изменена. Откройте её заново"};
    const auto contact = models::ParseModelJson(
        transaction
            .Execute(sql::kAddContact, organization, document, identity.Subject, identity.Issuer)
            .AsSingleRow<json::Value>(),
        std::type_identity<models::SalesContact>{});
    if (body.next_action.has_value()) {
        transaction.Execute(sql::kUpdateContactNextAction, organization, body.card_id,
            body.next_action.value(), body.next_action_date.value());
        card =
            models::ParseModelJson(transaction.Execute(sql::kGetCards, organization, body.card_id)
                                       .AsSingleRow<json::Value>(),
                std::type_identity<models::SalesCard>{});
        transaction.Execute(sql::kWriteHistory, organization, std::string{"cards"}, card.id,
            card.revision, identity.ActorId(), json::ValueBuilder(card).ExtractValue());
    }
    transaction.Commit();
    return Response200{.card = card, .item = contact};
}
}  // namespace sales::views::api::v1::sales::contacts::add
