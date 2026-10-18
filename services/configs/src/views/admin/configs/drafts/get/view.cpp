#include <components/config_store.hpp>
#include <views/admin/configs/drafts/get/view.hpp>

namespace configs::views::admin::configs::drafts::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto draft = store.GetDraft(request.body.id);
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
                           .status = draft.Status == ::configs::DraftStatus::Draft
                                         ? ::configs::models::DraftStatus::Draft
                                     : draft.Status == ::configs::DraftStatus::Published
                                         ? ::configs::models::DraftStatus::Published
                                         : ::configs::models::DraftStatus::Cancelled,
                           .value = draft.Value,
                           .version = draft.Version};
    } catch (const ::configs::InvalidRequest& error) {
        return Response400{.error = "invalid_request", .message = error.what()};
    } catch (const ::configs::RevisionNotFound& error) {
        return Response404{.error = "not_found", .message = error.what()};
    }
}
}  // namespace configs::views::admin::configs::drafts::get
