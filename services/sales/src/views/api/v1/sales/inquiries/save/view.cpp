#include <helpers/settings.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <views/api/v1/sales/inquiries/save/view.hpp>

namespace sales::views::api::v1::sales::inquiries::save {
namespace {
struct NumberAllocation {
    userver::utils::datetime::Date Date;
    std::int64_t Sequence;
};
}  // namespace

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& value = request.body;
    ValidateReferences(deps, context, value.customer_id, value.manager_subject);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto previous = transaction.Execute(sql::kLockInquiries, organization, value.id);
    if (previous.IsEmpty()) {
        if (value.status != models::InquiryStatus::Draft)
            throw ApiError<400>{"invalid_status", "Новый запрос должен быть черновиком"};
    } else {
        const auto old = models::ParseModelJson(
            previous.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
        if (old.converting || old.shipment_id.has_value())
            throw ApiError<409>{"conversion_started", "Запрос уже передан в создание поставки"};
        if (old.status == models::InquiryStatus::Agreed && value.status == old.status)
            throw ApiError<409>{"agreed_request", "Верните запрос в работу перед изменением"};
    }
    if (value.ready_date.has_value() && value.desired_delivery_date.has_value() &&
        value.desired_delivery_date < value.ready_date)
        throw ApiError<400>{"invalid_dates", "Дата доставки не может быть раньше готовности груза"};
    if (value.status == models::InquiryStatus::Agreed &&
        (!value.cargo_description.has_value() ||
            userver::utils::text::Trim(value.cargo_description.value()).empty()))
        throw ApiError<400>{"cargo_required", "Перед согласованием опишите груз"};
    std::string number;
    if (value.expected_revision == 0) {
        const auto settings = Settings(transaction.Execute(sql::kGetSettings, organization));
        const auto allocation = transaction
                                    .Execute(sql::kNextDocumentNumber, organization,
                                        std::string{"inquiry"}, settings.reset_number_yearly)
                                    .AsSingleRow<NumberAllocation>(pg::kRowTag);
        number = platform::FormatDocumentNumber(
            settings.inquiry_number_template, allocation.Date, allocation.Sequence);
        if (number.size() > 128)
            throw ApiError<400>{"number_too_long", "Номер запроса длиннее 128 символов"};
    }
    const auto document = json::ValueBuilder(value).ExtractValue();
    const auto changed = value.expected_revision == 0
        ? transaction.Execute(sql::kCreateInquiries, organization, document, number)
        : transaction.Execute(
              sql::kUpdateInquiries, organization, document, value.expected_revision);
    if (changed.IsEmpty())
        throw ApiError<409>{
            "revision_conflict", "Запись уже существует, изменена или передана в поставку"};
    const auto rows = transaction.Execute(sql::kGetInquiries, organization, value.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    transaction.Execute(sql::kWriteHistory, organization, std::string{"inquiries"}, result.id,
        result.revision, platform::auth::GetIdentity(context).ActorId(),
        json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return Response200{.item = result};
}
}  // namespace sales::views::api::v1::sales::inquiries::save
