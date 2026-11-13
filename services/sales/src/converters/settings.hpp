#pragma once
#include <models/errors.hpp>
#include <models/pg/sales_settings.hpp>
#include <openapi/models.hpp>
#include <sales/sql_queries.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace sales::converters {
namespace pg = userver::storages::postgres;

inline models::SalesSettings ToSalesSettings(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        return {.revision = 0, .stages = {}};
    const auto settings = rows.AsSingleRow<models::pg::SalesSettings>();
    models::SalesSettings result{.inquiry_number_template = settings.InquiryNumberTemplate,
        .reset_number_yearly = settings.ResetNumberYearly,
        .revision = settings.Revision,
        .stages = {}};
    result.stages.reserve(settings.Stages.GetSize());
    for (const auto& stage : settings.Stages)
        result.stages.push_back(
            models::ParseModelJson(stage, std::type_identity<models::SalesStage>{}));
    return result;
}

}  // namespace sales::converters
