#pragma once
#include <chrono>
#include <platform/date_validation.hpp>
#include <platform/handler_error.hpp>

namespace platform {
template <typename Segments> void ValidateRoutePlans(const Segments& segments) {
    for (const auto& segment : segments) {
        if (!segment.plan)
            continue;
        if (segment.type == "sea" || segment.type == "air")
            throw http::HandlerError<400>{
                "unexpected_route_plan", "Для моря и авиа заполните профильные поля участка"};
        const auto& plan = *segment.plan;
        for (const auto& date : {plan.departure_date, plan.arrival_date}) {
            if (date &&
                !IsCalendarDateInRange(*date, std::chrono::year{1900} / std::chrono::January / 1,
                    std::chrono::year{9999} / std::chrono::December / 31))
                throw http::HandlerError<400>{
                    "invalid_route_date", "Укажите плановую дату не раньше 01.01.1900"};
        }
        if (plan.departure_date && plan.arrival_date && plan.departure_date > plan.arrival_date)
            throw http::HandlerError<400>{
                "invalid_route_dates", "Плановое отправление не может быть позже прибытия"};
    }
}
}  // namespace platform
