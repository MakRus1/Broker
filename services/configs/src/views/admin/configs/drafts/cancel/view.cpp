#include <components/config_store.hpp>
#include <views/admin/configs/drafts/cancel/view.hpp>

namespace configs::views::admin::configs::drafts::cancel {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto draft = store.CancelDraft({.Actor = request.body.actor,
                                              .Id = request.body.id,
                                              .ExpectedVersion = request.body.expected_version});
        return Response200{.approved_by = draft.ApprovedBy,
                           .author = draft.Author,
                           .base_value = draft.BaseValue,
                           .base_version = draft.BaseVersion,
                           .config_name = draft.ConfigName,
                           .description = draft.Description,
                           .id = draft.Id,
                           .published_revision = draft.PublishedRevision,
                           .source_revision = draft.SourceRevision,
                           .status = status::Cancelled,
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
}  // namespace configs::views::admin::configs::drafts::cancel
