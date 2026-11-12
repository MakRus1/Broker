#pragma once
#include <openapi/models.hpp>

namespace sales::converters {
inline models::ProposalInput ToProposalInput(const models::SaveProposalRequest& request) {
    return {.calculation_date = request.calculation_date,
        .delivery_days = request.delivery_days,
        .id = request.id,
        .inquiry_id = request.inquiry_id,
        .lines = request.lines,
        .note = request.note,
        .title = request.title,
        .valid_until = request.valid_until};
}

inline models::RfqInput ToRfqInput(const models::SaveRfqRequest& request) {
    return {.answer_by = request.answer_by,
        .id = request.id,
        .inquiry_id = request.inquiry_id,
        .note = request.note,
        .quotes = request.quotes,
        .responsible_subject = request.responsible_subject,
        .route_segment_id = request.route_segment_id,
        .status = request.status,
        .title = request.title};
}
}  // namespace sales::converters
