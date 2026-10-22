#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/members/update_profile/view.hpp>

namespace organizations::views::api::admin::organizations::members::update_profile {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        {
            auto access =
                deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            RequireAdministrator(access, request.body.organization_id, identity);
            access.Commit();
        }
        const auto last = userver::utils::text::Trim(request.body.last_name);
        const auto first = userver::utils::text::Trim(request.body.first_name);
        auto middle = request.body.middle_name.value_or(std::nullopt);
        if (middle.has_value()) {
            middle = userver::utils::text::Trim(middle.value());
            if (middle.value().empty())
                middle.reset();
        }
        if (last.empty() || first.empty())
            throw ApiError{400, "invalid_profile", "Укажите фамилию и имя"};
        const auto existing =
            deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetMembership,
                request.body.organization_id, identity.Issuer, request.body.subject);
        if (existing.IsEmpty())
            throw ApiError{404, "member_not_found", "Сотрудник не найден"};
        // Сетевой вызов выполняется до транзакции БД. Повторное задание ФИО идемпотентно.
        const auto user = deps.users->UpdateProfile(request.body.subject, last, first, middle);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        transaction.Execute(sql::kUpdateMemberProfile, identity.Issuer, request.body.subject,
            user.DisplayName, user.LastName, user.FirstName, user.MiddleName);
        const auto member = Convert(transaction
                                        .Execute(sql::kGetMembership, request.body.organization_id,
                                            identity.Issuer, request.body.subject)
                                        .AsSingleRow<pgtypes::MemberV2>());
        WriteAudit(
            transaction, request.body.organization_id, identity, "member.profile_updated", member);
        transaction.Commit();
        return Response200{.member = member};
    } catch (const ::organizations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response503{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace organizations::views::api::admin::organizations::members::update_profile
