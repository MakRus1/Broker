#include <components/inquiries.hpp>
#include <components/rfq.hpp>
#include <components/rfq_rules.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/proposals/suggest/view.hpp>

namespace sales::views::api::v1::sales::proposals::suggest {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto inquiry = deps.inquiries->FindInquiry(organization, request.body.inquiry_id);
    const auto items = deps.rfq->ListRfqs(organization, inquiry.id);
    switch (request.body.strategy) {
    case models::ProposalSuggestionStrategy::MinimumPurchase: {
        auto result =
            rfq_rules::SuggestMinimumPurchase(inquiry, items, request.body.calculation_date);
        return Response200{.gaps = std::move(result.gaps),
            .items = std::move(result.items),
            .selected = std::move(result.selected)};
    }
    }
    throw ApiError<400>{"unsupported_strategy", "Стратегия подбора не поддерживается"};
}
}  // namespace sales::views::api::v1::sales::proposals::suggest
