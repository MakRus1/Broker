#include <components/config_store.hpp>
#include <platform/auth/principal.hpp>
#include <views/admin/configs/drafts/edit/view.hpp>

namespace configs::views::admin::configs::drafts::edit {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto draft =
            store.EditDraft({.Actor = platform::auth::GetPrincipal(context).ActorId(),
                .Id = request.body.id,
                .ExpectedVersion = request.body.expected_version,
                .Value = request.body.value,
                .Description = request.body.description});
        return Response200{.approved_by = draft.ApprovedBy,
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
    } catch (const ::configs::DraftForbidden& error) {
        return Response403{.error = "forbidden", .message = error.what()};
    } catch (const ::configs::RevisionNotFound& error) {
        return Response404{.error = "not_found", .message = error.what()};
    } catch (const ::configs::RevisionConflict& error) {
        return Response409{.error = "conflict", .message = error.what()};
    }
}
}  // namespace configs::views::admin::configs::drafts::edit
