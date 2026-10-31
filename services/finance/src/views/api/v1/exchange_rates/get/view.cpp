#include <components/exchange_rates.hpp>
#include <views/api/v1/exchange_rates/get/view.hpp>

namespace finance::views::api::v1::exchange_rates::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto& body = request.body;
    const auto result = deps.exchange_rates.Get(body.from_currency, body.to_currency, body.date);
    return Response200{.date = result.Date,
        .rate_millionths = result.Millionths,
        .source = models::ParseEnumScalar(result.Source, models::ExchangeRate_source{})};
}
}  // namespace finance::views::api::v1::exchange_rates::get
