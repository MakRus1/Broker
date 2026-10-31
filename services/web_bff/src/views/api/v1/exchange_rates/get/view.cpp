#include <platform/browser/session.hpp>
#include <views/api/v1/exchange_rates/get/view.hpp>

namespace web_bff::views::api::v1::exchange_rates::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result = deps.finance.ApiV1ExchangeRatesGet({.body = {.date = body.date,
                                                                .from_currency = body.from_currency,
                                                                .to_currency = body.to_currency}},
        session.Access);
    return Response200{.date = result.date,
        .rate_millionths = result.rate_millionths,
        .source =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(result.source),
                models::finance::ExchangeRate_source{})};
}
}  // namespace web_bff::views::api::v1::exchange_rates::get
