#include <components/members.hpp>
#include <converters/organizations.hpp>
#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <platform/contact_validation.hpp>
#include <registry/admin.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace organizations {
using namespace helpers;
using converters::Convert;
using converters::ConvertRows;
using namespace registry_query;

namespace {
struct ProvisioningRow {
    std::string Organization;
    std::string Actor;
    userver::formats::json::Value Payload;
    std::optional<std::string> Subject;
};

}  // namespace

MembersComponent::MembersComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context), users_(context.FindComponent<Users>()),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-organizations")
                   .GetCluster()) {}

userver::yaml_config::Schema MembersComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::MembersListResult MembersComponent::List(const models::ListMembersRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.organization_id, identity);
        const auto scope = registry::CursorScope(request, request.organization_id);
        if (!registry::CursorMatches(request, scope, EnumValue(request.sort) == "subject"))
            throw ApiError<400>{
                "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        const auto query = MemberQuery(request);
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<registry::Row<pgtypes::MemberV2>>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        if (more)
            rows.pop_back();
        std::optional<models::MemberListCursor> next;
        if (more)
            next = models::MemberListCursor{
                .id = rows.back().Value.Subject, .scope = scope, .value = rows.back().SortKey};
        const auto items =
            registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });
        transaction.Commit();
        return models::MembersListResult{.items = items, .next_cursor = next};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}

models::MemberResult MembersComponent::Create(const models::CreateMemberRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        const auto email = userver::utils::text::Trim(request.email);
        const auto last_name = userver::utils::text::Trim(request.last_name);
        const auto first_name = userver::utils::text::Trim(request.first_name);
        auto middle_name = request.middle_name.value_or(std::nullopt);
        if (middle_name.has_value()) {
            middle_name = userver::utils::text::Trim(middle_name.value());
            if (middle_name.value().empty())
                middle_name.reset();
        }
        if (last_name.empty() || first_name.empty() || !platform::IsEmail(email))
            throw ApiError<400>{"invalid_profile", "Укажите фамилию, имя и корректный email"};
        const auto role = request.role;
        userver::formats::json::ValueBuilder intent;
        intent["username"] = request.username;
        intent["email"] = email;
        intent["last_name"] = last_name;
        intent["first_name"] = first_name;
        intent["middle_name"] = middle_name;
        intent["role"] = role;
        const auto payload = intent.ExtractValue();
        {
            auto transaction =
                cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            RequireAdministrator(transaction, request.organization_id, identity);
            RequireRole(transaction, request.organization_id, request.role);
            transaction.Execute(sql::kSaveProvisioning, request.request_id, request.organization_id,
                identity.ActorId(), payload);
            const auto [organization, actor, saved, subject] =
                transaction.Execute(sql::kGetProvisioning, request.request_id)
                    .AsSingleRow<ProvisioningRow>(pg::kRowTag);
            if (organization != request.organization_id || actor != identity.ActorId() ||
                saved != payload)
                throw ApiError<409>{
                    "request_conflict", "Идентификатор запроса уже использован для других данных"};
            if (subject.has_value()) {
                const auto member = Convert(transaction
                                                .Execute(sql::kGetMembership, organization,
                                                    identity.Issuer, subject.value())
                                                .AsSingleRow<pgtypes::MemberV2>());
                transaction.Commit();
                return models::MemberResult{.member = member};
            }
            transaction.Commit();
        }
        const auto user = users_.Create(request.request_id, request.username, email, last_name,
            first_name, middle_name, request.temporary_password);
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.organization_id, identity);
        const auto [organization, actor, saved, subject] =
            transaction.Execute(sql::kGetProvisioning, request.request_id)
                .AsSingleRow<ProvisioningRow>(pg::kRowTag);
        if (subject.has_value()) {
            const auto member = Convert(
                transaction
                    .Execute(sql::kGetMembership, organization, identity.Issuer, subject.value())
                    .AsSingleRow<pgtypes::MemberV2>());
            transaction.Commit();
            return models::MemberResult{.member = member};
        }
        const auto member = Convert(
            transaction.Execute(sql::kAddMember, organization, identity.Issuer, Convert(user, role))
                .AsSingleRow<pgtypes::MemberV2>());
        transaction.Execute(sql::kCompleteProvisioning, request.request_id, user.Subject);
        WriteAudit(transaction, organization, identity, "member.created", member);
        transaction.Commit();
        return models::MemberResult{.member = member};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}

