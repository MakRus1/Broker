#include <components/config_store.hpp>
#include <platform/auth/principal.hpp>
#include <views/admin/configs/drafts/create/view.hpp>

namespace configs::views::admin::configs::drafts::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto draft =
            store.CreateDraft({.Actor = platform::auth::GetPrincipal(context).ActorId(),
                .ConfigName = request.body.config_name,
                .Description = request.body.description,
                .ExpectedConfigVersion = request.body.expected_config_version,
                .Value = request.body.value,
                .SourceRevision = request.body.source_revision,
                .ActorName = platform::auth::GetPrincipal(context).DisplayName});
        return Response201{.approved_by = draft.ApprovedBy,
            .approved_by_name = draft.ApprovedByName,
            .author = draft.Author,
            .author_name = draft.AuthorName,
            .base_value = draft.BaseValue,
            .base_version = draft.BaseVersion,
            .config_name = draft.ConfigName,
            .description = draft.Description,
            .id = draft.Id,
            .published_revision = draft.PublishedRevision,
            .source_revision = draft.SourceRevision,
            .status = ::configs::models::DraftStatus::Draft,
            .value = draft.Value,
            .version = draft.Version};
    } catch (const ::configs::InvalidRequest& error) {
        return Response400{.error = "invalid_request", .message = error.what()};
    } catch (const ::configs::RevisionNotFound& error) {
        return Response404{.error = "not_found", .message = error.what()};
    } catch (const ::configs::RevisionConflict& error) {
        return Response409{.error = "conflict", .message = error.what()};
    }
}
}  // namespace configs::views::admin::configs::drafts::create
