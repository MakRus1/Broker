#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/members/create/view.hpp>

namespace organizations::views::api::admin::organizations::members::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        const auto display_name = userver::utils::text::Trim(request.body.display_name);
        if (display_name.empty() || request.body.email.find('@') == std::string::npos)
            throw ApiError{400, "invalid_profile", "Укажите имя и корректный email"};
        const auto role = models::EnumValue(request.body.role);
        userver::formats::json::ValueBuilder intent;
        intent["username"] = request.body.username;
        intent["email"] = request.body.email;
        intent["display_name"] = display_name;
        intent["role"] = role;
        const auto payload = intent.ExtractValue();
        {
            auto transaction =
                deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            RequireAdministrator(transaction, request.body.organization_id, identity);
            transaction.Execute(sql::kSaveProvisioning, request.body.request_id,
                request.body.organization_id, identity.ActorId(), payload);
            const auto [organization, actor, saved, subject] =
                transaction.Execute(sql::kGetProvisioning, request.body.request_id)
                    .AsSingleRow<ProvisioningRow>(pg::kRowTag);
            if (organization != request.body.organization_id || actor != identity.ActorId() ||
                saved != payload)
                throw ApiError{409, "request_conflict",
                    "Идентификатор запроса уже использован для других данных"};
            if (subject.has_value()) {
                const auto member = Convert(transaction
                                                .Execute(sql::kGetMembership, organization,
                                                    identity.Issuer, subject.value())
                                                .AsSingleRow<pgtypes::MemberV1>());
                transaction.Commit();
                return Response201{.member = member};
            }
            transaction.Commit();
        }
        const auto user = deps.users->Create(request.body.request_id, request.body.username,
            request.body.email, display_name, request.body.temporary_password);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.body.organization_id, identity);
        const auto [organization, actor, saved, subject] =
            transaction.Execute(sql::kGetProvisioning, request.body.request_id)
                .AsSingleRow<ProvisioningRow>(pg::kRowTag);
        if (subject.has_value()) {
            const auto member = Convert(
                transaction
                    .Execute(sql::kGetMembership, organization, identity.Issuer, subject.value())
                    .AsSingleRow<pgtypes::MemberV1>());
            transaction.Commit();
            return Response201{.member = member};
        }
        const auto member = Convert(
            transaction.Execute(sql::kAddMember, organization, identity.Issuer, Convert(user, role))
                .AsSingleRow<pgtypes::MemberV1>());
        transaction.Execute(sql::kCompleteProvisioning, request.body.request_id, user.Subject);
        WriteAudit(transaction, organization, identity, "member.created", member);
        transaction.Commit();
        return Response201{.member = member};
    } catch (const ::organizations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            return Response503{.error = error.Code, .message = error.what()};
        }
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::admin::organizations::members::create