models::MemberResult MembersComponent::Add(const models::AddMemberRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        {
            auto transaction =
                cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
            RequireAdministrator(transaction, request.organization_id, identity);
            RequireRole(transaction, request.organization_id, request.role);
            transaction.Commit();
        }
        // Сетевое обращение выполняется без открытой транзакции и блокировок БД.
        const auto user = users_.Find(request.username);
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.organization_id, identity);
        const auto member = Convert(transaction
                                        .Execute(sql::kAddMember, request.organization_id,
                                            identity.Issuer, Convert(user, request.role))
                                        .AsSingleRow<pgtypes::MemberV2>());
        WriteAudit(transaction, request.organization_id, identity, "member.added", member);
        transaction.Commit();
        return models::MemberResult{.member = member};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}

models::MemberResult MembersComponent::Update(const models::UpdateMemberRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(transaction, request.organization_id, identity);
        const auto rows = transaction.Execute(
            sql::kGetMembership, request.organization_id, identity.Issuer, request.subject);
        if (rows.IsEmpty())
            throw ApiError<404>{"member_not_found", "Сотрудник не найден"};
        const auto previous = rows.AsSingleRow<pgtypes::MemberV2>();
        if (previous.Revision != request.expected_revision)
            throw ApiError<409>{"revision_conflict", "Доступ сотрудника уже изменён"};
        const auto role = request.role;
        const auto previous_role = RequireRole(transaction, request.organization_id, previous.Role);
        const auto next_role = RequireRole(transaction, request.organization_id, role);
        if (previous.Enabled && ManagesOrganization(previous_role.Permissions) &&
            (!request.enabled || !ManagesOrganization(next_role.Permissions)) &&
            transaction.Execute(sql::kCountAdministrators, request.organization_id)
                    .AsSingleRow<std::int64_t>() <= 1)
            throw ApiError<409>{
                "last_administrator", "Нельзя отключить или понизить последнего администратора"};
        const auto member = Convert(transaction
                                        .Execute(sql::kUpdateMember, request.organization_id,
                                            identity.Issuer, request.subject, role, request.enabled)
                                        .AsSingleRow<pgtypes::MemberV2>());
        WriteAudit(transaction, request.organization_id, identity, "member.updated", member);
        transaction.Commit();
        return models::MemberResult{.member = member};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        throw ApiError<409>{"already_exists", "Такая запись уже существует"};
    }
}

models::MemberResult MembersComponent::UpdateProfile(
    const models::UpdateMemberProfileRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);

    {
        auto access = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        RequireAdministrator(access, request.organization_id, identity);
        access.Commit();
    }
    const auto last = userver::utils::text::Trim(request.last_name);
    const auto first = userver::utils::text::Trim(request.first_name);
    auto middle = request.middle_name.value_or(std::nullopt);
    if (middle.has_value()) {
        middle = userver::utils::text::Trim(middle.value());
        if (middle.value().empty())
            middle.reset();
    }
    if (last.empty() || first.empty())
        throw ApiError<400>{"invalid_profile", "Укажите фамилию и имя"};
    const auto existing = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetMembership,
        request.organization_id, identity.Issuer, request.subject);
    if (existing.IsEmpty())
        throw ApiError<404>{"member_not_found", "Сотрудник не найден"};
    // Сетевой вызов выполняется до транзакции БД. Повторное задание ФИО идемпотентно.
    const auto user = users_.UpdateProfile(request.subject, last, first, middle);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kUpdateMemberProfile, identity.Issuer, request.subject,
        user.DisplayName, user.LastName, user.FirstName, user.MiddleName);
    const auto member = Convert(
        transaction
            .Execute(sql::kGetMembership, request.organization_id, identity.Issuer, request.subject)
            .AsSingleRow<pgtypes::MemberV2>());
    WriteAudit(transaction, request.organization_id, identity, "member.profile_updated", member);
    transaction.Commit();
    return models::MemberResult{.member = member};
}
}  // namespace organizations
